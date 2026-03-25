// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file progress_demo.cpp
/// Demonstrates ui::ThemedProgressBar and ui::WithSpinner.

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

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

  // ── State ──────────────────────────────────────────────────────────────────
  State<float> upload(0.f);
  State<float> download(0.f);
  State<float> processing(0.f);
  State<bool> spinner_visible(false);
  State<std::string> status("Idle");

  // ── Progress bars ──────────────────────────────────────────────────────────
  auto bar_upload = ThemedProgressBar(upload.Ptr(), "Upload  ");
  auto bar_download = ThemedProgressBar(download.Ptr(), "Download");
  auto bar_processing = ThemedProgressBar(processing.Ptr(), "Process ");

  auto progress_section =
      Container::Vertical({bar_upload, bar_download, bar_processing});

  // ── Main layout ────────────────────────────────────────────────────────────
  auto comp = Renderer(progress_section, [&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               text("  Progress & Spinner Demo  ") | bold | hcenter |
                   color(t.primary),
               separatorLight(),
               separatorEmpty(),
               bar_upload->Render() | size(WIDTH, GREATER_THAN, 40),
               separatorEmpty(),
               bar_download->Render() | size(WIDTH, GREATER_THAN, 40),
               separatorEmpty(),
               bar_processing->Render() | size(WIDTH, GREATER_THAN, 40),
               separatorEmpty(),
               separatorLight(),
               hbox({text("Status : ") | dim, text(*status) | bold}) | hcenter,
               separatorEmpty(),
               vbox({
                   text("  s  → simulate progress (resets then runs)") | dim,
                   text("  x  → toggle spinner overlay") | dim,
                   text("  q  → quit") | dim,
               }),
           }) |
           borderStyled(t.border_style, t.border_color) |
           size(WIDTH, EQUAL, 52) | center;
  });

  // ── Spinner overlay ────────────────────────────────────────────────────────
  comp |= WithSpinner("Simulating…", spinner_visible.Ptr());

  // ── Keybindings ────────────────────────────────────────────────────────────
  comp |= CatchEvent([&](Event event) -> bool {
    if (!event.is_character()) {
      return false;
    }
    const auto& ch = event.character();

    if (ch == "s") {
      upload.Set(0.f);
      download.Set(0.f);
      processing.Set(0.f);
      spinner_visible.Set(true);
      status.Set("Running…");

      RunAsync<bool>(
          [&]() -> bool {
            for (int i = 0; i <= 100; ++i) {
              upload.Set(i / 100.f);
              std::this_thread::sleep_for(20ms);
            }
            for (int i = 0; i <= 100; ++i) {
              download.Set(i / 100.f);
              std::this_thread::sleep_for(15ms);
            }
            for (int i = 0; i <= 100; ++i) {
              processing.Set(i / 100.f);
              std::this_thread::sleep_for(10ms);
            }
            return true;
          },
          [&](bool) {
            spinner_visible.Set(false);
            status.Set("Complete!");
          });
      return true;
    }

    if (ch == "x") {
      spinner_visible.Mutate([](bool& v) { v = !v; });
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

  App::FitComponent().Loop(comp);
  return 0;
}
