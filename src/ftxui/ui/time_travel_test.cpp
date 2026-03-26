// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file time_travel_test.cpp
/// Tests for StateRecorder, TimelinePlayer, and related time-travel debugging
/// infrastructure.

#include "ftxui/ui/time_travel.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── StateSnapshot ────────────────────────────────────────────────────────────

TEST(StateSnapshotTest, DefaultConstruction) {
  StateSnapshot s;
  EXPECT_EQ(s.id, 0u);
  EXPECT_TRUE(s.label.empty());
  EXPECT_TRUE(s.state_key.empty());
  EXPECT_TRUE(s.trigger.empty());
  EXPECT_TRUE(s.before.is_null());
  EXPECT_TRUE(s.after.is_null());
}

// ── StateRecorder – basic recording ─────────────────────────────────────────

TEST(StateRecorderTest, StartsEmpty) {
  StateRecorder rec;
  EXPECT_EQ(rec.Count(), 0u);
  EXPECT_TRUE(rec.Snapshots().empty());
}

TEST(StateRecorderTest, RecordAddsSnapshot) {
  StateRecorder rec;
  rec.Record("counter", JsonValue(0), JsonValue(1), "counter changed",
             "user_event");
  EXPECT_EQ(rec.Count(), 1u);
  ASSERT_EQ(rec.Snapshots().size(), 1u);
  EXPECT_EQ(rec.Snapshots()[0].state_key, "counter");
  EXPECT_EQ(rec.Snapshots()[0].label, "counter changed");
  EXPECT_EQ(rec.Snapshots()[0].trigger, "user_event");
}

TEST(StateRecorderTest, RecordAssignsMonotonicIds) {
  StateRecorder rec;
  rec.Record("a", JsonValue(0), JsonValue(1));
  rec.Record("b", JsonValue(1), JsonValue(2));
  rec.Record("c", JsonValue(2), JsonValue(3));
  EXPECT_EQ(rec.Snapshots()[0].id, 0u);
  EXPECT_EQ(rec.Snapshots()[1].id, 1u);
  EXPECT_EQ(rec.Snapshots()[2].id, 2u);
}

TEST(StateRecorderTest, ClearRemovesAllSnapshots) {
  StateRecorder rec;
  rec.Record("x", JsonValue(0), JsonValue(1));
  rec.Record("y", JsonValue(1), JsonValue(2));
  rec.Clear();
  EXPECT_EQ(rec.Count(), 0u);
  EXPECT_TRUE(rec.Snapshots().empty());
}

TEST(StateRecorderTest, RingBufferEvictsOldest) {
  StateRecorder rec(3);
  rec.Record("a", JsonValue(0), JsonValue(1));
  rec.Record("b", JsonValue(1), JsonValue(2));
  rec.Record("c", JsonValue(2), JsonValue(3));
  rec.Record("d", JsonValue(3), JsonValue(4));  // evicts "a"
  EXPECT_EQ(rec.Count(), 3u);
  EXPECT_EQ(rec.Snapshots()[0].state_key, "b");
  EXPECT_EQ(rec.Snapshots()[2].state_key, "d");
}

// ── StateRecorder – before/after values ──────────────────────────────────────

TEST(StateRecorderTest, StoresBeforeAndAfterValues) {
  StateRecorder rec;
  JsonValue before("hello");
  JsonValue after("world");
  rec.Record("msg", before, after, "msg changed", "test");
  const auto& snap = rec.Snapshots()[0];
  EXPECT_EQ(snap.before.as_string(), "hello");
  EXPECT_EQ(snap.after.as_string(), "world");
}

TEST(StateRecorderTest, StoresTimestamp) {
  auto t0 = std::chrono::steady_clock::now();
  StateRecorder rec;
  rec.Record("ts", JsonValue(0), JsonValue(1));
  auto t1 = std::chrono::steady_clock::now();
  const auto& snap = rec.Snapshots()[0];
  EXPECT_GE(snap.timestamp, t0);
  EXPECT_LE(snap.timestamp, t1);
}

// ── StateRecorder – Track<T> ─────────────────────────────────────────────────

TEST(StateRecorderTest, TrackRecordsChanges) {
  StateRecorder rec;
  auto val = std::make_shared<Reactive<int>>(0);
  rec.Track<int>(val, "counter", [](const int& v) { return JsonValue(v); });
  val->Set(42);
  EXPECT_EQ(rec.Count(), 1u);
  const auto& snap = rec.Snapshots()[0];
  EXPECT_EQ(snap.state_key, "counter");
  EXPECT_EQ(snap.after.as_int(), 42);
  EXPECT_EQ(snap.before.as_int(), 0);
}

TEST(StateRecorderTest, TrackMultipleChanges) {
  StateRecorder rec;
  auto val = std::make_shared<Reactive<int>>(10);
  rec.Track<int>(val, "n", [](const int& v) { return JsonValue(v); });
  val->Set(20);
  val->Set(30);
  EXPECT_EQ(rec.Count(), 2u);
  EXPECT_EQ(rec.Snapshots()[0].before.as_int(), 10);
  EXPECT_EQ(rec.Snapshots()[0].after.as_int(), 20);
  EXPECT_EQ(rec.Snapshots()[1].before.as_int(), 20);
  EXPECT_EQ(rec.Snapshots()[1].after.as_int(), 30);
}

