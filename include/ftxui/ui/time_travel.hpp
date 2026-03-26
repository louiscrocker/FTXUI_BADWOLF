// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_TIME_TRAVEL_HPP
#define FTXUI_UI_TIME_TRAVEL_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/ui/json.hpp"
#include "ftxui/ui/reactive.hpp"

namespace ftxui::ui {

// ── StateSnapshot ──────────────────────────────────────────────────────────
struct StateSnapshot {
  uint64_t id = 0;
  std::chrono::steady_clock::time_point timestamp;
  std::string label;
  std::string state_key;
  JsonValue before;
  JsonValue after;
  std::string trigger;
};

// ── StateRecorder ──────────────────────────────────────────────────────────
class StateRecorder {
 public:
  explicit StateRecorder(size_t max_snapshots = 1000);

  void Record(const std::string& key,
              const JsonValue& before,
              const JsonValue& after,
              const std::string& label = "",
              const std::string& trigger = "unknown");

  // Track a Reactive<T>, recording a snapshot on every change.
  template <typename T>
  void Track(std::shared_ptr<Reactive<T>> reactive,
             const std::string& key,
             std::function<JsonValue(const T&)> serializer = nullptr) {
    if (!serializer) {
      serializer = [](const T& val) -> JsonValue {
        std::ostringstream oss;
        oss << val;
        return JsonValue(oss.str());
      };
    }

    auto prev = std::make_shared<JsonValue>(serializer(reactive->Get()));
    auto* recorder = this;

    reactive->OnChange([recorder, key, serializer, prev](const T& new_val) {
      JsonValue after_val = serializer(new_val);
      JsonValue before_val = *prev;
      recorder->Record(key, before_val, after_val, key + " changed",
                       "user_event");
      *prev = after_val;
    });
  }

  const std::vector<StateSnapshot>& Snapshots() const;
  size_t Count() const;
  void Clear();

  JsonValue ExportJson() const;
  bool SaveToFile(const std::string& path) const;
  static StateRecorder LoadFromFile(const std::string& path);

  static StateRecorder& Global();

 private:
  mutable std::mutex mutex_;
  std::deque<StateSnapshot> snapshots_;
  size_t max_snapshots_;
  uint64_t next_id_ = 0;
  mutable std::vector<StateSnapshot> snapshots_cache_;

  void RebuildCache() const;

 public:
  // Move constructor needed since std::mutex is non-movable.
  StateRecorder(StateRecorder&& other) noexcept;
  StateRecorder& operator=(StateRecorder&& other) noexcept;
};

// ── TimelinePlayer ─────────────────────────────────────────────────────────
class TimelinePlayer {
 public:
  explicit TimelinePlayer(const StateRecorder& recorder);
  ~TimelinePlayer();

  void GoToSnapshot(size_t index);
  void GoToTime(std::chrono::steady_clock::time_point t);
  void StepForward();
  void StepBackward();
  void GoToStart();
  void GoToEnd();
  void PlayFrom(size_t index, double speed = 1.0);
  void Pause();

  size_t CurrentIndex() const;
  const StateSnapshot& CurrentSnapshot() const;
  bool AtStart() const;
  bool AtEnd() const;
  bool IsPlaying() const;

  int OnPositionChange(
      std::function<void(size_t index, const StateSnapshot&)> fn);
  void RemoveCallback(int id);

 private:
  const StateRecorder& recorder_;
  size_t current_index_ = 0;
  std::atomic<bool> playing_{false};
  std::atomic<bool> stop_requested_{false};
  std::thread play_thread_;
  mutable std::mutex cb_mutex_;
  std::map<int, std::function<void(size_t, const StateSnapshot&)>> callbacks_;
  int next_cb_id_ = 0;

  void FireCallbacks(size_t index);
  void StopPlayThread();
};

// ── TimeTravelDebugger Component ───────────────────────────────────────────
ftxui::Component TimeTravelDebugger(
    std::shared_ptr<StateRecorder> recorder = nullptr,
    ftxui::Component app_component = nullptr);

// ── WithTimeTravel ─────────────────────────────────────────────────────────
ftxui::Component WithTimeTravel(
    ftxui::Component app,
    std::shared_ptr<StateRecorder> recorder = nullptr);

// ── BADWOLF_TRACK macro ────────────────────────────────────────────────────
#define BADWOLF_TRACK(reactive_ptr, key) \
  ftxui::ui::StateRecorder::Global().Track(reactive_ptr, key)

}  // namespace ftxui::ui

#endif  // FTXUI_UI_TIME_TRAVEL_HPP
