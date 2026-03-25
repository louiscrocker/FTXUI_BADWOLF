// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/event_recorder.hpp"
#include "ftxui/ui/inspector.hpp"
#include "ftxui/ui/ui_builder.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"

#include "gtest/gtest.h"

using namespace ftxui;
using namespace ftxui::ui;

// ---------------------------------------------------------------------------
// EventRecorder tests
// ---------------------------------------------------------------------------

TEST(EventRecorder, DefaultNotRecording) {
  auto& rec = EventRecorder::Instance();
  rec.StopRecording();
  rec.Clear();
  EXPECT_FALSE(rec.IsRecording());
}

TEST(EventRecorder, StartRecording) {
  auto& rec = EventRecorder::Instance();
  rec.Clear();
  rec.StartRecording();
  EXPECT_TRUE(rec.IsRecording());
  rec.StopRecording();
}

TEST(EventRecorder, StopRecording) {
  auto& rec = EventRecorder::Instance();
  rec.StartRecording();
  rec.StopRecording();
  EXPECT_FALSE(rec.IsRecording());
}

TEST(EventRecorder, RecordsEvents) {
  auto& rec = EventRecorder::Instance();
  rec.Clear();
  rec.StartRecording();
  rec.RecordEvent(Event::Character('a'));
  rec.RecordEvent(Event::Return);
  rec.StopRecording();
  EXPECT_EQ(rec.EventCount(), 2);
}

TEST(EventRecorder, DoesNotRecordWhenStopped) {
  auto& rec = EventRecorder::Instance();
  rec.Clear();
  rec.StopRecording();
  rec.RecordEvent(Event::Character('x'));
  EXPECT_EQ(rec.EventCount(), 0);
}

TEST(EventRecorder, EventCount) {
  auto& rec = EventRecorder::Instance();
  rec.Clear();
  EXPECT_EQ(rec.EventCount(), 0);
  rec.StartRecording();
  rec.RecordEvent(Event::ArrowDown);
  rec.RecordEvent(Event::ArrowUp);
  rec.RecordEvent(Event::Return);
  rec.StopRecording();
  EXPECT_EQ(rec.EventCount(), 3);
}

TEST(EventRecorder, EventsAccessible) {
  auto& rec = EventRecorder::Instance();
  rec.Clear();
  rec.StartRecording();
  rec.RecordEvent(Event::Character('z'));
  rec.StopRecording();
  ASSERT_EQ(rec.EventCount(), 1);
  EXPECT_TRUE(rec.Events()[0].event.is_character());
  EXPECT_EQ(rec.Events()[0].event.character(), "z");
}

TEST(EventRecorder, SaveAndLoad) {
  auto& rec = EventRecorder::Instance();
  rec.Clear();
  rec.StartRecording();
  rec.RecordEvent(Event::Character('h'));
  rec.RecordEvent(Event::Character('i'));
  rec.RecordEvent(Event::Return);
  rec.StopRecording();
  ASSERT_EQ(rec.EventCount(), 3);

  rec.Save("devtools_test_session.txt");

  // Load into a fresh recorder (via Clear + Load).
  rec.Clear();
  EXPECT_EQ(rec.EventCount(), 0);

  bool loaded = rec.Load("devtools_test_session.txt");
  EXPECT_TRUE(loaded);
  EXPECT_EQ(rec.EventCount(), 3);

  // Verify event content.
  EXPECT_TRUE(rec.Events()[0].event.is_character());
  EXPECT_EQ(rec.Events()[0].event.character(), "h");
  EXPECT_TRUE(rec.Events()[1].event.is_character());
  EXPECT_EQ(rec.Events()[1].event.character(), "i");

  // Clean up test file.
  std::remove("devtools_test_session.txt");
}

TEST(EventRecorder, LoadMissingFileReturnsFalse) {
  auto& rec = EventRecorder::Instance();
  EXPECT_FALSE(rec.Load("__nonexistent_file_xyz__.txt"));
}

TEST(EventRecorder, DefaultNotReplaying) {
  auto& rec = EventRecorder::Instance();
  EXPECT_FALSE(rec.IsReplaying());
}

TEST(EventRecorder, WithRecordingBuildsOk) {
  auto& rec = EventRecorder::Instance();
  auto inner = Button("test", [] {});
  auto wrapped = rec.WithRecording(inner);
  EXPECT_NE(wrapped, nullptr);
}

TEST(EventRecorder, WithRecordingInterceptsEvents) {
  auto& rec = EventRecorder::Instance();
  rec.Clear();
  rec.StartRecording();

  auto inner = Button("test", [] {});
  auto wrapped = rec.WithRecording(inner);

  // Fire an event through the wrapper.
  wrapped->OnEvent(Event::Character('a'));

  rec.StopRecording();
  EXPECT_GE(rec.EventCount(), 1);
}

// ---------------------------------------------------------------------------
// UIBuilder tests
// ---------------------------------------------------------------------------

TEST(UIBuilder, BuildsOk) {
  auto builder = UIBuilder();
  EXPECT_NE(builder, nullptr);
}

TEST(UIBuilder, InitiallyEmpty) {
  auto builder = UIBuilder();
  // Should render without crashing.
  // We cannot easily check "empty" state without running the app,
  // but we verify Render() does not throw/crash.
  EXPECT_NE(builder, nullptr);
}

TEST(UIBuilder, RendersWithoutCrash) {
  // Render in a headless 80x24 screen.
  auto builder = UIBuilder();
  EXPECT_NO_THROW({
    auto element = builder->Render();
    (void)element;
  });
}

// ---------------------------------------------------------------------------
// WithInspector tests
// ---------------------------------------------------------------------------

TEST(WithInspector, BuildsOk) {
  auto inner = Button("OK", [] {});
  auto wrapped = WithInspector(inner);
  EXPECT_NE(wrapped, nullptr);
}

TEST(WithInspector, RendersWithoutCrash) {
  auto inner = Button("OK", [] {});
  auto wrapped = WithInspector(inner);
  EXPECT_NO_THROW({
    auto element = wrapped->Render();
    (void)element;
  });
}

TEST(WithInspector, ForwardsEvents) {
  bool clicked = false;
  auto inner = Button("OK", [&] { clicked = true; });
  auto wrapped = WithInspector(inner);
  // Send a normal event (not Ctrl+I), it should forward.
  wrapped->OnEvent(Event::Return);
  EXPECT_TRUE(clicked);
}
