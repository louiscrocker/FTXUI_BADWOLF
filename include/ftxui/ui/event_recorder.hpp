// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_EVENT_RECORDER_HPP
#define FTXUI_UI_EVENT_RECORDER_HPP

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"

namespace ftxui::ui {

/// @brief A single recorded event with its timestamp and description.
struct RecordedEvent {
  std::chrono::steady_clock::time_point timestamp;
  ftxui::Event event;
  std::string description;
};

/// @brief Singleton that records and replays terminal events.
///
/// Usage:
/// @code
/// auto& rec = EventRecorder::Instance();
/// rec.StartRecording();
/// // ... user interacts ...
/// rec.StopRecording();
/// rec.Save("session.txt");
///
/// // later:
/// rec.Load("session.txt");
/// rec.Replay([](){ /* done */ });
/// @endcode
///
/// @ingroup ui
class EventRecorder {
 public:
  static EventRecorder& Instance();

  /// @brief Start recording all events. Events must be fed via
  /// WithRecording() or manually via RecordEvent().
  void StartRecording();
  void StopRecording();
  bool IsRecording() const;

  /// @brief Record a single event (called automatically by WithRecording).
  void RecordEvent(const ftxui::Event& event);

  /// @brief Save recorded session to a text file.
  /// Format: one line per event: "TIME_MS TYPE DATA"
  void Save(std::string path) const;

  /// @brief Load a session from a previously saved file.
  /// Returns true on success.
  bool Load(std::string path);

  /// @brief Replay loaded session, injecting events at original timing
  /// into the active App. Calls on_complete when the replay finishes.
  void Replay(std::function<void()> on_complete = nullptr);
  void StopReplay();
  bool IsReplaying() const;

  /// @brief Access recorded events.
  const std::vector<RecordedEvent>& Events() const;
  int EventCount() const;

  /// @brief Clear all recorded events.
  void Clear();

  /// @brief Wrap a component so that all events it receives are recorded.
  ftxui::Component WithRecording(ftxui::Component inner);

 private:
  EventRecorder() = default;
  ~EventRecorder();

  bool recording_ = false;
  bool replaying_ = false;
  std::vector<RecordedEvent> events_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_EVENT_RECORDER_HPP
