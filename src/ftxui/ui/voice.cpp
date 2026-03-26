// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/voice.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/llm_bridge.hpp"

using namespace ftxui;

namespace ftxui::ui {

namespace {

// ── String helpers
// ────────────────────────────────────────────────────────────

std::string ToLower(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

std::string Trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\n\r\f\v");
  if (start == std::string::npos) {
    return {};
  }
  size_t end = s.find_last_not_of(" \t\n\r\f\v");
  return s.substr(start, end - start + 1);
}

bool ContainsWord(const std::string& text, const std::string& word) {
  std::string lower = ToLower(text);
  std::string lword = ToLower(word);
  size_t pos = lower.find(lword);
  if (pos == std::string::npos) {
    return false;
  }
  // Check word boundaries
  bool start_ok =
      (pos == 0) || !std::isalpha(static_cast<unsigned char>(lower[pos - 1]));
  bool end_ok =
      (pos + lword.size() >= lower.size()) ||
      !std::isalpha(static_cast<unsigned char>(lower[pos + lword.size()]));
  return start_ok && end_ok;
}

/// Generate a timestamped temp filename relative to the current directory.
std::string MakeTempWavPath() {
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return "badwolf_voice_" + std::to_string(now) + ".wav";
}

/// Convert IntentType to a lowercase string for handler dispatch.
std::string IntentTypeToString(IntentType t) {
  switch (t) {
    case IntentType::TABLE:
      return "table";
    case IntentType::CHART:
      return "chart";
    case IntentType::FORM:
      return "form";
    case IntentType::DASHBOARD:
      return "dashboard";
    case IntentType::MAP:
      return "map";
    case IntentType::LOG:
      return "log";
    case IntentType::PROGRESS:
      return "progress";
    default:
      return "unknown";
  }
}

/// Run a shell command and capture its output.
std::string RunCommand(const std::string& cmd) {
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return {};
  }
  std::string result;
  char buf[256];
  while (fgets(buf, sizeof(buf), pipe)) {
    result += buf;
  }
  pclose(pipe);
  return result;
}

/// Strip whisper artifact lines like "[BLANK_AUDIO]", "[Music]", etc.
std::string CleanWhisperOutput(const std::string& raw) {
  std::istringstream stream(raw);
  std::string line;
  std::string out;
  while (std::getline(stream, line)) {
    std::string t = Trim(line);
    if (t.empty()) {
      continue;
    }
    // Skip artifact tags
    if (t.front() == '[' && t.back() == ']') {
      continue;
    }
    if (t.front() == '(' && t.back() == ')') {
      continue;
    }
    if (!out.empty()) {
      out += ' ';
    }
    out += t;
  }
  return Trim(out);
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════════
//  AudioCapture
// ═══════════════════════════════════════════════════════════════════════════════

AudioCapture::AudioCapture(VoiceConfig cfg) : cfg_(std::move(cfg)) {}

AudioCapture::~AudioCapture() {
  StopRecording();
}

std::string AudioCapture::RecordToFile(std::chrono::milliseconds max_duration) {
  std::string path = MakeTempWavPath();
  if (!StartRecording(path)) {
    return {};
  }
  std::this_thread::sleep_for(max_duration);
  StopRecording();
  return recording_path_;
}

bool AudioCapture::StartRecording(const std::string& output_path) {
  if (recording_.load()) {
    return false;
  }
  recording_path_ = output_path.empty() ? MakeTempWavPath() : output_path;
  recording_.store(true);
  rms_level_.store(0.0f);

  std::string path = recording_path_;
  std::chrono::milliseconds max_dur = cfg_.max_record_duration;
  capture_thread_ =
      std::thread([this, path, max_dur]() { CaptureLoop(path, max_dur); });
  return true;
}

void AudioCapture::StopRecording() {
  recording_.store(false);
  if (capture_thread_.joinable()) {
    capture_thread_.join();
  }
  rms_level_.store(0.0f);
}

bool AudioCapture::IsRecording() const {
  return recording_.load();
}

std::string AudioCapture::RecordingPath() const {
  return recording_path_;
}

float AudioCapture::CurrentLevel() const {
  return rms_level_.load();
}

// static
bool AudioCapture::MicrophoneAvailable() {
#if defined(__APPLE__)
  std::string out =
      RunCommand("ffmpeg -f avfoundation -list_devices true -i \"\" 2>&1");
  return out.find("AVFoundation") != std::string::npos ||
         out.find("audio") != std::string::npos;
#else
  // Linux: check if ffmpeg can open the default ALSA device briefly
  int rc =
      system("ffmpeg -f alsa -i default -t 0.01 -f null - >/dev/null 2>&1");
  return rc == 0;
#endif
}

void AudioCapture::CaptureLoop(const std::string& path,
                               std::chrono::milliseconds max_duration) {
#if defined(__APPLE__)
  std::string cmd =
      "ffmpeg -y -f avfoundation -i \":0\""
      " -ar " +
      std::to_string(cfg_.sample_rate) + " -ac " +
      std::to_string(cfg_.channels) + " -f wav \"" + path + "\" 2>/dev/null";
#else
  std::string cmd =
      "ffmpeg -y -f alsa -i default"
      " -ar " +
      std::to_string(cfg_.sample_rate) + " -ac " +
      std::to_string(cfg_.channels) + " -f wav \"" + path + "\" 2>/dev/null";
#endif

  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    recording_.store(false);
    return;
  }

  auto start = std::chrono::steady_clock::now();
  while (recording_.load()) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed >= max_duration) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  pclose(pipe);
  recording_.store(false);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  WhisperTranscriber
// ═══════════════════════════════════════════════════════════════════════════════

WhisperTranscriber::WhisperTranscriber(VoiceConfig cfg)
    : cfg_(std::move(cfg)) {}

TranscriptionResult WhisperTranscriber::Transcribe(
    const std::string& wav_path) {
  if (wav_path.empty()) {
    return {.success = false, .error = "empty wav path"};
  }

  // Build whisper command: -nt = no timestamps, -of txt = text output
  std::string cmd = "\"" + cfg_.whisper_path + "\" \"" + wav_path +
                    "\""
                    " -m \"" +
                    cfg_.model_path +
                    "\""
                    " -l " +
                    cfg_.language + " -nt -of txt 2>/dev/null";

  auto t_start = std::chrono::steady_clock::now();
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return {.success = false, .error = "failed to run whisper"};
  }

