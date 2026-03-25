// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file hello_wasm.cpp
/// Minimal BadWolf WebAssembly demo.
///
/// Demonstrates the ftxui::ui API in a context that compiles cleanly for both
/// native and Emscripten targets.  When built with Emscripten the resulting
/// .js/.wasm files are loaded by wasm_shell.html.
///
/// Features shown:
///   - Centered titled panel  (ui::Panel)
///   - Counter with +/- buttons (ftxui::Button + ftxui::ui::State)
///   - Small LineChart with random seed data
///   - Single-line TextInput field

#include <array>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Deterministic pseudo-random chart data
// ────────────────────────────────────

static std::vector<float> SeedData() {
  // Simple deterministic sequence — avoids <random> header variability on
  // different platforms and keeps the demo reproducible in a browser.
  std::vector<float> data;
  data.reserve(20);
  float v = 0.5f;
  for (int i = 0; i < 20; ++i) {
    // Park-Miller-like one-liner
    v = v * 16807.0f;
    v -= static_cast<float>(static_cast<int>(v));  // fractional part
    data.push_back(0.2f + v * 0.6f);
  }
  return data;
}

// ── Entry point
// ───────────────────────────────────────────────────────────────

int main() {
  // ── Reactive state ──────────────────────────────────────────────
  int count = 0;
  std::string user_text;

  // Pre-computed chart data (static for the demo).
  const std::vector<float> chart_data = SeedData();

  // ── Counter row ─────────────────────────────────────────────────
  auto btn_dec = Button(" − ", [&] { --count; }, ButtonOption::Animated());
  auto btn_inc = Button(" + ", [&] { ++count; }, ButtonOption::Animated());

  auto counter_row = Renderer(
      Container::Horizontal({btn_dec, btn_inc}),
      [&](bool /*focused*/) -> Element {
        (void)false;  // suppress unused-param warning in older compilers
        return hbox({
                   btn_dec->Render(),
                   text("  Count: ") | bold,
                   text(std::to_string(count)) | color(Color::Cyan) | bold,
                   text("  "),
                   btn_inc->Render(),
               }) |
               center;
      });

  // ── Chart ───────────────────────────────────────────────────────
  auto chart = LineChart()
                   .Series("signal", chart_data, Color::Green)
                   .Title("Live Signal")
                   .ShowAxes(true)
                   .ShowLegend(false)
                   .Build();

  // ── Text input ──────────────────────────────────────────────────
  auto input_field = TextInput("Your name")
                         .Bind(&user_text)
                         .Placeholder("Type something…")
                         .MaxLength(60)
                         .Build();

  // ── Top-level layout ─────────────────────────────────────────────
  auto greeting = Renderer([&] {
    if (user_text.empty()) {
      return text("Hello, world!") | dim;
    }
    return hbox({text("Hello, ") | dim,
                 text(user_text) | bold | color(Color::Cyan), text("!") | dim});
  });

  auto inner = Container::Vertical({
      counter_row,
      chart,
      input_field,
      greeting,
  });

  auto content = Renderer(inner, [&](bool /*focused*/) -> Element {
    return vbox({
               text("BadWolf WASM Playground") | bold | center,
               separator(),
               counter_row->Render() | flex,
               separator(),
               text("Sparkline preview:") | dim,
               Sparkline(chart_data, 40, Color::Green),
               separator(),
               text("Chart:") | dim,
               chart->Render() | size(HEIGHT, EQUAL, 8),
               separator(),
               input_field->Render(),
               greeting->Render() | center,
           }) |
           flex;
  });

  auto panel = Panel("BadWolf WASM Playground", content);

  auto root = Renderer(panel, [&](bool /*focused*/) -> Element {
    return panel->Render() | center | flex;
  });

  // ── Run ─────────────────────────────────────────────────────────
  // In a real Emscripten build the FTXUI ScreenInteractive render loop
  // integrates with emscripten_set_main_loop via the browser event loop.
  // For native builds the loop runs normally.
  auto screen = ScreenInteractive::Fullscreen();
  screen.Loop(Container::Vertical({panel}));
  return 0;
}
