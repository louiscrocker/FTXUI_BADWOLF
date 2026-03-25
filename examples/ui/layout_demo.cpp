// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file layout_demo.cpp
/// Demonstrates the ftxui::ui layout helpers:
///   Panel, HSplit, ScrollView, TabView, StatusBar, Column, Keymap, WithHelp

#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Dracula());

  // ── Editor panel (left) ────────────────────────────────────────────────────
  std::string note;
  InputOption multiline_opt;
  multiline_opt.multiline = true;
  auto editor_input = Input(&note, "Start typing…", multiline_opt);
  auto editor = Panel("Editor", editor_input);

  // ── Log panel (right, scrollable) ──────────────────────────────────────────
  const std::vector<std::string> log_lines = {
      "[INFO]  App started.",
      "[INFO]  Loading config…",
      "[WARN]  No config found, using defaults.",
      "[INFO]  Ready.",
      "[INFO]  Waiting for input.",
      "[INFO]  User opened editor.",
      "[DEBUG] Render cycle 1.",
      "[DEBUG] Render cycle 2.",
      "[DEBUG] Render cycle 3.",
      "[DEBUG] Render cycle 4.",
      "[DEBUG] Render cycle 5.",
  };
  auto log_content = Renderer([&log_lines]() -> Element {
    const Theme& t = GetTheme();
    Elements rows;
    for (const auto& line : log_lines) {
      Color c = t.text;
      if (line.find("[WARN]") != std::string::npos) {
        c = t.warning_color;
      }
      if (line.find("[ERROR]") != std::string::npos) {
        c = t.error_color;
      }
      rows.push_back(text(line) | color(c));
    }
    return vbox(std::move(rows));
  });
  auto log_panel = ScrollView("Log", log_content);

  // ── Main: resizable left/right split ───────────────────────────────────────
  int split_pos = 50;
  auto main_view = HSplit(editor, log_panel, &split_pos);

  // ── Settings tab ───────────────────────────────────────────────────────────
  std::string timeout_val = "30";
  bool dark_mode = true;
  std::vector<std::string> themes = {"Dracula", "Nord", "Monokai", "Light"};
  int theme_sel = 0;
  auto settings = Form()
                      .Title("Settings")
                      .Field("Timeout", &timeout_val, "seconds")
                      .Check("Dark mode", &dark_mode)
                      .Select("Color theme", &themes, &theme_sel)
                      .Build();

  // ── About tab ──────────────────────────────────────────────────────────────
  auto about = Renderer([]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               separatorEmpty(),
               text("FTXUI ui:: Layout Demo") | bold | hcenter |
                   color(t.primary),
               separatorEmpty(),
               paragraph(
                   "This example showcases the higher-level layout helpers "
                   "in ftxui::ui.  It demonstrates Panel, HSplit, ScrollView, "
                   "TabView, StatusBar, Form, Keymap, and WithHelp working "
                   "together in a realistic multi-panel layout.") |
                   center,
               separatorEmpty(),
               text("Press '?' for keyboard shortcuts, 'q' to quit.") | dim |
                   hcenter,
           }) |
           center | flex;
  });

  // ── Tab view ───────────────────────────────────────────────────────────────
  int tab_sel = 0;
  auto tabs = TabView({"Main", "Settings", "About"},
                      {main_view | flex, settings, about}, &tab_sel);

  // ── Status bar ─────────────────────────────────────────────────────────────
  std::string status_msg = "Ready  │  Use Tab/Shift+Tab to navigate";
  auto status = StatusBar([&status_msg] { return status_msg; });

  // ── Compose into a full-screen layout ──────────────────────────────────────
  auto layout = Column({tabs | flex, status});

  // ── Keybindings ────────────────────────────────────────────────────────────
  bool show_help = false;

  layout |= Keymap()
                .Bind(
                    "q",
                    [] {
                      if (App* a = App::Active()) {
                        a->Exit();
                      }
                    },
                    "Quit")
                .Bind(
                    "1",
                    [&] {
                      tab_sel = 0;
                      status_msg = "Main view";
                    },
                    "Main tab")
                .Bind(
                    "2",
                    [&] {
                      tab_sel = 1;
                      status_msg = "Settings";
                    },
                    "Settings tab")
                .Bind(
                    "3",
                    [&] {
                      tab_sel = 2;
                      status_msg = "About";
                    },
                    "About tab")
                .Bind(
                    "?", [&] { show_help = true; }, "Help")
                .AsDecorator();

  // ── Help overlay ───────────────────────────────────────────────────────────
  layout |= WithHelp("Keyboard Shortcuts",
                     {{"q", "Quit"},
                      {"1 / 2 / 3", "Switch tabs"},
                      {"Tab", "Next focusable field"},
                      {"Shift+Tab", "Previous field"},
                      {"←→ (in split)", "Resize panels"},
                      {"?", "Show this help"}},
                     &show_help, [&] { show_help = false; });

  App::Fullscreen().Loop(layout);
  return 0;
}
