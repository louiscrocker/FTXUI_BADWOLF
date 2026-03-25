// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file nl_theme_demo.cpp
/// @brief Natural Language Theming demo.
///
/// Full-screen ThemePicker with a sidebar of 8 preset descriptions.
/// Each preset applies the theme with a smooth ThemeTransition animation.
/// Shows LLM status and a sample UI panel that reflects the active theme.

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/nl_theme.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Dark());

  // ── Preset descriptions ──────────────────────────────────────────────────
  const std::vector<std::string> presets = {
      "dark blue cyberpunk", "minimal white corporate", "matrix hacker green",
      "warm sunset orange",  "deep space navy gold",    "retro amber 80s",
      "neon pink synthwave", "forest nature green",
  };

  // Current transition (heap-allocated so it can be reset on each click).
  auto transition = std::make_shared<std::unique_ptr<ThemeTransition>>();
  *transition = nullptr;

  // ── Preset buttons ───────────────────────────────────────────────────────
  std::vector<ftxui::Component> preset_btns;
  for (const auto& desc : presets) {
    auto btn = ftxui::Button(
        desc,
        [desc, transition] {
          Theme from = GetTheme();
          Theme to = NLThemeParser::FromDescription(desc);
          auto tr = std::make_unique<ThemeTransition>(
              from, to, std::chrono::milliseconds(600));
          tr->Start();
          *transition = std::move(tr);
        },
        ButtonOption::Ascii());
    preset_btns.push_back(std::move(btn));
  }

  auto sidebar_container = ftxui::Container::Vertical(preset_btns);

  // ── ThemePicker (main panel) ─────────────────────────────────────────────
  auto picker = ThemePicker([](Theme t) { SetTheme(t); });

  // ── Sample UI panel ──────────────────────────────────────────────────────
  auto sample_panel = ftxui::Renderer([] {
    const Theme& t = GetTheme();
    return ftxui::vbox({
               ftxui::text("Sample UI") | ftxui::bold | ftxui::color(t.primary),
               ftxui::separator() | ftxui::color(t.border_color),
               ftxui::hbox({
                   ftxui::text("  Name: ") | ftxui::color(t.text_muted),
                   ftxui::text("Arthur Sonzogni") | ftxui::color(t.text),
               }),
               ftxui::hbox({
                   ftxui::text("  Status: ") | ftxui::color(t.text_muted),
                   ftxui::text("Active") | ftxui::color(t.success_color),
               }),
               ftxui::hbox({
                   ftxui::text("  Warning: ") | ftxui::color(t.text_muted),
                   ftxui::text("Low memory") | ftxui::color(t.warning_color),
               }),
               ftxui::hbox({
                   ftxui::text("  Error: ") | ftxui::color(t.text_muted),
                   ftxui::text("None") | ftxui::color(t.error_color),
               }),
               ftxui::separator() | ftxui::color(t.border_color),
               ftxui::gauge(0.72f) | ftxui::color(t.accent),
               ftxui::text("  72% capacity") | ftxui::color(t.text_muted),
           }) |
           ftxui::border | ftxui::color(t.border_color);
  });

  // ── Layout ───────────────────────────────────────────────────────────────
  auto container = ftxui::Container::Horizontal({
      sidebar_container,
      ftxui::Container::Vertical({picker, sample_panel}),
  });

  auto renderer = ftxui::Renderer(container, [&] {
    const Theme& t = GetTheme();
    std::string llm_status = LLMThemeGenerator::IsAvailable()
                                 ? "● Ollama available"
                                 : "○ Rule-based mode";

    auto sidebar_elem = ftxui::vbox({
        ftxui::text("Presets") | ftxui::bold | ftxui::color(t.primary),
        ftxui::separator() | ftxui::color(t.border_color),
        sidebar_container->Render(),
        ftxui::filler(),
        ftxui::separator() | ftxui::color(t.border_color),
        ftxui::text(llm_status) | ftxui::color(t.text_muted),
    });

    auto right_elem = ftxui::vbox({
        picker->Render(),
        sample_panel->Render(),
    });

    return ftxui::hbox({
               sidebar_elem | ftxui::size(WIDTH, EQUAL, 28) | ftxui::border |
                   ftxui::color(t.border_color),
               right_elem | ftxui::flex,
           }) |
           ftxui::flex;
  });

  auto app = App::Fullscreen();
  app.Loop(renderer);

  return 0;
}
