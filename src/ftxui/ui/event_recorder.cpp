// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/event_recorder.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/mouse.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ---------------------------------------------------------------------------
// Helpers: event → text serialization
// ---------------------------------------------------------------------------

namespace {

// Encode raw bytes as hex string.
std::string ToHex(const std::string& s) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (unsigned char c : s) {
    oss << std::setw(2) << static_cast<int>(c);
  }
  return oss.str();
}

// Decode hex string back to bytes.
std::string FromHex(const std::string& hex) {
  std::string result;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    unsigned char byte =
        static_cast<unsigned char>(std::stoi(hex.substr(i, 2), nullptr, 16));
    result.push_back(static_cast<char>(byte));
  }
  return result;
}

// Produce a short human-readable description of an event.
std::string Describe(const ftxui::Event& e) {
  if (e.is_character()) {
    return "Char('" + e.character() + "')";
  }
  if (e.is_mouse()) {
    char buf[64];
    auto mutable_e = e;
    std::snprintf(buf, sizeof(buf), "Mouse(%d,%d)", mutable_e.mouse().x, mutable_e.mouse().y);
    return buf;
  }
  std::string raw = ToHex(e.input());
  if (raw.size() > 12) {
    raw = raw.substr(0, 12) + "…";
  }
  return "Special(" + raw + ")";
}

// Serialize an event to a single line.
// Format:
//   CHAR <hex_utf8_data>
//   MOUSE <button> <motion> <shift> <meta> <ctrl> <x> <y> <hex_input>
//   SPECIAL <hex_input>
std::string SerializeEvent(const ftxui::Event& e) {
  if (e.is_character()) {
    return "CHAR " + ToHex(e.character());
  }
  if (e.is_mouse()) {
    auto mutable_e = e;
    auto m = mutable_e.mouse();
    std::ostringstream oss;
    oss << "MOUSE " << static_cast<int>(m.button) << " "
        << static_cast<int>(m.motion) << " " << (m.shift ? 1 : 0) << " "
        << (m.meta ? 1 : 0) << " " << (m.control ? 1 : 0) << " " << m.x
        << " " << m.y << " " << ToHex(e.input());
    return oss.str();
  }
  return "SPECIAL " + ToHex(e.input());
}

// Deserialize one event from a serialized line.
// Returns true on success.
bool DeserializeEvent(const std::string& line, ftxui::Event& out) {
  if (line.empty()) {
    return false;
  }

  std::istringstream iss(line);
  std::string type;
  iss >> type;

  if (type == "CHAR") {
    std::string hex;
    iss >> hex;
    out = ftxui::Event::Character(FromHex(hex));
    return true;
  }

  if (type == "MOUSE") {
    int button, motion, shift, meta, ctrl, x, y;
    std::string hex;
    iss >> button >> motion >> shift >> meta >> ctrl >> x >> y >> hex;
    ftxui::Mouse m;
    m.button = static_cast<ftxui::Mouse::Button>(button);
    m.motion = static_cast<ftxui::Mouse::Motion>(motion);
    m.shift = (shift != 0);
    m.meta = (meta != 0);
    m.control = (ctrl != 0);
    m.x = x;
    m.y = y;
    out = ftxui::Event::Mouse(FromHex(hex), m);
    return true;
  }

  if (type == "SPECIAL") {
    std::string hex;
    iss >> hex;
    out = ftxui::Event::Special(FromHex(hex));
    return true;
  }

  return false;
}

// ---------------------------------------------------------------------------
// Recording component wrapper
// ---------------------------------------------------------------------------

class RecordingComponent : public ComponentBase {
 public:
  explicit RecordingComponent(Component inner) : inner_(std::move(inner)) {
    Add(inner_);
  }

  Element OnRender() override { return inner_->Render(); }

  bool OnEvent(Event event) override {
    EventRecorder::Instance().RecordEvent(event);
    return inner_->OnEvent(event);
  }

  Component ActiveChild() override { return inner_; }

 private:
  Component inner_;
};

}  // namespace

// ---------------------------------------------------------------------------
// EventRecorder
// ---------------------------------------------------------------------------

EventRecorder& EventRecorder::Instance() {
  static EventRecorder instance;
  return instance;
}

EventRecorder::~EventRecorder() {
  StopReplay();
}

void EventRecorder::StartRecording() {
  events_.clear();
  recording_ = true;
}

void EventRecorder::StopRecording() {
  recording_ = false;
}

bool EventRecorder::IsRecording() const {
  return recording_;
}

void EventRecorder::RecordEvent(const ftxui::Event& event) {
  if (!recording_) {
    return;
  }
  RecordedEvent re;
  re.timestamp = std::chrono::steady_clock::now();
  re.event = event;
  re.description = Describe(event);
  events_.push_back(std::move(re));
}

void EventRecorder::Save(std::string path) const {
  std::ofstream out(path);
  if (!out.is_open()) {
    return;
  }
  if (events_.empty()) {
    return;
  }
  const auto base_time = events_.front().timestamp;
  for (const auto& re : events_) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  re.timestamp - base_time)
                  .count();
    out << ms << " " << SerializeEvent(re.event) << "\n";
  }
}

bool EventRecorder::Load(std::string path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    return false;
  }
  events_.clear();
  const auto base = std::chrono::steady_clock::now();
  std::string line;
  bool any = false;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    std::istringstream iss(line);
    long long ms = 0;
    iss >> ms;
    std::string rest;
    std::getline(iss, rest);
    if (!rest.empty() && rest.front() == ' ') {
      rest.erase(0, 1);
    }
    ftxui::Event ev = ftxui::Event::Custom;
    if (!DeserializeEvent(rest, ev)) {
      continue;
    }
    RecordedEvent re;
    re.timestamp = base + std::chrono::milliseconds(ms);
    re.event = ev;
    re.description = Describe(ev);
    events_.push_back(std::move(re));
    any = true;
  }
  return any;
}

void EventRecorder::Replay(std::function<void()> on_complete) {
  if (events_.empty() || replaying_) {
    return;
  }
  replaying_ = true;

  // Capture events by value for the thread.
  auto events_copy = events_;
  auto complete_cb = std::move(on_complete);

  std::thread([this, events_copy = std::move(events_copy),
               complete_cb = std::move(complete_cb)]() mutable {
    const auto wall_start = std::chrono::steady_clock::now();
    const auto rec_start = events_copy.empty()
                               ? wall_start
                               : events_copy.front().timestamp;

    for (const auto& re : events_copy) {
      if (!replaying_) {
        break;
      }
      // Sleep until the event's relative time.
      auto delay = re.timestamp - rec_start;
      auto target = wall_start + delay;
      std::this_thread::sleep_until(target);

      if (!replaying_) {
        break;
      }
      auto* app = ftxui::App::Active();
      if (app) {
        app->PostEvent(re.event);
      }
    }

    replaying_ = false;
    if (complete_cb) {
      // Call on the replay thread (safe because it's just a callback).
      complete_cb();
    }
  }).detach();
}

void EventRecorder::StopReplay() {
  replaying_ = false;
}

bool EventRecorder::IsReplaying() const {
  return replaying_;
}

const std::vector<RecordedEvent>& EventRecorder::Events() const {
  return events_;
}

int EventRecorder::EventCount() const {
  return static_cast<int>(events_.size());
}

void EventRecorder::Clear() {
  events_.clear();
}

ftxui::Component EventRecorder::WithRecording(ftxui::Component inner) {
  return std::make_shared<RecordingComponent>(std::move(inner));
}

}  // namespace ftxui::ui
