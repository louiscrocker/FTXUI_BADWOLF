// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/log_panel.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Impl ──────────────────────────────────────────────────────────────────────

struct LogPanel::Impl {
  std::deque<Entry> entries;
  std::mutex mutex;
  size_t max_lines = 1000;
  LogLevel min_level = LogLevel::Trace;
};

// ── Timestamp helper ──────────────────────────────────────────────────────────

namespace {

std::string CurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_info = {};
#ifdef _WIN32
  localtime_s(&tm_info, &t);
#else
  localtime_r(&t, &tm_info);
#endif
  char buf[9];  // HH:MM:SS\0
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm_info.tm_hour,
                tm_info.tm_min, tm_info.tm_sec);
  return buf;
}

int LevelValue(LogLevel level) {
  switch (level) {
    case LogLevel::Trace: return 0;
    case LogLevel::Debug: return 1;
    case LogLevel::Info:  return 2;
    case LogLevel::Warn:  return 3;
    case LogLevel::Error: return 4;
  }
  return 0;
}

const char* LevelLabel(LogLevel level) {
  switch (level) {
    case LogLevel::Trace: return "[TRC]";
    case LogLevel::Debug: return "[DBG]";
    case LogLevel::Info:  return "[INF]";
    case LogLevel::Warn:  return "[WRN]";
    case LogLevel::Error: return "[ERR]";
  }
  return "[INF]";
}

Element LevelElement(LogLevel level, const Theme& t) {
  const char* label = LevelLabel(level);
  switch (level) {
    case LogLevel::Trace:
    case LogLevel::Debug:
      return text(label) | color(t.text_muted) | dim;
    case LogLevel::Info:
      return text(label) | color(t.secondary);
    case LogLevel::Warn:
      return text(label) | color(t.warning_color);
    case LogLevel::Error:
      return text(label) | color(t.error_color) | bold;
  }
  return text(label);
}

Element MessageElement(LogLevel level, const std::string& message,
                       const Theme& t) {
  switch (level) {
    case LogLevel::Trace:
    case LogLevel::Debug:
      return text(" " + message) | color(t.text_muted) | dim;
    case LogLevel::Info:
      return text(" " + message) | color(t.text);
    case LogLevel::Warn:
      return text(" " + message) | color(t.warning_color);
    case LogLevel::Error:
      return text(" " + message) | color(t.error_color) | bold;
  }
  return text(" " + message);
}

}  // namespace

// ── Factory ───────────────────────────────────────────────────────────────────

std::shared_ptr<LogPanel> LogPanel::Create(size_t max_lines) {
  auto panel = std::shared_ptr<LogPanel>(new LogPanel());
  panel->impl_ = std::make_shared<Impl>();
  panel->impl_->max_lines = max_lines;
  return panel;
}

// ── Logging ───────────────────────────────────────────────────────────────────

void LogPanel::Log(std::string_view message, LogLevel level) {
  auto impl = impl_;
  std::lock_guard<std::mutex> lock(impl->mutex);
  impl->entries.push_back(
      {std::string(message), level, CurrentTimestamp()});
  while (impl->entries.size() > impl->max_lines) {
    impl->entries.pop_front();
  }
}

void LogPanel::Trace(std::string_view message) { Log(message, LogLevel::Trace); }
void LogPanel::Debug(std::string_view message) { Log(message, LogLevel::Debug); }
void LogPanel::Info(std::string_view message)  { Log(message, LogLevel::Info); }
void LogPanel::Warn(std::string_view message)  { Log(message, LogLevel::Warn); }
void LogPanel::Error(std::string_view message) { Log(message, LogLevel::Error); }

// ── Management ────────────────────────────────────────────────────────────────

void LogPanel::Clear() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->entries.clear();
}

void LogPanel::SetMaxLines(size_t n) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->max_lines = n;
  while (impl_->entries.size() > n) {
    impl_->entries.pop_front();
  }
}

void LogPanel::SetMinLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->min_level = level;
}

// ── Build ─────────────────────────────────────────────────────────────────────

Component LogPanel::Build(std::string_view title) const {
  auto impl = impl_;
  std::string title_str{title};

  return Renderer([impl, title_str]() -> Element {
    // Snapshot entries under lock.
    std::vector<Entry> snapshot;
    LogLevel min_level;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      snapshot.assign(impl->entries.begin(), impl->entries.end());
      min_level = impl->min_level;
    }

    const Theme& t = GetTheme();

    // Build line elements, filtered by min_level.
    Elements lines;
    lines.reserve(snapshot.size());
    for (const auto& entry : snapshot) {
      if (LevelValue(entry.level) < LevelValue(min_level)) {
        continue;
      }
      Element ts = text(entry.timestamp + " ") | dim | color(t.text_muted);
      Element badge = LevelElement(entry.level, t);
      Element msg = MessageElement(entry.level, entry.message, t);
      lines.push_back(hbox({ts, badge, msg}));
    }

    // Focus on the last element to auto-scroll to bottom.
    if (!lines.empty()) {
      lines.back() = lines.back() | focus;
    }

    Element log_body =
        vbox(std::move(lines)) | vscroll_indicator | yframe | flex;

    if (title_str.empty()) {
      return log_body;
    }

    return window(text(" " + title_str + " "), std::move(log_body),
                  t.border_style);
  });
}

}  // namespace ftxui::ui