  std::string raw;
  char buf[512];
  while (fgets(buf, sizeof(buf), pipe)) {
    raw += buf;
  }
  int rc = pclose(pipe);

  double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start)
          .count();

  if (rc != 0 && raw.empty()) {
    return {.success = false,
            .error = "whisper exited with code " + std::to_string(rc)};
  }

  std::string text = CleanWhisperOutput(raw);
  if (text.empty()) {
    return {.success = false, .error = "empty transcription"};
  }

  return {.text = text,
          .confidence = 0.9f,
          .duration_seconds = elapsed,
          .success = true};
}

void WhisperTranscriber::TranscribeAsync(
    const std::string& wav_path,
    std::function<void(TranscriptionResult)> callback) {
  std::string path = wav_path;
  VoiceConfig cfg = cfg_;
  std::thread([path, cfg, callback]() {
    WhisperTranscriber t(cfg);
    callback(t.Transcribe(path));
  }).detach();
}

// static
bool WhisperTranscriber::WhisperAvailable(const std::string& whisper_path) {
  // Try --help
  std::string cmd = "\"" + whisper_path + "\" --help >/dev/null 2>&1";
  if (system(cmd.c_str()) == 0) {
    return true;
  }
  // Check common installation paths
  const char* paths[] = {"/usr/local/bin/whisper", "/usr/bin/whisper", nullptr};
  for (int i = 0; paths[i]; ++i) {
    std::string check =
        std::string("\"") + paths[i] + "\" --help >/dev/null 2>&1";
    if (system(check.c_str()) == 0) {
      return true;
    }
  }
  return false;
}

