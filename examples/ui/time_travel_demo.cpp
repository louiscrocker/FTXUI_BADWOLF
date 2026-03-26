// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file time_travel_demo.cpp
/// Demonstrates the Time-Travel Debugger: tracks reactive state changes and
/// lets you scrub through history with F12.

#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/reactive.hpp"
#include "ftxui/ui/time_travel.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  // ── Reactive state ─────────────────────────────────────────────────────────
  auto counter = std::make_shared<Reactive<int>>(0);
  auto username = std::make_shared<Reactive<std::string>>("guest");
  auto theme_name = std::make_shared<Reactive<std::string>>("default");

  // Track all state changes
  StateRecorder::Global().Track<int>(counter, "counter",
                                     [](const int& v) { return JsonValue(v); });
  StateRecorder::Global().Track<std::string>(
      username, "username", [](const std::string& v) { return JsonValue(v); });
  StateRecorder::Global().Track<std::string>(
      theme_name, "theme", [](const std::string& v) { return JsonValue(v); });

  // ── Username input ─────────────────────────────────────────────────────────
  std::string input_buf = username->Get();
  auto user_input =
      Input(&input_buf, "Enter username") | CatchEvent([&](Event ev) {
        if (ev == Event::Return || ev == Event::Tab) {
          username->Set(input_buf);
          return true;
        }
        return false;
      });

  // ── Theme cycle ────────────────────────────────────────────────────────────
  const std::vector<std::string> themes = {"default", "nord", "solarized",
                                           "gruvbox", "dracula"};
  int theme_idx = 0;
  auto btn_theme = Button(
      "Cycle Theme",
      [&] {
        theme_idx = (theme_idx + 1) % static_cast<int>(themes.size());
        theme_name->Set(themes[static_cast<size_t>(theme_idx)]);
      },
      ButtonOption::Animated());

  // ── Counter buttons ────────────────────────────────────────────────────────
  auto btn_inc = Button(
      " + ", [&] { counter->Set(counter->Get() + 1); },
      ButtonOption::Animated());
  auto btn_dec = Button(
      " - ", [&] { counter->Set(counter->Get() - 1); },
      ButtonOption::Animated());
  auto btn_reset =
      Button("Reset", [&] { counter->Set(0); }, ButtonOption::Animated());

  auto controls =
      Container::Horizontal({btn_dec, btn_inc, btn_reset, btn_theme});
  auto app_container = Container::Vertical({controls, user_input});

  auto app_ui = Renderer(app_container, [&]() -> Element {
    return window(
        text(" ⏱ Time-Travel Demo — press F12 to toggle debugger "),
        vbox({
            hbox({text(" Counter: "), text(std::to_string(counter->Get())) |
                                          bold | color(Color::Cyan)}),
            hbox({text(" Username: "),
                  text(username->Get()) | color(Color::Green)}),
            hbox({text(" Theme: "),
                  text(theme_name->Get()) | color(Color::Magenta)}),
            separator(),
            hbox({btn_dec->Render(), text(" "), btn_inc->Render(), text("  "),
                  btn_reset->Render(), text("  "), btn_theme->Render()}),
            separator(),
            hbox({text(" User: "), user_input->Render() | flex}),
            text("") | dim,
            text(" ← → Home End Space to navigate timeline") | dim,
        }));
  });

  // Wrap with time-travel debugger (F12 toggle)
  auto root = WithTimeTravel(app_ui);

  auto screen = ScreenInteractive::Fullscreen();
  screen.Loop(root);
  return 0;
}
