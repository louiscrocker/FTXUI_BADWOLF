// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_VOICE_HPP
#define FTXUI_UI_VOICE_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

// ── VoiceConfig
// ───────────────────────────────────────────────────────────────
struct VoiceConfig {
  std::string whisper_path = "whisper";      ///< path to whisper CLI binary
  std::string model_path = "ggml-base.bin";  ///< whisper model file
  std::string language = "en";
  int sample_rate = 16000;  ///< Hz
  int channels = 1;
  float silence_threshold = 0.01f;  ///< RMS threshold to detect silence
  std::chrono::milliseconds min_speech_duration{500};    ///< min to transcribe
  std::chrono::milliseconds max_record_duration{10000};  ///< auto-stop after
  std::chrono::milliseconds silence_timeout{1500};       ///< stop after silence
  bool continuous = false;         ///< keep listening after each command
  bool echo_transcription = true;  ///< print what was heard
};

// ── VoiceStatus
// ───────────────────────────────────────────────────────────────
enum class VoiceStatus {
  Idle,         ///< not listening
  Listening,    ///< recording microphone
  Processing,   ///< running whisper
  Ready,        ///< transcription complete
  Error,        ///< something went wrong
  Unavailable,  ///< whisper/mic not available
};

// ── TranscriptionResult
// ───────────────────────────────────────────────────────
struct TranscriptionResult {
  std::string text;
  float confidence = 0.0f;  ///< 0-1
  double duration_seconds = 0.0;
  bool success = false;
  std::string error;
};

// ── AudioCapture
// ──────────────────────────────────────────────────────────────
/// @brief Captures audio from microphone to a WAV file via ffmpeg.
/// @ingroup ui
class AudioCapture {
 public:
  explicit AudioCapture(VoiceConfig cfg = {});
  ~AudioCapture();

  /// Record for up to max_duration, saves to a WAV file, returns path.
  std::string RecordToFile(
      std::chrono::milliseconds max_duration = std::chrono::seconds(5));

  /// Non-blocking: start recording in the background.
  bool StartRecording(const std::string& output_path = "");
  void StopRecording();
  bool IsRecording() const;
  std::string RecordingPath() const;

  /// Returns true if a microphone is accessible via ffmpeg.
  static bool MicrophoneAvailable();

  /// RMS level [0,1] for VU meter display.
  float CurrentLevel() const;

 private:
  VoiceConfig cfg_;
  std::string recording_path_;
  std::atomic<bool> recording_{false};
  std::thread capture_thread_;
  std::atomic<float> rms_level_{0.0f};

  void CaptureLoop(const std::string& path,
                   std::chrono::milliseconds max_duration);
};

// ── WhisperTranscriber
// ────────────────────────────────────────────────────────
/// @brief Runs the whisper CLI on a WAV file and returns the transcription.
/// @ingroup ui
class WhisperTranscriber {
 public:
  explicit WhisperTranscriber(VoiceConfig cfg = {});

  /// Synchronous transcription — blocks until complete.
  TranscriptionResult Transcribe(const std::string& wav_path);

  /// Async transcription — calls callback on completion.
  void TranscribeAsync(const std::string& wav_path,
                       std::function<void(TranscriptionResult)> callback);

  /// Returns true if the whisper CLI is found and responds to --help.
  static bool WhisperAvailable(const std::string& whisper_path = "whisper");

  /// Returns the whisper CLI version string, or "" if unavailable.
  static std::string WhisperVersion(
      const std::string& whisper_path = "whisper");

 private:
  VoiceConfig cfg_;
};

// ── VoiceCommandEngine
// ────────────────────────────────────────────────────────
/// @brief Ties together AudioCapture + WhisperTranscriber + NLParser.
///
/// Register handlers by intent name (e.g. "table", "chart", "unknown") or
/// arbitrary keyword strings.  The engine records one utterance, transcribes
/// it, parses the intent, and calls the matching handler.
///
/// @ingroup ui
class VoiceCommandEngine {
 public:
  explicit VoiceCommandEngine(VoiceConfig cfg = {});
  ~VoiceCommandEngine();

  /// Register a handler for a specific intent key.
  /// Intent keys match against the IntentType string ("table", "chart",
  /// "form", "dashboard", "map", "log", "progress", "unknown") or any
  /// custom keyword found in the transcription.
  void OnCommand(const std::string& intent,
                 std::function<void(const std::string& entity)> handler);

  /// Called with every successful transcription text.
  void OnTranscription(std::function<void(const std::string& text)> handler);

  /// Start listening (continuous or single-shot based on VoiceConfig).
  void StartListening();
  void StopListening();
  bool IsListening() const;

  VoiceStatus status() const;
  std::string last_transcription() const;
  std::string last_error() const;

  /// Record one command, transcribe, dispatch handlers, then stop.
  void ListenOnce(
      std::function<void(const std::string& transcription)> cb = {});

 private:
  VoiceConfig cfg_;
  AudioCapture audio_;
  WhisperTranscriber transcriber_;
  std::map<std::string, std::function<void(const std::string&)>> handlers_;
  std::function<void(const std::string&)> transcription_handler_;
  std::atomic<VoiceStatus> status_{VoiceStatus::Idle};
  std::string last_transcription_;
  std::string last_error_;
  std::thread listen_thread_;
  std::atomic<bool> continuous_{false};
  std::atomic<bool> stop_requested_{false};
  mutable std::mutex state_mutex_;

  void DoListen();
  void Dispatch(const std::string& text);
};

// ── VoiceInputBar
// ─────────────────────────────────────────────────────────────
/// @brief UI bar: microphone button + VU meter + transcription display.
///
/// Click the 🎙 button (or press Space when focused) to start/stop recording.
/// @ingroup ui
ftxui::Component VoiceInputBar(
    std::shared_ptr<VoiceCommandEngine> engine = nullptr,
    VoiceConfig cfg = {});

// ── WithVoiceControl
// ──────────────────────────────────────────────────────────
/// @brief Wrap an app with voice control — press V to record, V again to stop.
///
/// Transcription is fed through NLParser and dispatched to standard BadWolf
/// actions: navigate, select, submit, open, close, etc.
/// @ingroup ui
ftxui::Component WithVoiceControl(
    ftxui::Component app,
    std::shared_ptr<VoiceCommandEngine> engine = nullptr,
    VoiceConfig cfg = {});

// ── VoiceStatusIndicator ─────────────────────────────────────────────────────
/// @brief Compact status icon element — 🎙 gray, 🔴 pulsing, ⟳ yellow, etc.
/// @ingroup ui
ftxui::Element VoiceStatusIndicator(VoiceStatus status, float level = 0.0f);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_VOICE_HPP
