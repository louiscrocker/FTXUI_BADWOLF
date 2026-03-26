// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/hot_reload.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

namespace fs = std::filesystem;

namespace ftxui::ui {

// ── FileWatcher
// ───────────────────────────────────────────────────────────────

FileWatcher::FileWatcher(std::chrono::milliseconds poll_interval)
    : poll_interval_(poll_interval) {}

FileWatcher::~FileWatcher() {
  Stop();
}

void FileWatcher::Watch(const std::string& path) {
  std::lock_guard<std::mutex> lock(mutex_);
  watch_paths_.push_back(path);
}

void FileWatcher::Unwatch(const std::string& path) {
  std::lock_guard<std::mutex> lock(mutex_);
  watch_paths_.erase(
      std::remove(watch_paths_.begin(), watch_paths_.end(), path),
      watch_paths_.end());
}

void FileWatcher::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  watch_paths_.clear();
  file_times_.clear();
  pending_changes_.clear();
}

std::vector<std::string> FileWatcher::PollChanges() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Run a synchronous scan and collect changes
  std::vector<std::string> changed;

  auto scan_file = [&](const fs::path& p) {
    std::error_code ec;
    auto mtime = fs::last_write_time(p, ec);
    if (ec) {
      return;
    }
    std::string key = p.string();
    auto it = file_times_.find(key);
    if (it == file_times_.end()) {
      file_times_[key] = mtime;
      // New file: treat as change
      changed.push_back(key);
    } else if (it->second != mtime) {
      it->second = mtime;
      changed.push_back(key);
    }
  };

  for (const auto& watch_path : watch_paths_) {
    std::error_code ec;
    fs::path p(watch_path);
    if (fs::is_directory(p, ec)) {
      for (auto& entry : fs::recursive_directory_iterator(p, ec)) {
        if (!entry.is_regular_file(ec)) {
          continue;
        }
        auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc" ||
            ext == ".cxx") {
          scan_file(entry.path());
        }
      }
    } else if (fs::is_regular_file(p, ec)) {
      scan_file(p);
    }
  }

  return changed;
}

int FileWatcher::OnChange(std::function<void(const std::string&)> fn) {
  std::lock_guard<std::mutex> lock(mutex_);
  int id = next_id_++;
  callbacks_[id] = std::move(fn);
  return id;
}

void FileWatcher::RemoveOnChange(int id) {
  std::lock_guard<std::mutex> lock(mutex_);
  callbacks_.erase(id);
}

void FileWatcher::Start() {
  if (running_.load()) {
    return;
  }
  running_ = true;
  watch_thread_ = std::thread([this] { WatchLoop(); });
}

void FileWatcher::Stop() {
  if (!running_.load()) {
    return;
  }
  running_ = false;
  if (watch_thread_.joinable()) {
    watch_thread_.join();
  }
}

bool FileWatcher::IsRunning() const {
  return running_.load();
}

void FileWatcher::ScanPaths() {
  // Called from WatchLoop (mutex NOT held — we hold it below per-operation)
  std::vector<std::string> paths_copy;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    paths_copy = watch_paths_;
  }

  auto scan_file = [&](const fs::path& p) {
    std::error_code ec;
    auto mtime = fs::last_write_time(p, ec);
    if (ec) {
      return;
    }
    std::string key = p.string();

    std::map<int, std::function<void(const std::string&)>> cbs_copy;
    bool changed = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = file_times_.find(key);
      if (it == file_times_.end()) {
        file_times_[key] = mtime;
        // First scan — record times, no change event
      } else if (it->second != mtime) {
        it->second = mtime;
        changed = true;
        pending_changes_.push_back(key);
        cbs_copy = callbacks_;
      }
    }

    if (changed) {
      for (auto& [id, cb] : cbs_copy) {
        cb(key);
      }
    }
  };

  for (const auto& watch_path : paths_copy) {
    std::error_code ec;
    fs::path p(watch_path);
    if (fs::is_directory(p, ec)) {
      for (auto& entry : fs::recursive_directory_iterator(p, ec)) {
        if (!running_.load()) {
          return;
        }
        if (!entry.is_regular_file(ec)) {
          continue;
        }
        auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc" ||
            ext == ".cxx") {
          scan_file(entry.path());
        }
      }
    } else if (fs::is_regular_file(p, ec)) {
      scan_file(p);
    }
  }
}

