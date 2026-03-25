// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file webgl_demo.cpp
/// WebGL hybrid renderer showcase.
///
/// Controls:
///   w  – toggle WebGL / terminal rendering mode
///   b  – trigger particle explosion
///   g  – toggle globe auto-spin
///   q  – quit

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/particles.hpp"
#include "ftxui/ui/webgl_renderer.hpp"
#include "ftxui/ui/wasm_bridge.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  // WebGL is a no-op on native; this line is safe everywhere.
  bool webgl_active  = WebGLRenderer::Init("badwolf-canvas");
  bool webgl_enabled = webgl_active;
  bool globe_spin    = true;

  // ── Particle system (terminal rendering) ─────────────────────────────────
  auto ps = Explosion(40.0f, 20.0f);

  // ── Globe ─────────────────────────────────────────────────────────────────
  Component globe = GlobeMap()
      .LineColor(Color::GreenLight)
      .RotationSpeed(0.8)
      .ShowGraticule(true)
      .GraticuleStep(30.0f)
      .Build();

  // ── Status panel ──────────────────────────────────────────────────────────
  auto status_line = [&]() -> Element {
    std::string mode_str;
    if (webgl_active && webgl_enabled) {
      mode_str = "WebGL: ACTIVE (GPU)";
    } else if (webgl_active) {
      mode_str = "WebGL: available (terminal mode)";
    } else {
      mode_str = "WebGL: unavailable (CPU fallback)";
    }

    auto stats = WebGLRenderer::GetStats();
    std::string stats_str = " | quads/frame: " +
        std::to_string(stats.quads_rendered) +
        "  draw_calls: " + std::to_string(stats.draw_calls) +
        "  frame: " + std::to_string(static_cast<int>(stats.frame_ms)) + "ms";

    return hbox({
        text(mode_str)  | color(webgl_active && webgl_enabled
                                    ? Color::GreenLight : Color::YellowLight),
        text(stats_str) | color(Color::GrayLight),
    });
  };

  // ── Controls info ─────────────────────────────────────────────────────────
  auto controls = [] {
    return hbox({
        text("[w] toggle WebGL  ") | color(Color::Cyan),
        text("[b] explosion  ")    | color(Color::Cyan),
        text("[g] globe spin  ")   | color(Color::Cyan),
        text("[q] quit")           | color(Color::Cyan),
    });
  };

  // ── Root component ────────────────────────────────────────────────────────
  int frame_counter = 0;
  auto renderer = Renderer(globe, [&] {
    ++frame_counter;

    // If WebGL is enabled, pump a WebGL frame.
    // (All WebGL calls are no-ops on native — they compile cleanly.)
    if (webgl_active && webgl_enabled) {
      WebGLRenderer::Clear(0, 0, 0);
      // Build a simple animated dot pattern to demonstrate braille rendering.
      constexpr int W = 200, H = 100;
      std::vector<std::vector<bool>> dots(H, std::vector<bool>(W, false));
      for (int i = 0; i < 100; ++i) {
        float angle = static_cast<float>(frame_counter) * 0.05f +
                      static_cast<float>(i) * 0.3f;
        int col = static_cast<int>((std::cos(angle) * 0.4f + 0.5f) * W);
        int row = static_cast<int>((std::sin(angle) * 0.4f + 0.5f) * H);
        if (col >= 0 && col < W && row >= 0 && row < H) dots[row][col] = true;
      }
      WebGLRenderer::RenderBrailleCanvas(dots, W, H, 255, 200, 50);
      WebGLRenderer::Present();
    }

    // Always render a terminal frame.
    auto canvas_el = canvas(80, 40, [&](Canvas& c) {
      ps->Render(c);
    });

    return vbox({
        text("── WebGL Hybrid Renderer Demo ──") | bold | color(Color::Green),
        status_line(),
        separatorEmpty(),
        canvas_el | flex,
        globe->Render() | flex,
        separatorEmpty(),
        controls(),
    });
  });

  auto root = CatchEvent(renderer, [&](Event e) {
    if (e == Event::Character('q')) {
      App::Active()->Exit();
      return true;
    }
    if (e == Event::Character('w')) {
      webgl_enabled = !webgl_enabled;
      return true;
    }
    if (e == Event::Character('b')) {
      ps->SetPosition(40.0f, 20.0f);
      ps->Start();
      return true;
    }
    if (e == Event::Character('g')) {
      globe_spin = !globe_spin;
      return true;
    }
    return false;
  });

  RunApp([&root] { return root; });

  if (webgl_active) WebGLRenderer::Shutdown();
  return 0;
}
