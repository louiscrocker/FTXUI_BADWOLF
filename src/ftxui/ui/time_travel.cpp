// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/time_travel.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/json.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ============================================================================
// StateRecorder
// ============================================================================

StateRecorder::StateRecorder(size_t max_snapshots)
    : max_snapshots_(max_snapshots) {}

StateRecorder::StateRecorder(StateRecorder&& other) noexcept {
  std::lock_guard<std::mutex> lock(other.mutex_);
  snapshots_ = std::move(other.snapshots_);
  max_snapshots_ = other.max_snapshots_;
  next_id_ = other.next_id_;
  snapshots_cache_ = std::move(other.snapshots_cache_);
}

StateRecorder& StateRecorder::operator=(StateRecorder&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  std::lock_guard<std::mutex> lock(other.mutex_);
  std::lock_guard<std::mutex> lock2(mutex_);
  snapshots_ = std::move(other.snapshots_);
  max_snapshots_ = other.max_snapshots_;
  next_id_ = other.next_id_;
  snapshots_cache_ = std::move(other.snapshots_cache_);
  return *this;
}

void StateRecorder::Record(const std::string& key,
                           const JsonValue& before,
                           const JsonValue& after,
                           const std::string& label,
                           const std::string& trigger) {
  std::lock_guard<std::mutex> lock(mutex_);
  StateSnapshot snap;
  snap.id = next_id_++;
  snap.timestamp = std::chrono::steady_clock::now();
  snap.label = label;
  snap.state_key = key;
  snap.before = before;
  snap.after = after;
  snap.trigger = trigger;
  snapshots_.push_back(std::move(snap));
  while (snapshots_.size() > max_snapshots_) {
    snapshots_.pop_front();
  }
  RebuildCache();
}

void StateRecorder::RebuildCache() const {
  snapshots_cache_.assign(snapshots_.begin(), snapshots_.end());
}

const std::vector<StateSnapshot>& StateRecorder::Snapshots() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshots_cache_;
}

size_t StateRecorder::Count() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshots_.size();
}

void StateRecorder::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshots_.clear();
  next_id_ = 0;
  RebuildCache();
}

JsonValue StateRecorder::ExportJson() const {
  std::lock_guard<std::mutex> lock(mutex_);
  JsonValue arr = JsonValue::Array();
  for (const auto& snap : snapshots_) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  snap.timestamp.time_since_epoch())
                  .count();
    JsonValue obj = JsonValue::Object();
    obj["id"] = JsonValue(static_cast<double>(snap.id));
    obj["timestamp"] = JsonValue(static_cast<double>(ms));
    obj["label"] = JsonValue(snap.label);
    obj["state_key"] = JsonValue(snap.state_key);
    obj["trigger"] = JsonValue(snap.trigger);
    obj["before"] = snap.before;
    obj["after"] = snap.after;
    arr.push(std::move(obj));
  }
  return arr;
}

bool StateRecorder::SaveToFile(const std::string& path) const {
  std::ofstream f(path);
  if (!f.is_open()) {
    return false;
  }
  f << Json::StringifyPretty(ExportJson());
  return f.good();
}

