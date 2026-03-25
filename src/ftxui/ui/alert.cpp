// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/alert.hpp"

#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── AlertSystem singleton ─────────────────────────────────────────────────────

AlertSystem::AlertSystem() = default;

AlertSystem& AlertSystem::Instance() {
  static AlertSystem instance;
  return instance;
}

namespace {
void PostUIRefresh() {
  if (App* app = App::Active()) {
    app->PostEvent(Event::Custom);
  }
}
}  // namespace

void AlertSystem::Red(std::string message) {
  std::vector<std::function<void(AlertLevel, std::string)>> snapshot;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = AlertLevel::Red;
    message_ = std::move(message);
    snapshot = listeners_;
  }
  for (auto& fn : snapshot) {
    fn(AlertLevel::Red, message_);
  }
  PostUIRefresh();
}

void AlertSystem::Yellow(std::string message) {
  std::vector<std::function<void(AlertLevel, std::string)>> snapshot;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = AlertLevel::Yellow;
    message_ = std::move(message);
    snapshot = listeners_;
  }
  for (auto& fn : snapshot) {
    fn(AlertLevel::Yellow, message_);
  }
  PostUIRefresh();
}

void AlertSystem::Blue(std::string message) {
  std::vector<std::function<void(AlertLevel, std::string)>> snapshot;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = AlertLevel::Blue;
    message_ = std::move(message);
    snapshot = listeners_;
  }
  for (auto& fn : snapshot) {
    fn(AlertLevel::Blue, message_);
  }
  PostUIRefresh();
}

void AlertSystem::AllClear() {
  std::vector<std::function<void(AlertLevel, std::string)>> snapshot;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = AlertLevel::AllClear;
    message_ = "";
    snapshot = listeners_;
  }
  for (auto& fn : snapshot) {
    fn(AlertLevel::AllClear, "");
  }
  PostUIRefresh();
}

AlertLevel AlertSystem::Level() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return level_;
}

std::string AlertSystem::Message() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return message_;
}

void AlertSystem::OnChange(std::function<void(AlertLevel, std::string)> fn) {
  std::lock_guard<std::mutex> lock(mutex_);
  listeners_.push_back(std::move(fn));
}

// ── WithAlertOverlay ──────────────────────────────────────────────────────────

Component AlertSystem::WithAlertOverlay(Component inner) {
  return Renderer(inner, [inner]() -> Element {
    const Theme& t = GetTheme();
    AlertLevel level = AlertSystem::Instance().Level();
    std::string msg  = AlertSystem::Instance().Message();

    Element rendered = inner->Render();

    switch (level) {
      case AlertLevel::Red: {
        auto header = text(" \U0001F6A8 " + msg + " \U0001F6A8 ") | blink |
                      bold | color(t.error_color);
        return vbox({header | hcenter, rendered | flex}) |
               borderStyled(HEAVY, t.error_color);
      }
      case AlertLevel::Yellow: {
        auto header = text(" \u26A0 " + msg) | bold | color(t.warning_color);
        return vbox({header | hcenter, rendered | flex}) |
               borderStyled(ROUNDED, t.warning_color);
      }
      case AlertLevel::Blue: {
        auto header = text(" \u2139 " + msg) | color(t.secondary);
        return vbox({header | hcenter, rendered | flex}) |
               borderStyled(ROUNDED, t.secondary);
      }
      case AlertLevel::AllClear:
        return rendered;
    }
    return rendered;
  });
}

// ── Free function ─────────────────────────────────────────────────────────────

Component WithAlertOverlay(Component inner) {
  return AlertSystem::Instance().WithAlertOverlay(std::move(inner));
}

}  // namespace ftxui::ui
