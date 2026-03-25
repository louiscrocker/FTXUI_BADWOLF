// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/notification.hpp"

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Internal types
// ────────────────────────────────────────────────────────────

struct NotificationEntry {
  std::string message;
  Severity severity;
  std::chrono::steady_clock::time_point creation_time;
  std::chrono::milliseconds duration;
};

// ── Singleton manager
// ─────────────────────────────────────────────────────────

class NotificationManager {
 public:
  static NotificationManager& Get() {
    static NotificationManager instance;
    return instance;
  }

  void Add(std::string_view message,
           Severity severity,
           std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back({std::string(message), severity,
                        std::chrono::steady_clock::now(), duration});
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
  }

  // Removes expired entries and returns a snapshot of what remains.
  std::vector<NotificationEntry> GetActive() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    for (auto it = entries_.begin(); it != entries_.end();) {
      if (it->duration.count() > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->creation_time);
        if (elapsed >= it->duration) {
          it = entries_.erase(it);
          continue;
        }
      }
      ++it;
    }
    return std::vector<NotificationEntry>(entries_.begin(), entries_.end());
  }

  bool HasActive() {
    std::lock_guard<std::mutex> lock(mutex_);
    return !entries_.empty();
  }

 private:
  NotificationManager() = default;
  std::mutex mutex_;
  std::deque<NotificationEntry> entries_;
};

// ── Public API
// ────────────────────────────────────────────────────────────────

void Notify(std::string_view message,
            Severity severity,
            std::chrono::milliseconds duration) {
  NotificationManager::Get().Add(message, severity, duration);
  if (auto* app = App::Active()) {
    app->RequestAnimationFrame();
  }
}

void ClearNotifications() {
  NotificationManager::Get().Clear();
}

// ── Rendering helpers
// ─────────────────────────────────────────────────────────

namespace {

Color SeverityColor(Severity severity) {
  const Theme& t = GetTheme();
  switch (severity) {
    case Severity::Info:
      return t.secondary;
    case Severity::Success:
      return t.accent;
    case Severity::Warning:
      return t.warning_color;
    case Severity::Error:
      return t.error_color;
    case Severity::Debug:
      return t.text_muted;
  }
  return t.secondary;
}

std::string SeverityLabel(Severity severity) {
  switch (severity) {
    case Severity::Info:
      return " INFO ";
    case Severity::Success:
      return " OK   ";
    case Severity::Warning:
      return " WARN ";
    case Severity::Error:
      return " ERR  ";
    case Severity::Debug:
      return " DBG  ";
  }
  return " INFO ";
}

Element RenderNotification(const NotificationEntry& entry) {
  Color c = SeverityColor(entry.severity);
  return hbox({
             text(SeverityLabel(entry.severity)) | bold | color(Color::Black) |
                 bgcolor(c),
             text(" " + entry.message + " "),
         }) |
         borderStyled(ROUNDED, c) | size(WIDTH, LESS_THAN, 40);
}

}  // namespace

// ── WithNotifications
// ─────────────────────────────────────────────────────────

Component WithNotifications(Component parent) {
  return Renderer(parent, [parent]() -> Element {
    auto entries = NotificationManager::Get().GetActive();

    if (entries.empty()) {
      return parent->Render();
    }

    // Keep requesting frames while notifications are visible for auto-dismiss.
    if (auto* app = App::Active()) {
      app->RequestAnimationFrame();
    }

    Elements boxes;
    boxes.reserve(entries.size());
    for (const auto& entry : entries) {
      boxes.push_back(RenderNotification(entry));
    }

    return dbox({
        parent->Render(),
        hbox({filler(), vbox(std::move(boxes))}),
    });
  });
}

ComponentDecorator WithNotifications() {
  return [](Component comp) -> Component {
    return WithNotifications(std::move(comp));
  };
}

}  // namespace ftxui::ui