// static
std::string WhisperTranscriber::WhisperVersion(
    const std::string& whisper_path) {
  std::string cmd = "\"" + whisper_path + "\" --version 2>&1";
  std::string out = RunCommand(cmd);
  return Trim(out);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  VoiceCommandEngine
// ═══════════════════════════════════════════════════════════════════════════════

VoiceCommandEngine::VoiceCommandEngine(VoiceConfig cfg)
    : cfg_(cfg), audio_(cfg), transcriber_(cfg) {
  continuous_.store(cfg.continuous);
}

VoiceCommandEngine::~VoiceCommandEngine() {
  StopListening();
}

void VoiceCommandEngine::OnCommand(
    const std::string& intent,
    std::function<void(const std::string&)> handler) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  handlers_[ToLower(intent)] = std::move(handler);
}

void VoiceCommandEngine::OnTranscription(
    std::function<void(const std::string&)> handler) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  transcription_handler_ = std::move(handler);
}

void VoiceCommandEngine::StartListening() {
  if (IsListening()) {
    return;
  }
  stop_requested_.store(false);
  status_.store(VoiceStatus::Listening);
  listen_thread_ = std::thread([this]() { DoListen(); });
}

void VoiceCommandEngine::StopListening() {
  stop_requested_.store(true);
  audio_.StopRecording();
  if (listen_thread_.joinable()) {
    listen_thread_.join();
  }
  status_.store(VoiceStatus::Idle);
}

bool VoiceCommandEngine::IsListening() const {
  auto s = status_.load();
  return s == VoiceStatus::Listening || s == VoiceStatus::Processing;
}

VoiceStatus VoiceCommandEngine::status() const {
  return status_.load();
}

std::string VoiceCommandEngine::last_transcription() const {
  std::lock_guard<std::mutex> lock(state_mutex_);
  return last_transcription_;
}

std::string VoiceCommandEngine::last_error() const {
  std::lock_guard<std::mutex> lock(state_mutex_);
  return last_error_;
}

void VoiceCommandEngine::ListenOnce(
    std::function<void(const std::string&)> cb) {
  if (IsListening()) {
    return;
  }
  stop_requested_.store(false);
  status_.store(VoiceStatus::Listening);
  listen_thread_ = std::thread([this, cb]() {
    // Record
    std::string path = audio_.RecordToFile(cfg_.max_record_duration);
    if (stop_requested_.load() || path.empty()) {
      status_.store(VoiceStatus::Idle);
      return;
    }
    // Transcribe
    status_.store(VoiceStatus::Processing);
    auto result = transcriber_.Transcribe(path);
    std::remove(path.c_str());

    if (!result.success) {
      std::lock_guard<std::mutex> lock(state_mutex_);
      last_error_ = result.error;
      status_.store(VoiceStatus::Error);
      return;
    }

    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      last_transcription_ = result.text;
    }

    if (cfg_.echo_transcription) {
      // No-op for library code; demo can handle echo via OnTranscription
    }

    Dispatch(result.text);
    if (cb) {
      cb(result.text);
    }
    status_.store(VoiceStatus::Ready);
  });
}

void VoiceCommandEngine::DoListen() {
  do {
    if (stop_requested_.load()) {
      break;
    }

    // Record
    status_.store(VoiceStatus::Listening);
    std::string path = audio_.RecordToFile(cfg_.max_record_duration);
    if (stop_requested_.load() || path.empty()) {
      break;
    }

    // Transcribe
    status_.store(VoiceStatus::Processing);
    auto result = transcriber_.Transcribe(path);
    std::remove(path.c_str());

    if (!result.success) {
      std::lock_guard<std::mutex> lock(state_mutex_);
      last_error_ = result.error;
      status_.store(VoiceStatus::Error);
      if (!continuous_.load()) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      last_transcription_ = result.text;
    }

    Dispatch(result.text);
    status_.store(VoiceStatus::Ready);

  } while (continuous_.load() && !stop_requested_.load());

  if (!stop_requested_.load()) {
    status_.store(VoiceStatus::Idle);
  }
}