void FileWatcher::WatchLoop() {
  // First pass: seed file_times_ without triggering callbacks
  ScanPaths();

  while (running_.load()) {
    std::this_thread::sleep_for(poll_interval_);
    if (!running_.load()) {
      break;
    }
    ScanPaths();
  }
}

// ── HotReloader
// ───────────────────────────────────────────────────────────────

HotReloader::HotReloader(HotReloadConfig cfg) : cfg_(std::move(cfg)) {
  for (const auto& p : cfg_.watch_paths) {
    watcher_.Watch(p);
  }
}

HotReloader::~HotReloader() {
  Stop();
}

void HotReloader::SetFactory(std::function<ftxui::Component()> factory) {
  std::lock_guard<std::mutex> lock(mutex_);
  factory_ = std::move(factory);
  if (factory_) {
    current_component_ = factory_();
  }
}

void HotReloader::Start() {
  if (running_.load()) {
    return;
  }
  running_ = true;
  watcher_.OnChange([this](const std::string& path) { OnFileChanged(path); });
  watcher_.Start();
}

void HotReloader::Stop() {
  if (!running_.load()) {
    return;
  }
  running_ = false;
  watcher_.Stop();
  if (debounce_thread_.joinable()) {
    debounce_thread_.join();
  }
}

bool HotReloader::IsRunning() const {
  return running_.load();
}

HotReloadStatus HotReloader::status() const {
  return status_.load();
}

std::string HotReloader::last_error() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return last_error_;
}

int HotReloader::reload_count() const {
  return reload_count_.load();
}

ftxui::Component HotReloader::Component() {
  // Ensure we have a factory-produced component
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_component_ && factory_) {
      current_component_ = factory_();
    }
  }

  // Return a Renderer that always delegates to the latest current_component_
  return ftxui::Renderer([this]() -> ftxui::Element {
    ftxui::Component comp;
    HotReloadStatus s = status_.load();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      comp = current_component_;
    }

    ftxui::Element main_elem =
        comp ? comp->Render() : ftxui::text("(no component)");

    if (cfg_.show_overlay && s != HotReloadStatus::Idle &&
        s != HotReloadStatus::Success) {
      std::string err;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        err = last_error_;
      }
      return ftxui::dbox({main_elem, ReloadOverlay(s, err)});
    }
    return main_elem;
  });
}

int HotReloader::OnReload(std::function<void()> fn) {
  std::lock_guard<std::mutex> lock(mutex_);
  int id = next_cb_id_++;
  reload_callbacks_[id] = std::move(fn);
  return id;
}

int HotReloader::OnError(std::function<void(const std::string&)> fn) {
  std::lock_guard<std::mutex> lock(mutex_);
  int id = next_cb_id_++;
  error_callbacks_[id] = std::move(fn);
  return id;
}

void HotReloader::RemoveCallback(int id) {
  std::lock_guard<std::mutex> lock(mutex_);
  reload_callbacks_.erase(id);
  error_callbacks_.erase(id);
}

void HotReloader::OnFileChanged(const std::string& /*path*/) {
  status_ = HotReloadStatus::Detected;
  last_change_time_ = std::chrono::steady_clock::now();
  change_pending_ = true;

  // Kick off / restart debounce thread
  if (debounce_thread_.joinable()) {
    debounce_thread_.detach();
  }
  debounce_thread_ = std::thread([this] {
    std::this_thread::sleep_for(cfg_.debounce);
    if (change_pending_.load()) {
      change_pending_ = false;
      TriggerReload();
    }
  });
}

void HotReloader::TriggerReload() {
  status_ = HotReloadStatus::Compiling;

  // Within-process reload: re-invoke the factory and replace current component
  std::function<ftxui::Component()> factory_copy;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    factory_copy = factory_;
  }

  if (!factory_copy) {
    status_ = HotReloadStatus::Error;
    std::lock_guard<std::mutex> lock(mutex_);
    last_error_ = "No factory set";
    return;
  }

  try {
    auto new_comp = factory_copy();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      current_component_ = std::move(new_comp);
      last_error_.clear();
    }
    reload_count_++;
    status_ = HotReloadStatus::Success;

    // Fire reload callbacks
    std::map<int, std::function<void()>> cbs;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      cbs = reload_callbacks_;
    }
    for (auto& [id, cb] : cbs) {
      cb();
    }
  } catch (const std::exception& e) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      last_error_ = e.what();
    }
    status_ = HotReloadStatus::Error;

    std::map<int, std::function<void(const std::string&)>> ecbs;
    std::string err;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ecbs = error_callbacks_;
      err = last_error_;
    }
    for (auto& [id, cb] : ecbs) {
      cb(err);
    }
  }
}

