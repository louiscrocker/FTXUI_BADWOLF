// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file animation_demo.cpp
///
/// Demonstrates the FTXUI animation engine and braille particle system.
///
/// Controls:
///   w  — toggle warp streaks
///   r  — toggle rain
///   e  — trigger explosion at centre
///   q  — quit

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/animation.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/particles.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  // Target 60 fps.
  AnimationLoop::Instance().SetFPS(60);

  // ── Tweens ──────────────────────────────────────────────────────────────

  // Central panel fades in on startup.
  auto fade_tween = Animate(0.0f, 1.0f, 0.8f, Easing::EaseOut);

  // Progress gauge bounces 0 → 1 repeatedly.
  auto gauge_tween =
      std::make_shared<Tween>(0.0f, 1.0f, 2.0f, Easing::Bounce);
  gauge_tween->OnComplete([&] {
    gauge_tween->Reset();
    gauge_tween->Start();
    AnimationLoop::Instance().Add(gauge_tween);
  });
  AnimationLoop::Instance().Add(gauge_tween);
  gauge_tween->Start();

  // ── Particle systems ─────────────────────────────────────────────────────

  auto warp = WarpStreaks();
  auto rain = Rain();
  std::vector<std::shared_ptr<ParticleSystem>> explosions;
  std::atomic<bool> warp_on{true};
  std::atomic<bool> rain_on{false};

  warp->Start();

  // ── Canvas size state (updated each render) ──────────────────────────────
  std::atomic<int> canvas_w{160};
  std::atomic<int> canvas_h{80};

  // ── Renderer ─────────────────────────────────────────────────────────────

  auto particle_canvas = Renderer([&]() -> Element {
    int w = canvas_w.load();
    int h = canvas_h.load();

    // Update warp emitter position so streaks start from the left edge.
    warp->SetPosition(0.0f, static_cast<float>(h) / 2.0f);
    // Update rain emitter to span the top.
    rain->SetPosition(static_cast<float>(w) / 2.0f, 0.0f);

    return canvas(2, 4, [&](Canvas& c) {
      warp->Render(c);
      rain->Render(c);
      for (auto& ex : explosions) {
        ex->Render(c);
      }
    }) | flex;
  });

  // ── Central info panel ────────────────────────────────────────────────────

  auto info_panel = Renderer([&]() -> Element {
    float fade = fade_tween->Value();
    float gauge_val = gauge_tween->Value();

    std::string warp_label = warp_on.load() ? "[ON]" : "[off]";
    std::string rain_label = rain_on.load() ? "[ON]" : "[off]";

    auto panel = vbox({
        text("  ✦  Animation Demo  ✦") | bold | color(Color::Cyan) | hcenter,
        separator(),
        hbox({text("Warp streaks: ") | color(Color::White),
              text(warp_label) | color(Color::Yellow)}),
        hbox({text("Rain:         ") | color(Color::White),
              text(rain_label) | color(Color::Blue)}),
        separator(),
        text("Bouncing gauge:") | color(Color::GrayLight),
        gauge(gauge_val) | color(Color::GreenLight),
        separator(),
        text("[w] warp  [r] rain  [e] explosion  [q] quit") |
            color(Color::GrayDark),
    }) | border;

    // Apply dim when still fading in.
    if (fade < 0.7f) {
      return panel | dim;
    }
    return panel;
  });

  // ── Root layout ──────────────────────────────────────────────────────────

  auto root = Renderer([&]() -> Element {
    return dbox({
        particle_canvas->Render() | flex,
        vbox({
            filler(),
            hbox({filler(), info_panel->Render(), filler()}),
            filler(),
        }) | flex,
    });
  });

  // ── Event handling ────────────────────────────────────────────────────────

  auto app_comp = CatchEvent(root, [&](Event event) -> bool {
    if (event == Event::Character('q') || event == Event::Character('Q')) {
      if (App* a = App::Active()) a->Exit();
      return true;
    }
    if (event == Event::Character('w') || event == Event::Character('W')) {
      if (warp_on.load()) {
        warp->Stop();
        warp_on.store(false);
      } else {
        warp->Start();
        warp_on.store(true);
      }
      return true;
    }
    if (event == Event::Character('r') || event == Event::Character('R')) {
      if (rain_on.load()) {
        rain->Stop();
        rain_on.store(false);
      } else {
        rain->Start();
        rain_on.store(true);
      }
      return true;
    }
    if (event == Event::Character('e') || event == Event::Character('E')) {
      int w = canvas_w.load();
      int h = canvas_h.load();
      auto ex = Explosion(static_cast<float>(w) / 2.0f,
                          static_cast<float>(h) / 2.0f);
      ex->Start();
      explosions.push_back(ex);
      // Keep explosion list small — drop finished ones.
      explosions.erase(
          std::remove_if(explosions.begin(), explosions.end(),
                         [](const std::shared_ptr<ParticleSystem>& e) {
                           return e->LiveCount() == 0;
                         }),
          explosions.end());
      return true;
    }
    return false;
  });

  RunFullscreen(app_comp);

  // Clean up: stop all particle systems.
  warp->Stop();
  rain->Stop();
  for (auto& ex : explosions) ex->Stop();

  return 0;
}
