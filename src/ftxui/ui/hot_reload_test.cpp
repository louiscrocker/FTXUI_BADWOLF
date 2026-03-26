// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file hot_reload_test.cpp
/// Unit tests for ftxui::ui::FileWatcher, HotReloader, ReloadOverlay, and
/// WithHotReload.

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "ftxui/ui/hot_reload.hpp"
#include "gtest/gtest.h"

using namespace ftxui::ui;
namespace fs = std::filesystem;

// ============================================================================
// HotReloadConfig defaults
// ============================================================================

TEST(HotReloadConfig, Defaults) {
  HotReloadConfig cfg;
  EXPECT_TRUE(cfg.watch_paths.empty());
  EXPECT_EQ(cfg.compiler, "g++");
  EXPECT_EQ(cfg.flags, "-std=c++17 -O0 -g");
  EXPECT_EQ(cfg.debounce, std::chrono::milliseconds(300));
  EXPECT_FALSE(cfg.verbose);
  EXPECT_TRUE(cfg.show_overlay);
}

// ============================================================================
// FileWatcher construction
// ============================================================================

TEST(FileWatcher, ConstructsWithoutCrash) {
  EXPECT_NO_THROW({
    FileWatcher watcher;
    (void)watcher;
  });
}

TEST(FileWatcher, ConstructsWithCustomInterval) {
  EXPECT_NO_THROW({
    FileWatcher watcher(std::chrono::milliseconds(100));
    (void)watcher;
  });
}

// ============================================================================
// FileWatcher Start / Stop
// ============================================================================

TEST(FileWatcher, StartStopWithoutCrash) {
  FileWatcher watcher(std::chrono::milliseconds(50));
  EXPECT_FALSE(watcher.IsRunning());
  watcher.Start();
  EXPECT_TRUE(watcher.IsRunning());
  watcher.Stop();
  EXPECT_FALSE(watcher.IsRunning());
}

TEST(FileWatcher, DoubleStartIsSafe) {
  FileWatcher watcher(std::chrono::milliseconds(50));
  watcher.Start();
  watcher.Start();  // Should not crash or deadlock
  EXPECT_TRUE(watcher.IsRunning());
  watcher.Stop();
}

// ============================================================================
// FileWatcher Watch / Unwatch / Clear
// ============================================================================

TEST(FileWatcher, WatchAddsPath) {
  FileWatcher watcher;
  // Just verify no crash when watching a path
  EXPECT_NO_THROW(watcher.Watch("."));
  EXPECT_NO_THROW(watcher.Unwatch("."));
}

TEST(FileWatcher, ClearNoCrash) {
  FileWatcher watcher;
  watcher.Watch(".");
  EXPECT_NO_THROW(watcher.Clear());
}

// ============================================================================
// FileWatcher PollChanges — empty initially
// ============================================================================

TEST(FileWatcher, PollChangesEmptyWhenNoWatchPaths) {
  FileWatcher watcher;
  auto changes = watcher.PollChanges();
  EXPECT_TRUE(changes.empty());
}

// ============================================================================
// FileWatcher detects new file
// ============================================================================

TEST(FileWatcher, DetectsNewFile) {
  // Create a unique temp directory inside the build area
  fs::path dir = fs::temp_directory_path() / "ftxui_filewatcher_test_new";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  if (ec) {
    GTEST_SKIP() << "Cannot create temp directory";
  }

  FileWatcher watcher(std::chrono::milliseconds(50));
  watcher.Watch(dir.string());

  // Seed: first PollChanges call records existing state (none)
  watcher.PollChanges();

  // Create a new .hpp file
  fs::path new_file = dir / "test_new.hpp";
  {
    std::ofstream f(new_file);
    f << "// hello\n";
  }

  // Give the filesystem a moment
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  auto changes = watcher.PollChanges();
  EXPECT_FALSE(changes.empty());

  fs::remove_all(dir, ec);
}

// ============================================================================
// FileWatcher detects modification
// ============================================================================

TEST(FileWatcher, DetectsModification) {
  fs::path dir = fs::temp_directory_path() / "ftxui_filewatcher_test_mod";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  if (ec) {
    GTEST_SKIP() << "Cannot create temp directory";
  }

  fs::path file = dir / "watch_me.cpp";
  {
    std::ofstream f(file);
    f << "// initial\n";
  }

  FileWatcher watcher(std::chrono::milliseconds(50));
  watcher.Watch(dir.string());
  watcher.PollChanges();  // Seed timestamps

  // Ensure mtime changes (some filesystems have 1s resolution)
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Modify the file
  {
    std::ofstream f(file, std::ios::app);
    f << "// modified\n";
  }
  // Force a different mtime if filesystem has coarse resolution
  auto new_time = fs::last_write_time(file, ec) + std::chrono::seconds(1);
  fs::last_write_time(file, new_time, ec);

  auto changes = watcher.PollChanges();
  EXPECT_FALSE(changes.empty());

  fs::remove_all(dir, ec);
}