// ── ReloadOverlay
// ─────────────────────────────────────────────────────────────

ftxui::Element ReloadOverlay(HotReloadStatus status, const std::string& error) {
  using namespace ftxui;
  switch (status) {
    case HotReloadStatus::Idle:
      return ftxui::text("");

    case HotReloadStatus::Detected:
      return ftxui::hbox({
          ftxui::text(" ● Change detected... ") | ftxui::color(Color::Yellow) |
              ftxui::dim,
      });

    case HotReloadStatus::Compiling:
      return ftxui::hbox({
          ftxui::spinner(6, 0) | ftxui::color(Color::Blue),
          ftxui::text(" ⟳ Rebuilding...") | ftxui::color(Color::Blue),
      });

    case HotReloadStatus::Success:
      return ftxui::hbox({
          ftxui::text(" ✓ Rebuilt") | ftxui::color(Color::Green),
      });

    case HotReloadStatus::Error:
      return ftxui::vbox({
                 ftxui::text(" ✗ Build error:") | ftxui::color(Color::Red) |
                     ftxui::bold,
                 ftxui::paragraph(error) | ftxui::color(Color::Red),
             }) |
             ftxui::border;
  }
  return ftxui::text("");
}

// ── WithHotReload
// ─────────────────────────────────────────────────────────────

ftxui::Component WithHotReload(std::function<ftxui::Component()> factory,
                               HotReloadConfig cfg) {
#ifdef NDEBUG
  return factory();
#else
  auto reloader = std::make_shared<HotReloader>(std::move(cfg));
  reloader->SetFactory(factory);
  reloader->Start();

  // Return a Renderer that delegates to the current component, keeping
  // reloader alive via capture.
  return ftxui::Renderer([reloader]() -> ftxui::Element {
    auto comp = reloader->Component();
    return comp ? comp->Render() : ftxui::text("(no component)");
  });
#endif
}

// ── DevApp
// ────────────────────────────────────────────────────────────────────

void DevApp(int argc,
            char** argv,
            std::function<ftxui::Component()> factory,
            std::vector<std::string> watch_paths,
            HotReloadConfig cfg) {
  cfg.watch_paths = std::move(watch_paths);
  cfg.show_overlay = true;

  auto reloader = std::make_shared<HotReloader>(cfg);
  reloader->SetFactory(factory);
  reloader->Start();

  auto content = reloader->Component();

  // Status bar showing reload count and current status
  auto status_bar = ftxui::Renderer([reloader]() -> ftxui::Element {
    HotReloadStatus s = reloader->status();
    int count = reloader->reload_count();

    std::string status_str;
    ftxui::Color status_color = ftxui::Color::White;
    switch (s) {
      case HotReloadStatus::Idle:
        status_str = "● watching";
        status_color = ftxui::Color::GrayDark;
        break;
      case HotReloadStatus::Detected:
        status_str = "● change detected";
        status_color = ftxui::Color::Yellow;
        break;
      case HotReloadStatus::Compiling:
        status_str = "⟳ rebuilding";
        status_color = ftxui::Color::Blue;
        break;
      case HotReloadStatus::Success:
        status_str = "✓ rebuilt";
        status_color = ftxui::Color::Green;
        break;
      case HotReloadStatus::Error:
        status_str = "✗ error";
        status_color = ftxui::Color::Red;
        break;
    }

    return ftxui::hbox({
               ftxui::text(" 🔥 BadWolf Dev Mode ") | ftxui::bold,
               ftxui::separator(),
               ftxui::text(" reloads: " + std::to_string(count) + " "),
               ftxui::separator(),
               ftxui::text(" " + status_str + " ") | ftxui::color(status_color),
               ftxui::filler(),
               ftxui::text(" --dev ") | ftxui::dim,
           }) |
           ftxui::bgcolor(ftxui::Color::Black);
  });

  auto root = ftxui::Container::Vertical({});
  auto layout = ftxui::Renderer(root, [content, status_bar]() {
    return ftxui::vbox({
        content->Render() | ftxui::flex,
        ftxui::separator(),
        status_bar->Render(),
    });
  });

  auto screen = ftxui::ScreenInteractive::Fullscreen();
  screen.Loop(layout);
}

}  // namespace ftxui::ui
