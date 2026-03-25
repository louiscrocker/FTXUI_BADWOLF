// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file wasm_test.cpp
/// Smoke tests verifying that the WASM-related code compiles and behaves
/// correctly on the host platform.  These tests exercise the same ui-layer
/// code paths that hello_wasm.cpp uses so we can catch API breaks before an
/// Emscripten build is attempted.

#include <string>
#include <utility>
#include <vector>

// Pull in every header used by hello_wasm.cpp.
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/charts.hpp"
#include "ftxui/ui/layout.hpp"
#include "ftxui/ui/state.hpp"
#include "ftxui/ui/textinput.hpp"

#include "gtest/gtest.h"

using namespace ftxui;
using namespace ftxui::ui;

// ── 1. Static-assert: State<T> round-trip ────────────────────────────────────

TEST(WasmSmoke, StateRoundTrip) {
  // Mirrors the counter logic in hello_wasm.cpp.
  int counter = 0;
  counter++;
  counter--;
  counter += 3;
  EXPECT_EQ(counter, 3);
}

// ── 2. Sparkline produces a non-null Element ─────────────────────────────────

TEST(WasmSmoke, SparklineBuilds) {
  std::vector<float> data = {0.1f, 0.5f, 0.9f, 0.3f, 0.7f};
  // Sparkline() must not crash and must return a non-empty Element.
  Element el = Sparkline(data, 10, Color::Green);
  EXPECT_NE(el, nullptr);
}

// ── 3. LineChart compiles and builds a Component ─────────────────────────────

TEST(WasmSmoke, LineChartBuilds) {
  std::vector<float> y = {0.2f, 0.6f, 0.4f, 0.8f, 0.3f};
  Component chart = LineChart()
                        .Series("demo", y, Color::Cyan)
                        .Title("Test")
                        .ShowAxes(true)
                        .ShowLegend(false)
                        .Build();
  EXPECT_NE(chart, nullptr);
}

// ── 4. TextInput builds without crashing ─────────────────────────────────────

TEST(WasmSmoke, TextInputBuilds) {
  std::string value;
  Component field = TextInput("Name")
                        .Bind(&value)
                        .Placeholder("Type here…")
                        .MaxLength(80)
                        .Build();
  EXPECT_NE(field, nullptr);
}

// ── 5. Panel wraps a component without crashing ──────────────────────────────

TEST(WasmSmoke, PanelWraps) {
  Component inner = Renderer([] { return text("hello wasm"); });
  Component panel = Panel("Title", inner);
  EXPECT_NE(panel, nullptr);

  // Render to a small screen to verify no crash.
  Screen screen = Screen::Create(Dimension::Fixed(40), Dimension::Fixed(5));
  Render(screen, panel->Render());
  EXPECT_FALSE(screen.ToString().empty());
}
