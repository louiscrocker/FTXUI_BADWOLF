// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file state_demo.cpp
/// Demonstrates ui::State<T> reactive wrappers and ui::RunAsync background
/// tasks.

#include <chrono>
#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;
using namespace std::chrono_literals;

int main() {
  SetTheme(Theme::Nord());

  // ── Reactive state ─────────────────────────────────────────────────────────
  State<int> counter(0);
  State<std::string> message("(press keys below)");
  State<bool> loading(false);
  State<std::string> async_result("(not started)");

  // ── Main view ──────────────────────────────────────────────────────────────
  auto comp = Renderer([&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               text("  ui::State<T> Demo  ") | bold | hcenter |
                   color(t.primary),
               separatorLight(),

               separatorEmpty(),
               hbox({text("Counter    : ") | dim,
                     text(std::to_string(*counter)) | bold | color(t.accent)}) |
                   hcenter,
               hbox({text("Message    : ") | dim,
                     text(*message) | color(t.secondary)}) |
                   hcenter,
               hbox({text("Loading    : ") | dim,
                     (*loading ? text("yes") | color(t.warning_color) | bold
                               : text("no") | dim)}) |
                   hcenter,
               hbox({text("Async result: ") | dim,
                     text(*async_result) | color(t.success_color)}) |
                   hcenter,
               separatorEmpty(),
               separatorLight(),

               vbox({
                   text("  + / -   increment / decrement counter") | dim,
                   text("  r       reset counter") | dim,
                   text("  m       mutate message via State::Mutate") | dim,
                   text("  a       launch background task (2s delay)") | dim,
                   text("  q       quit") | dim,
               }),
           }) |
           borderStyled(t.border_style, t.border_color) |
           size(WIDTH, EQUAL, 50) | center;
  });

  // ── Keybindings ────────────────────────────────────────────────────────────
  comp |= CatchEvent([&](Event event) -> bool {
    if (!event.is_character()) {
      return false;
    }
    const std::string& ch = event.character();

    if (ch == "+") {
      counter.Mutate([](int& v) { v++; });
      return true;
    }
    if (ch == "-") {
      counter.Mutate([](int& v) { v--; });
      return true;
    }
    if (ch == "r") {
      counter.Set(0);
      return true;
    }

    if (ch == "m") {
      int n = *counter;
      message.Set("Counter is " + std::to_string(n) +
                  (n % 2 == 0 ? " (even)" : " (odd)"));
      return true;
    }

    if (ch == "a") {
      if (!*loading) {
        loading.Set(true);
        async_result.Set("working…");
        RunAsync<std::string>(
            []() -> std::string {
              // Simulate 2-second background work.
              std::this_thread::sleep_for(2s);
              return "Done! (background thread result)";
            },
            [&](std::string result) {
              // Called back on the UI thread.
              async_result.Set(std::move(result));
              loading.Set(false);
            });
      }
      return true;
    }

    if (ch == "q") {
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    return false;
  });

  comp |= WithSpinner("Working…", loading.Ptr());

  App::FitComponent().Loop(comp);
  return 0;
}