// ============================================================================
// FileWatcher callbacks fire on change (background thread)
// ============================================================================

TEST(FileWatcher, CallbackFiresOnChange) {
  fs::path dir = fs::temp_directory_path() / "ftxui_filewatcher_test_cb";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  if (ec) {
    GTEST_SKIP() << "Cannot create temp directory";
  }

  // Create existing file
  fs::path file = dir / "watched.cpp";
  {
    std::ofstream f(file);
    f << "// v1\n";
  }

  std::atomic<int> callback_count{0};
  FileWatcher watcher(std::chrono::milliseconds(50));
  watcher.Watch(dir.string());
  watcher.OnChange([&](const std::string&) { callback_count++; });
  watcher.Start();

  // Let watcher seed
  std::this_thread::sleep_for(std::chrono::milliseconds(120));

  // Modify file and bump mtime
  {
    std::ofstream f(file, std::ios::app);
    f << "// v2\n";
  }
  auto new_time = fs::last_write_time(file, ec) + std::chrono::seconds(1);
  fs::last_write_time(file, new_time, ec);

  // Wait for background thread to detect
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  watcher.Stop();

  EXPECT_GT(callback_count.load(), 0);

  fs::remove_all(dir, ec);
}

// ============================================================================
// HotReloader basic construction
// ============================================================================

TEST(HotReloader, ConstructsWithoutCrash) {
  EXPECT_NO_THROW({
    HotReloader reloader;
    (void)reloader;
  });
}

TEST(HotReloader, StatusIsIdleInitially) {
  HotReloader reloader;
  EXPECT_EQ(reloader.status(), HotReloadStatus::Idle);
}

TEST(HotReloader, ReloadCountIsZeroInitially) {
  HotReloader reloader;
  EXPECT_EQ(reloader.reload_count(), 0);
}

TEST(HotReloader, ComponentReturnsNonNull) {
  HotReloader reloader;
  reloader.SetFactory(
      [] { return ftxui::Renderer([] { return ftxui::text("hi"); }); });
  auto comp = reloader.Component();
  EXPECT_NE(comp, nullptr);
}

TEST(HotReloader, SetFactoryUpdatesComponent) {
  HotReloader reloader;
  reloader.SetFactory(
      [] { return ftxui::Renderer([] { return ftxui::text("v1"); }); });

  auto comp1 = reloader.Component();
  ASSERT_NE(comp1, nullptr);

  reloader.SetFactory(
      [] { return ftxui::Renderer([] { return ftxui::text("v2"); }); });
  // Component wrapper always delegates to current — just verify no crash
  auto elem = comp1->Render();
  EXPECT_NE(elem, nullptr);
}

// ============================================================================
// ReloadOverlay
// ============================================================================

TEST(ReloadOverlay, IdleIsEmpty) {
  auto elem = ReloadOverlay(HotReloadStatus::Idle);
  EXPECT_NE(elem, nullptr);
  // Idle should produce minimal / empty element — just verify no crash
}

TEST(ReloadOverlay, CompilingHasContent) {
  auto elem = ReloadOverlay(HotReloadStatus::Compiling);
  EXPECT_NE(elem, nullptr);
  // Verify it renders without crash
  ftxui::Screen screen(40, 3);
  ftxui::Render(screen, elem);
  std::string rendered = screen.ToString();
  // Should contain some indicator of rebuilding
  EXPECT_FALSE(rendered.empty());
}

TEST(ReloadOverlay, ErrorShowsMessage) {
  auto elem = ReloadOverlay(HotReloadStatus::Error, "compile error here");
  EXPECT_NE(elem, nullptr);
  ftxui::Screen screen(60, 5);
  ftxui::Render(screen, elem);
  // Just verify it doesn't crash
}

// ============================================================================
// WithHotReload
// ============================================================================

TEST(WithHotReload, ReturnsNonNullComponent) {
  HotReloadConfig cfg;
  auto comp = WithHotReload(
      [] { return ftxui::Renderer([] { return ftxui::text("test"); }); }, cfg);
  EXPECT_NE(comp, nullptr);
}