// ── StateRecorder – Export/Save/Load ─────────────────────────────────────────

TEST(StateRecorderTest, ExportJsonProducesArray) {
  StateRecorder rec;
  rec.Record("k", JsonValue(1), JsonValue(2), "lbl", "trig");
  JsonValue j = rec.ExportJson();
  EXPECT_TRUE(j.is_array());
  EXPECT_EQ(j.as_array().size(), 1u);
  const auto& obj = j[0];
  EXPECT_TRUE(obj.is_object());
  EXPECT_EQ(obj["state_key"].as_string(), "k");
  EXPECT_EQ(obj["label"].as_string(), "lbl");
  EXPECT_EQ(obj["trigger"].as_string(), "trig");
}

TEST(StateRecorderTest, ExportJsonContainsBeforeAfter) {
  StateRecorder rec;
  rec.Record("v", JsonValue(false), JsonValue(true));
  JsonValue j = rec.ExportJson();
  EXPECT_FALSE(j[0]["before"].as_bool());
  EXPECT_TRUE(j[0]["after"].as_bool());
}

TEST(StateRecorderTest, GlobalIsSingleton) {
  StateRecorder& g1 = StateRecorder::Global();
  StateRecorder& g2 = StateRecorder::Global();
  EXPECT_EQ(&g1, &g2);
}

TEST(StateRecorderTest, DefaultTriggerIsUnknown) {
  StateRecorder rec;
  rec.Record("k", JsonValue(0), JsonValue(1));  // no trigger arg
  EXPECT_EQ(rec.Snapshots()[0].trigger, "unknown");
}

TEST(StateRecorderTest, DefaultLabelIsEmpty) {
  StateRecorder rec;
  rec.Record("k", JsonValue(0), JsonValue(1));  // no label arg
  EXPECT_TRUE(rec.Snapshots()[0].label.empty());
}

// ── TimelinePlayer
// ────────────────────────────────────────────────────────────

TEST(TimelinePlayerTest, GoToSnapshotClampsIndex) {
  StateRecorder rec;
  rec.Record("a", JsonValue(0), JsonValue(1));
  rec.Record("b", JsonValue(1), JsonValue(2));
  TimelinePlayer player(rec);
  player.GoToSnapshot(999);
  EXPECT_EQ(player.CurrentIndex(), 1u);
}

TEST(TimelinePlayerTest, StepForwardAndBackward) {
  StateRecorder rec;
  rec.Record("a", JsonValue(0), JsonValue(1));
  rec.Record("b", JsonValue(1), JsonValue(2));
  rec.Record("c", JsonValue(2), JsonValue(3));
  TimelinePlayer player(rec);
  player.GoToStart();
  EXPECT_EQ(player.CurrentIndex(), 0u);
  player.StepForward();
  EXPECT_EQ(player.CurrentIndex(), 1u);
  player.StepForward();
  EXPECT_EQ(player.CurrentIndex(), 2u);
  player.StepForward();  // already at end
  EXPECT_EQ(player.CurrentIndex(), 2u);
  player.StepBackward();
  EXPECT_EQ(player.CurrentIndex(), 1u);
}

TEST(TimelinePlayerTest, AtStartAtEnd) {
  StateRecorder rec;
  rec.Record("a", JsonValue(0), JsonValue(1));
  rec.Record("b", JsonValue(1), JsonValue(2));
  TimelinePlayer player(rec);
  player.GoToStart();
  EXPECT_TRUE(player.AtStart());
  EXPECT_FALSE(player.AtEnd());
  player.GoToEnd();
  EXPECT_FALSE(player.AtStart());
  EXPECT_TRUE(player.AtEnd());
}

TEST(TimelinePlayerTest, OnPositionChangeCallback) {
  StateRecorder rec;
  rec.Record("a", JsonValue(0), JsonValue(1));
  rec.Record("b", JsonValue(1), JsonValue(2));
  TimelinePlayer player(rec);

  size_t called_with = 99;
  int cb_id = player.OnPositionChange(
      [&](size_t idx, const StateSnapshot&) { called_with = idx; });

  player.StepForward();
  EXPECT_EQ(called_with, 1u);

  player.RemoveCallback(cb_id);
  player.StepBackward();
  EXPECT_EQ(called_with, 1u);  // not called again
}

TEST(TimelinePlayerTest, IsPlayingAfterPlayFrom) {
  StateRecorder rec;
  // Add enough entries with spread timestamps to avoid instant completion
  for (int i = 0; i < 5; ++i) {
    rec.Record("x", JsonValue(i), JsonValue(i + 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  TimelinePlayer player(rec);
  player.PlayFrom(0, 0.01);  // very slow playback
  EXPECT_TRUE(player.IsPlaying());
  player.Pause();
  // Give the thread a moment to stop
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_FALSE(player.IsPlaying());
}

}  // namespace
}  // namespace ftxui::ui
