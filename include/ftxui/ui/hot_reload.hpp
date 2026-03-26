// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_HOT_RELOAD_HPP
#define FTXUI_UI_HOT_RELOAD_HPP

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

// ── HotReloadConfig
// ───────────────────────────────────────────────────────────
struct HotReloadConfig {
  std::vector<std::string> watch_paths;     // directories/files to watch
  std::string compiler = "g++";             // compiler command
  std::string flags = "-std=c++17 -O0 -g";  // compiler flags
  std::chrono::milliseconds debounce{
      300};                  // wait after change before recompile
  bool verbose = false;      // print rebuild output
  bool show_overlay = true;  // show "Rebuilding..." overlay
};

// ── HotReloadStatus
// ───────────────────────────────────────────────────────────
enum class HotReloadStatus {
  Idle,       // watching, no changes
  Detected,   // change detected, debouncing
  Compiling,  // recompile in progress
  Success,    // last rebuild succeeded
  Error,      // last rebuild failed
};

// ── FileWatcher
// ───────────────────────────────────────────────────────────────
/// Cross-platform file modification watcher (polling-based, no inotify/kqueue
/// dep)
class FileWatcher {
 public:
  explicit FileWatcher(
      std::chrono::milliseconds poll_interval = std::chrono::milliseconds(250));
  ~FileWatcher();

  void Watch(const std::string& path);  // file or directory (recursive)
  void Unwatch(const std::string& path);
  void Clear();

  // Returns list of changed files since last call
  std::vector<std::string> PollChanges();

  // Callback-based: calls fn on any change (from background thread)
  int OnChange(std::function<void(const std::string& changed_path)> fn);
  void RemoveOnChange(int id);

  void Start();
  void Stop();
  bool IsRunning() const;

 private:
  // Map from absolute file path → last known write time
  std::map<std::string, std::filesystem::file_time_type> file_times_;
  std::vector<std::string> watch_paths_;
  std::chrono::milliseconds poll_interval_;
  std::thread watch_thread_;
  std::atomic<bool> running_{false};
  mutable std::mutex mutex_;
  std::map<int, std::function<void(const std::string&)>> callbacks_;
  int next_id_{0};

  // Changed paths accumulated for PollChanges()
  std::vector<std::string> pending_changes_;

  void ScanPaths();
  void WatchLoop();
};

// ── HotReloader
// ───────────────────────────────────────────────────────────────
class HotReloader {
 public:
  explicit HotReloader(HotReloadConfig cfg = {});
  ~HotReloader();

  // Set the factory that creates the component (called on each reload)
  void SetFactory(std::function<ftxui::Component()> factory);

  // Start watching and rebuild on change
  void Start();
  void Stop();
  bool IsRunning() const;

  HotReloadStatus status() const;
  std::string last_error() const;
  int reload_count() const;

  // Returns a wrapper Component that auto-updates on reload
  ftxui::Component Component();

  // Callbacks
  int OnReload(std::function<void()> fn);
  int OnError(std::function<void(const std::string&)> fn);
  void RemoveCallback(int id);

 private:
  HotReloadConfig cfg_;
  FileWatcher watcher_;

  mutable std::mutex mutex_;
  std::function<ftxui::Component()> factory_;
  ftxui::Component current_component_;
  std::atomic<HotReloadStatus> status_{HotReloadStatus::Idle};
  std::string last_error_;
  std::atomic<int> reload_count_{0};
  std::atomic<bool> running_{false};

  // Reload/error callbacks
  std::map<int, std::function<void()>> reload_callbacks_;
  std::map<int, std::function<void(const std::string&)>> error_callbacks_;
  int next_cb_id_{0};

  // Debounce support
  std::thread debounce_thread_;
  std::atomic<bool> change_pending_{false};
  std::chrono::steady_clock::time_point last_change_time_;

  void OnFileChanged(const std::string& path);
  void TriggerReload();
};

// ── WithHotReload
// ─────────────────────────────────────────────────────────────
/// Wraps a component factory with hot reload support.
/// In non-debug builds (NDEBUG), just calls factory() and returns it.
/// In debug builds, wraps with HotReloader.
ftxui::Component WithHotReload(std::function<ftxui::Component()> factory,
                               HotReloadConfig cfg = {});

// ── DevApp
// ────────────────────────────────────────────────────────────────────
/// Development app runner with hot reload built in.
/// Usage: DevApp(argc, argv, []{ return MyComponent(); }, {"src/"});
void DevApp(int argc,
            char** argv,
            std::function<ftxui::Component()> factory,
            std::vector<std::string> watch_paths = {"src/", "include/"},
            HotReloadConfig cfg = {});

// ── ReloadOverlay
// ─────────────────────────────────────────────────────────────
/// Visual indicator shown during rebuild
ftxui::Element ReloadOverlay(HotReloadStatus status,
                             const std::string& error = "");

}  // namespace ftxui::ui

#endif  // FTXUI_UI_HOT_RELOAD_HPP
