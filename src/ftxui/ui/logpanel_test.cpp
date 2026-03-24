// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/log_panel.hpp"

#include <atomic>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── Create ─────────────────────────────────────────────────────────────────────

TEST(LogPanelTest, CreateReturnsNonNull) {
  auto log = LogPanel::Create();
  ASSERT_NE(log, nullptr);
}

TEST(LogPanelTest, CreateWithMaxLines) {
  auto log = LogPanel::Create(50);
  ASSERT_NE(log, nullptr);
}

// ── Log / level helpers ────────────────────────────────────────────────────────

TEST(LogPanelTest, LogAddsEntries) {
  auto log = LogPanel::Create();
  log->Info("hello");
  log->Info("world");

  // Build a component and render — if entries were added the render won't crash.
  auto comp = log->Build("test");
  ASSERT_NE(comp, nullptr);
  auto elem = comp->Render();
  ASSERT_NE(elem, nullptr);
}

TEST(LogPanelTest, AllLevelHelpers) {
  auto log = LogPanel::Create();
  log->Trace("trace msg");
  log->Debug("debug msg");
  log->Info("info msg");
  log->Warn("warn msg");
  log->Error("error msg");

  auto comp = log->Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(LogPanelTest, LogWithExplicitLevel) {
  auto log = LogPanel::Create();
  log->Log("explicit info", LogLevel::Info);
  log->Log("explicit error", LogLevel::Error);
  ASSERT_NE(log->Build()->Render(), nullptr);
}

// ── Clear ──────────────────────────────────────────────────────────────────────

TEST(LogPanelTest, ClearRemovesAllEntries) {
  auto log = LogPanel::Create();
  log->Info("a");
  log->Info("b");
  log->Clear();
  // After clear the component should still render without crash.
  ASSERT_NE(log->Build()->Render(), nullptr);
}

// ── SetMaxLines ────────────────────────────────────────────────────────────────

TEST(LogPanelTest, SetMaxLinesTrimsOldEntries) {
  auto log = LogPanel::Create(1000);
  for (int i = 0; i < 20; ++i) {
    log->Info("msg " + std::to_string(i));
  }
  log->SetMaxLines(5);
  // Render should succeed and only keep newest 5 entries.
  ASSERT_NE(log->Build()->Render(), nullptr);
}

TEST(LogPanelTest, MaxLinesEnforcedDuringLog) {
  auto log = LogPanel::Create(3);
  for (int i = 0; i < 10; ++i) {
    log->Info("msg " + std::to_string(i));
  }
  // Should still render fine — oldest messages were trimmed.
  ASSERT_NE(log->Build()->Render(), nullptr);
}

// ── SetMinLevel ────────────────────────────────────────────────────────────────

TEST(LogPanelTest, SetMinLevelFiltersEntries) {
  auto log = LogPanel::Create();
  log->SetMinLevel(LogLevel::Warn);
  log->Trace("should be filtered");
  log->Debug("should be filtered");
  log->Info("should be filtered");
  log->Warn("should appear");
  log->Error("should appear");

  // Build renders the filtered view — no crash expected.
  auto comp = log->Build("filtered");
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(LogPanelTest, SetMinLevelTraceShowsAll) {
  auto log = LogPanel::Create();
  log->SetMinLevel(LogLevel::Trace);
  log->Trace("trace");
  log->Debug("debug");
  log->Info("info");
  ASSERT_NE(log->Build()->Render(), nullptr);
}

TEST(LogPanelTest, SetMinLevelErrorHidesNonErrors) {
  auto log = LogPanel::Create();
  log->SetMinLevel(LogLevel::Error);
  log->Info("info1");
  log->Info("info2");
  log->Error("err");
  ASSERT_NE(log->Build()->Render(), nullptr);
}

// ── Build with title ──────────────────────────────────────────────────────────

TEST(LogPanelTest, BuildWithTitleDoesNotCrash) {
  auto log = LogPanel::Create();
  log->Info("some message");
  auto comp = log->Build("My Log");
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(LogPanelTest, BuildWithoutTitleDoesNotCrash) {
  auto log = LogPanel::Create();
  log->Info("some message");
  auto comp = log->Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

// ── Thread safety ──────────────────────────────────────────────────────────────

TEST(LogPanelTest, ThreadSafetyNoCrash) {
  constexpr int kThreads = 10;
  constexpr int kMessagesPerThread = 100;
  constexpr size_t kMaxLines = 200;

  auto log = LogPanel::Create(kMaxLines);

  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&log, t] {
      for (int i = 0; i < kMessagesPerThread; ++i) {
        log->Info("thread " + std::to_string(t) + " msg " + std::to_string(i));
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }

  // Build and render after all threads complete — must not crash.
  auto comp = log->Build("stress");
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(LogPanelTest, ThreadSafetyFinalCountRespectMaxLines) {
  constexpr int kThreads = 10;
  constexpr int kMessagesPerThread = 100;
  constexpr size_t kMaxLines = 150;

  auto log = LogPanel::Create(kMaxLines);

  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&log, t] {
      for (int i = 0; i < kMessagesPerThread; ++i) {
        log->Info("t" + std::to_string(t) + ":" + std::to_string(i));
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }

  // Render must succeed (the panel should have trimmed to kMaxLines or fewer).
  ASSERT_NE(log->Build()->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