void VoiceCommandEngine::Dispatch(const std::string& text) {
  // Notify raw transcription handler
  std::function<void(const std::string&)> tx_handler;
  std::map<std::string, std::function<void(const std::string&)>> handlers_copy;
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    tx_handler = transcription_handler_;
    handlers_copy = handlers_;
  }

  if (tx_handler) {
    tx_handler(text);
  }

  if (handlers_copy.empty()) {
    return;
  }

  // Parse via NLParser
  UIIntent intent = NLParser::Parse(text);
  std::string intent_key = IntentTypeToString(intent.type);

  // Try exact intent type match
  auto it = handlers_copy.find(intent_key);
  if (it != handlers_copy.end()) {
    it->second(intent.entity);
    return;
  }

  // Try keyword matching against the transcription text
  std::string lower_text = ToLower(text);
  for (auto& [key, handler] : handlers_copy) {
    if (ContainsWord(lower_text, key)) {
      handler(intent.entity.empty() ? text : intent.entity);
      return;
    }
  }

  // Fall back to "unknown" handler if registered
  auto unk = handlers_copy.find("unknown");
  if (unk != handlers_copy.end()) {
    unk->second(text);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  VoiceStatusIndicator
// ═══════════════════════════════════════════════════════════════════════════════

Element VoiceStatusIndicator(VoiceStatus status, float level) {
  switch (status) {
    case VoiceStatus::Idle:
      return text("🎙") | color(Color::GrayDark);

    case VoiceStatus::Listening: {
      // Animate: alternate between bright red and dim red based on level
      float l = std::max(0.0f, std::min(1.0f, level));
      (void)l;
      return text("🔴") | color(Color::Red) | bold;
    }

    case VoiceStatus::Processing:
      return text("⟳") | color(Color::Yellow);

    case VoiceStatus::Ready:
      return text("✓") | color(Color::Green);

    case VoiceStatus::Error:
      return text("✗") | color(Color::Red);

    case VoiceStatus::Unavailable:
      return text("○") | dim;
  }
  return text("?");
}

// ═══════════════════════════════════════════════════════════════════════════════
//  VoiceInputBar
// ═══════════════════════════════════════════════════════════════════════════════

namespace {

/// VU meter string — braille-style bars scaled to [0,1]
Element VuMeter(float level, int width = 8) {
  // Use block characters for VU meter
  const std::vector<std::string> blocks = {"▁", "▂", "▃", "▄",
                                           "▅", "▆", "▇", "█"};
  int filled = static_cast<int>(level * width);
  std::string meter;
  for (int i = 0; i < width; ++i) {
    if (i < filled) {
      int idx = std::min(static_cast<int>(level * 8), 7);
      meter += blocks[idx];
    } else {
      meter += "░";
    }
  }
  Color c = level > 0.7f   ? Color::Red
            : level > 0.3f ? Color::Yellow
                           : Color::Green;
  return text(meter) | color(c);
}

class VoiceInputBarComponent : public ComponentBase {
 public:
  explicit VoiceInputBarComponent(std::shared_ptr<VoiceCommandEngine> engine,
                                  VoiceConfig cfg)
      : engine_(engine ? engine : std::make_shared<VoiceCommandEngine>(cfg)),
        cfg_(std::move(cfg)) {}

  Element OnRender() override {
    VoiceStatus st = engine_->status();
    float level = 0.0f;  // level from audio capture if available
    std::string last = engine_->last_transcription();

    // Mic button
    std::string mic_label;
    Color mic_color;
    switch (st) {
      case VoiceStatus::Listening:
        mic_label = "[🔴 Stop ]";
        mic_color = Color::Red;
        break;
      case VoiceStatus::Processing:
        mic_label = "[⟳ Wait  ]";
        mic_color = Color::Yellow;
        break;
      case VoiceStatus::Ready:
        mic_label = "[✓ Done  ]";
        mic_color = Color::Green;
        break;
      case VoiceStatus::Error:
        mic_label = "[✗ Error ]";
        mic_color = Color::Red;
        break;
      case VoiceStatus::Unavailable:
        mic_label = "[○ N/A   ]";
        mic_color = Color::GrayDark;
        break;
      default:
        mic_label = "[🎙 Listen]";
        mic_color = Color::GrayLight;
        break;
    }

    Element btn = text(mic_label) | color(mic_color) | bold;
    if (Focused()) {
      btn = btn | inverted;
    }

    // VU meter
    Element vu = VuMeter(level, 10);

    // Status text
    std::string status_str;
    switch (st) {
      case VoiceStatus::Idle:
        status_str = "idle";
        break;
      case VoiceStatus::Listening:
        status_str = "listening…";
        break;
      case VoiceStatus::Processing:
        status_str = "processing…";
        break;
      case VoiceStatus::Ready:
        status_str = "ready";
        break;
      case VoiceStatus::Error:
        status_str = "error: " + engine_->last_error();
        break;
      case VoiceStatus::Unavailable:
        status_str = "unavailable";
        break;
    }

    // Transcription text (truncated)
    std::string display_text = last.empty() ? "(no transcription yet)" : last;
    if (display_text.size() > 40) {
      display_text = display_text.substr(0, 37) + "…";
    }

    return hbox({
               text(" "),
               btn,
               text(" "),
               vu,
               text(" "),
               text(status_str) | color(Color::GrayLight),
               text("  "),
               text(display_text) | color(Color::Cyan) | flex,
               text(" "),
           }) |
           border;
  }

  bool OnEvent(Event event) override {
    if (event == Event::Return || event == Event::Character(' ')) {
      ToggleListening();
      return true;
    }
    return false;
  }

  bool Focusable() const override { return true; }

 private:
  std::shared_ptr<VoiceCommandEngine> engine_;
  VoiceConfig cfg_;

  void ToggleListening() {
    if (engine_->IsListening()) {
      engine_->StopListening();
    } else {
      engine_->StartListening();
    }
  }
};

}  // namespace

ftxui::Component VoiceInputBar(std::shared_ptr<VoiceCommandEngine> engine,
                               VoiceConfig cfg) {
  return Make<VoiceInputBarComponent>(std::move(engine), std::move(cfg));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  WithVoiceControl
// ═══════════════════════════════════════════════════════════════════════════════

ftxui::Component WithVoiceControl(ftxui::Component app,
                                  std::shared_ptr<VoiceCommandEngine> engine,
                                  VoiceConfig cfg) {
  auto eng = engine ? engine : std::make_shared<VoiceCommandEngine>(cfg);

  // Register standard BadWolf actions
  eng->OnCommand("navigate", [](const std::string&) {});
  eng->OnCommand("select", [](const std::string&) {});
  eng->OnCommand("submit", [](const std::string&) {});
  eng->OnCommand("open", [](const std::string&) {});
  eng->OnCommand("close", [](const std::string&) {});

  class WithVoiceControlComponent : public ComponentBase {
   public:
    WithVoiceControlComponent(ftxui::Component inner,
                              std::shared_ptr<VoiceCommandEngine> eng,
                              VoiceConfig cfg)
        : inner_(std::move(inner)),
          engine_(std::move(eng)),
          bar_(VoiceInputBar(engine_, cfg)),
          show_bar_(false) {
      Add(inner_);
      Add(bar_);
    }

    Element OnRender() override {
      if (show_bar_) {
        return vbox({
            inner_->Render() | flex,
            bar_->Render(),
        });
      }
      return inner_->Render();
    }

    bool OnEvent(Event event) override {
      // V key toggles voice control
      if (event == Event::Character('v') || event == Event::Character('V')) {
        show_bar_ = !show_bar_;
        if (engine_->IsListening()) {
          engine_->StopListening();
        } else if (show_bar_) {
          engine_->StartListening();
        }
        return true;
      }
      if (show_bar_ && bar_->OnEvent(event)) {
        return true;
      }
      return inner_->OnEvent(event);
    }

    bool Focusable() const override { return true; }

   private:
    ftxui::Component inner_;
    std::shared_ptr<VoiceCommandEngine> engine_;
    ftxui::Component bar_;
    bool show_bar_;
  };

  return Make<WithVoiceControlComponent>(std::move(app), std::move(eng),
                                         std::move(cfg));
}

}  // namespace ftxui::ui