StateRecorder StateRecorder::LoadFromFile(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    return StateRecorder{};
  }
  std::string content((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
  std::string err;
  JsonValue arr = Json::ParseSafe(content, &err);
  StateRecorder rec;
  if (!arr.is_array()) {
    return rec;
  }
  auto now = std::chrono::steady_clock::now();
  for (size_t i = 0; i < arr.as_array().size(); ++i) {
    const auto& obj = arr[i];
    if (!obj.is_object()) {
      continue;
    }
    StateSnapshot snap;
    snap.id = static_cast<uint64_t>(obj["id"].get_number(0.0));
    auto ms_val = static_cast<int64_t>(obj["timestamp"].get_number(0.0));
    snap.timestamp =
        now +
        std::chrono::milliseconds(
            ms_val - std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count());
    snap.label = obj["label"].get_string("");
    snap.state_key = obj["state_key"].get_string("");
    snap.trigger = obj["trigger"].get_string("unknown");
    snap.before = obj.has("before") ? obj["before"] : JsonValue{};
    snap.after = obj.has("after") ? obj["after"] : JsonValue{};
    rec.snapshots_.push_back(std::move(snap));
  }
  rec.RebuildCache();
  if (!rec.snapshots_.empty()) {
    rec.next_id_ = rec.snapshots_.back().id + 1;
  }
  return rec;
}

StateRecorder& StateRecorder::Global() {
  static StateRecorder instance;
  return instance;
}

// ============================================================================
// TimelinePlayer
// ============================================================================

TimelinePlayer::TimelinePlayer(const StateRecorder& recorder)
    : recorder_(recorder) {}

TimelinePlayer::~TimelinePlayer() {
  StopPlayThread();
}

void TimelinePlayer::StopPlayThread() {
  stop_requested_.store(true);
  playing_.store(false);
  if (play_thread_.joinable()) {
    play_thread_.join();
  }
}

void TimelinePlayer::FireCallbacks(size_t index) {
  const auto& snaps = recorder_.Snapshots();
  if (snaps.empty()) {
    return;
  }
  size_t clamped = std::min(index, snaps.size() - 1);
  std::map<int, std::function<void(size_t, const StateSnapshot&)>> cbs;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    cbs = callbacks_;
  }
  for (auto& [id, fn] : cbs) {
    fn(clamped, snaps[clamped]);
  }
}

void TimelinePlayer::GoToSnapshot(size_t index) {
  const auto& snaps = recorder_.Snapshots();
  if (snaps.empty()) {
    return;
  }
  current_index_ = std::min(index, snaps.size() - 1);
  FireCallbacks(current_index_);
}

void TimelinePlayer::GoToTime(std::chrono::steady_clock::time_point t) {
  const auto& snaps = recorder_.Snapshots();
  if (snaps.empty()) {
    return;
  }
  // Find last snapshot at or before t
  size_t best = 0;
  for (size_t i = 0; i < snaps.size(); ++i) {
    if (snaps[i].timestamp <= t) {
      best = i;
    }
  }
  GoToSnapshot(best);
}

void TimelinePlayer::StepForward() {
  const auto& snaps = recorder_.Snapshots();
  if (snaps.empty()) {
    return;
  }
  if (current_index_ + 1 < snaps.size()) {
    GoToSnapshot(current_index_ + 1);
  }
}

void TimelinePlayer::StepBackward() {
  if (current_index_ > 0) {
    GoToSnapshot(current_index_ - 1);
  }
}

void TimelinePlayer::GoToStart() {
  GoToSnapshot(0);
}

void TimelinePlayer::GoToEnd() {
  const auto& snaps = recorder_.Snapshots();
  if (!snaps.empty()) {
    GoToSnapshot(snaps.size() - 1);
  }
}

void TimelinePlayer::PlayFrom(size_t index, double speed) {
  StopPlayThread();
  const auto& snaps = recorder_.Snapshots();
  if (snaps.empty()) {
    return;
  }
  stop_requested_.store(false);
  playing_.store(true);
  GoToSnapshot(index);

  play_thread_ = std::thread([this, speed]() {
    while (!stop_requested_.load()) {
      const auto& snaps = recorder_.Snapshots();
      if (snaps.empty() || current_index_ >= snaps.size() - 1) {
        playing_.store(false);
        break;
      }
      size_t next = current_index_ + 1;
      auto dt = snaps[next].timestamp - snaps[current_index_].timestamp;
      auto sleep_dur = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::duration<double>(dt) / speed);
      // Cap sleep to 10s to stay responsive to stop requests
      auto max_sleep = std::chrono::microseconds(10'000'000);
      sleep_dur = std::min(sleep_dur, max_sleep);

      // Sleep in small chunks so stop_requested_ is checked frequently
      auto chunk = std::chrono::milliseconds(50);
      auto remaining = sleep_dur;
      while (remaining.count() > 0 && !stop_requested_.load()) {
        auto this_chunk =
            remaining < chunk
                ? remaining
                : std::chrono::duration_cast<std::chrono::microseconds>(chunk);
        std::this_thread::sleep_for(this_chunk);
        remaining -= this_chunk;
      }
      if (stop_requested_.load()) {
        break;
      }
      GoToSnapshot(next);
    }
    playing_.store(false);
  });
}

