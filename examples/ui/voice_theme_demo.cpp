// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file voice_theme_demo.cpp
/// @brief FTXUI BadWolf — Live Theme Studio
///
/// Full-screen demo combining voice-activated theme switching, a live
/// ThemeStage preview and a bottom LiveThemeBar.
///
/// Keys:
///   1-9   — apply a preset theme
///   V     — toggle voice listening
///   S / F12 — showcase mode (auto-cycle)
///   Q / Escape — quit

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/ui/voice_theme.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::LCARS());

  // ── Shared controller ────────────────────────────────────────────────────
  auto ctrl = std::make_shared<VoiceThemeController>();

  // ── Preset theme buttons ─────────────────────────────────────────────────
  struct Preset {
    std::string label;
    std::string key;
  };
  const std::vector<Preset> presets = {
      {"LCARS", "lcars"},           {"Matrix", "matrix"},
      {"Imperial", "imperial"},     {"Rebel", "rebel"},
      {"Enterprise", "enterprise"}, {"Dark", "dark"},
      {"Light", "light"},           {"Nord", "nord"},
      {"Dracula", "dracula"},
  };

  std::vector<Component> preset_buttons;
  for (const auto& p : presets) {
    auto btn = Button(
        p.label, [ctrl, key = p.key]() { ctrl->ApplyThemeByName(key); },
        ButtonOption::Ascii());
    preset_buttons.push_back(std::move(btn));
  }

  auto preset_row = Container::Horizontal(preset_buttons);

  // ── ThemeStage (center) ──────────────────────────────────────────────────
  auto stage = ThemeStage();

  // ── LiveThemeBar (bottom) ────────────────────────────────────────────────
  auto bar = LiveThemeBar(ctrl);

  // ── Header ───────────────────────────────────────────────────────────────
  auto header_renderer = Renderer([] {
    const Theme& t = GetTheme();
    return hbox({
               text(" 🐺 FTXUI BadWolf") | bold | color(t.primary),
               text(" — Live Theme Studio") | color(t.text_muted),
               filler(),
               text(" [1-9] Presets  [V] Voice  [S] Showcase  [Q] Quit ") |
                   color(t.text_muted),
           }) |
           border;
  });

  // ── Showcase state ────────────────────────────────────────────────────────
  struct ShowcaseState {
    bool active = false;
    int frame_cb = -1;
    float accum = 0.0f;
    int idx = 0;
  };
  auto sc = std::make_shared<ShowcaseState>();
  const float cycle_seconds = 5.0f;
  const std::vector<std::string> cycle_names =
      VoiceThemeController::AvailableThemeNames();

  auto toggle_showcase = [sc, ctrl, &cycle_names, cycle_seconds]() {
    if (sc->active) {
      if (sc->frame_cb >= 0) {
        AnimationLoop::Instance().RemoveCallback(sc->frame_cb);
        sc->frame_cb = -1;
      }
      sc->active = false;
    } else {
      sc->active = true;
      sc->accum = 0.0f;
      sc->frame_cb = AnimationLoop::Instance().OnFrame(
          [sc, ctrl, cycle_names, cycle_seconds](float dt) {
            sc->accum += dt;
            if (sc->accum >= cycle_seconds) {
              sc->accum = 0.0f;
              sc->idx = (sc->idx + 1) % static_cast<int>(cycle_names.size());
              ctrl->ApplyThemeByName(cycle_names[sc->idx]);
              if (auto* app = App::Active()) {
                app->PostEvent(Event::Custom);
              }
            }
          });
    }
  };

  // ── Main layout ──────────────────────────────────────────────────────────
  auto layout = Container::Vertical({
      preset_row,
      stage,
      bar,
  });

  auto root = Renderer(layout, [&, header_renderer, preset_row, stage, bar]() {
    const Theme& t = GetTheme();
    return vbox({
        header_renderer->Render(),
        hbox({
            preset_row->Render() | border | color(t.border_color),
        }),
        stage->Render() | flex,
        bar->Render(),
    });
  });

  // ── Keyboard shortcuts ────────────────────────────────────────────────────
  auto event_root = CatchEvent(root, [&, ctrl, sc](Event event) {
    // 1-9 preset keys
    if (event.is_character()) {
      char c = event.character()[0];
      if (c >= '1' && c <= '9') {
        int idx = c - '1';
        if (idx < static_cast<int>(presets.size())) {
          ctrl->ApplyThemeByName(presets[idx].key);
          return true;
        }
      }
      if (c == 'q' || c == 'Q') {
        if (auto* app = App::Active()) {
          app->PostEvent(Event::Custom);
        }
        return false;  // Let app handle quit
      }
      if (c == 's' || c == 'S') {
        toggle_showcase();
        return true;
      }
    }
    if (event == Event::F12) {
      toggle_showcase();
      return true;
    }
    return false;
  });

  auto app = App::Fullscreen();
  app.Loop(event_root);

  // Cleanup showcase frame callback
  if (sc->frame_cb >= 0) {
    AnimationLoop::Instance().RemoveCallback(sc->frame_cb);
  }

  return 0;
}