void TimelinePlayer::Pause() {
  StopPlayThread();
}

size_t TimelinePlayer::CurrentIndex() const {
  return current_index_;
}

const StateSnapshot& TimelinePlayer::CurrentSnapshot() const {
  const auto& snaps = recorder_.Snapshots();
  static StateSnapshot empty{};
  if (snaps.empty()) {
    return empty;
  }
  size_t idx = std::min(current_index_, snaps.size() - 1);
  return snaps[idx];
}

bool TimelinePlayer::AtStart() const {
  return current_index_ == 0;
}

bool TimelinePlayer::AtEnd() const {
  const auto& snaps = recorder_.Snapshots();
  return snaps.empty() || current_index_ >= snaps.size() - 1;
}

bool TimelinePlayer::IsPlaying() const {
  return playing_.load();
}

int TimelinePlayer::OnPositionChange(
    std::function<void(size_t index, const StateSnapshot&)> fn) {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  int id = next_cb_id_++;
  callbacks_[id] = std::move(fn);
  return id;
}

void TimelinePlayer::RemoveCallback(int id) {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  callbacks_.erase(id);
}

// ============================================================================
// TimeTravelDebugger Component
// ============================================================================

ftxui::Component TimeTravelDebugger(std::shared_ptr<StateRecorder> recorder,
                                    ftxui::Component /*app_component*/) {
  if (!recorder) {
    recorder = std::shared_ptr<StateRecorder>(&StateRecorder::Global(),
                                              [](StateRecorder*) {});
  }

  struct State {
    std::shared_ptr<StateRecorder> recorder;
    std::shared_ptr<TimelinePlayer> player;
    std::vector<double> speeds = {0.25, 0.5, 1.0, 2.0, 4.0};
    int speed_idx = 2;
  };

  auto state = std::make_shared<State>();
  state->recorder = recorder;
  state->player = std::make_shared<TimelinePlayer>(*recorder);

  auto btn_start = Button(
      "◀◀", [state] { state->player->GoToStart(); }, ButtonOption::Ascii());
  auto btn_back = Button(
      "◀", [state] { state->player->StepBackward(); }, ButtonOption::Ascii());
  auto btn_play = Button(
      "▶/⏸",
      [state] {
        if (state->player->IsPlaying()) {
          state->player->Pause();
        } else {
          state->player->PlayFrom(state->player->CurrentIndex(),
                                  state->speeds[state->speed_idx]);
        }
      },
      ButtonOption::Ascii());
  auto btn_forward = Button(
      "▶", [state] { state->player->StepForward(); }, ButtonOption::Ascii());
  auto btn_end = Button(
      "▶▶", [state] { state->player->GoToEnd(); }, ButtonOption::Ascii());

  auto controls = Container::Horizontal(
      {btn_start, btn_back, btn_play, btn_forward, btn_end});

  auto renderer = Renderer(controls, [state, controls]() -> Element {
    const auto& snaps = state->recorder->Snapshots();

    if (snaps.empty()) {
      return window(text(" Time-Travel Debugger "),
                    vbox({
                        controls->Render(),
                        separator(),
                        text("No state recorded yet") | center | flex,
                    }));
    }

    size_t cur = state->player->CurrentIndex();
    if (cur >= snaps.size()) {
      cur = snaps.size() - 1;
    }
    const auto& snap = snaps[cur];

    // Scrubber bar
    float progress = snaps.size() > 1 ? static_cast<float>(cur) /
                                            static_cast<float>(snaps.size() - 1)
                                      : 0.f;
    int bar_width = 30;
    int pos = static_cast<int>(progress * static_cast<float>(bar_width));
    std::string bar = "[";
    for (int i = 0; i < bar_width; ++i) {
      bar += (i == pos ? "●" : "─");
    }
    bar += "]";

    // Timeline list (show ±3 entries around current)
    std::vector<Element> timeline_entries;
    int start_idx = std::max(0, static_cast<int>(cur) - 3);
    int end_idx =
        std::min(static_cast<int>(snaps.size()) - 1, static_cast<int>(cur) + 3);
    for (int i = start_idx; i <= end_idx; ++i) {
      const auto& s = snaps[static_cast<size_t>(i)];
      bool is_cur = (static_cast<size_t>(i) == cur);
      std::string prefix = is_cur ? "► " : "  ";
      std::string lbl = s.label.empty() ? s.state_key : s.label;
      auto entry = hbox({text(prefix + "#" + std::to_string(i) + " " + lbl) |
                             (is_cur ? bold : nothing),
                         filler(), text(s.trigger)});
      if (is_cur) {
        timeline_entries.push_back(entry | color(Color::Yellow));
      } else {
        timeline_entries.push_back(entry);
      }
    }

    std::string before_str = Json::StringifyPretty(snap.before);
    std::string after_str = Json::StringifyPretty(snap.after);

    return window(
        text(" Time-Travel Debugger "),
        vbox({
            hbox({controls->Render(), filler(),
                  text(bar + " #" + std::to_string(cur) + "/" +
                       std::to_string(snaps.size()))}),
            separator(),
            hbox({text("#" + std::to_string(cur) + " " +
                       (snap.label.empty() ? snap.state_key : snap.label)),
                  filler(), text("[" + snap.trigger + "]")}),
            separator(),
            text(" Timeline:"),
            vbox(std::move(timeline_entries)),
            separator(),
            hbox({vbox({text(" Before:"), text(before_str)}) | flex,
                  separator(),
                  vbox({text(" After:"), text(after_str)}) | flex}),
        }));
  });

  return CatchEvent(renderer, [state](Event event) {
    if (event == Event::ArrowLeft) {
      state->player->StepBackward();
      return true;
    }
    if (event == Event::ArrowRight) {
      state->player->StepForward();
      return true;
    }
    if (event == Event::Home) {
      state->player->GoToStart();
      return true;
    }
    if (event == Event::End) {
      state->player->GoToEnd();
      return true;
    }
    if (event == Event::Character(' ')) {
      if (state->player->IsPlaying()) {
        state->player->Pause();
      } else {
        state->player->PlayFrom(state->player->CurrentIndex(),
                                state->speeds[state->speed_idx]);
      }
      return true;
    }
    return false;
  });
}

// ============================================================================
// WithTimeTravel
// ============================================================================

ftxui::Component WithTimeTravel(ftxui::Component app,
                                std::shared_ptr<StateRecorder> recorder) {
  if (!recorder) {
    recorder = std::shared_ptr<StateRecorder>(&StateRecorder::Global(),
                                              [](StateRecorder*) {});
  }

  auto visible = std::make_shared<bool>(false);
  auto debugger = TimeTravelDebugger(recorder, nullptr);
  auto main_container = Container::Vertical({app, debugger});

  auto renderer =
      Renderer(main_container, [app, debugger, visible]() -> Element {
        if (*visible) {
          return vbox({
              flex_grow(app->Render()),
              debugger->Render() | size(HEIGHT, LESS_THAN, 15),
          });
        }
        return vbox({
            flex_grow(app->Render()),
            hbox({filler(), text(" ⏱ F12=Debug ") | dim}),
        });
      });

  return CatchEvent(renderer, [visible](Event event) -> bool {
    if (event == Event::F12) {
      *visible = !*visible;
      return true;
    }
    return false;
  });
}

}  // namespace ftxui::ui
