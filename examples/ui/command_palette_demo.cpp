// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file command_palette_demo.cpp
/// Demonstrates ui::CommandPalette — Ctrl+P fuzzy command launcher.

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Monokai());

  std::string log_msg = "Press Ctrl+P to open the Command Palette";
  std::string current_theme_name = "Monokai";

  auto body = Renderer([&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        separatorEmpty(),
        text("  Command Palette Demo  ") | bold | hcenter | color(t.primary),
        separatorEmpty(),
        separatorLight(),
        separatorEmpty(),
        paragraph(
            "This demo shows the CommandPalette component. Press Ctrl+P "
            "to open it, type to filter commands, use up/down to navigate, "
            "Enter to execute, and Escape to close.") |
            xflex,
        separatorEmpty(),
        hbox({text("  Current theme: ") | dim,
              text(current_theme_name) | bold | color(t.accent)}),
        separatorEmpty(),
        separatorLight(),
        separatorEmpty(),
        text("  Last action:") | dim,
        text("  " + log_msg) | color(t.primary),
        separatorEmpty(),
    });
  });

  // Build the palette and wrap the body component.
  auto comp =
      CommandPalette()
          .Bind("Ctrl+P")
          .Add(
              "File: New", [&] { log_msg = "New file created"; },
              "Create a new file", "File")
          .Add(
              "File: Open", [&] { log_msg = "Open dialog"; },
              "Open an existing file", "File")
          .Add(
              "File: Save", [&] { log_msg = "File saved"; },
              "Save current file", "File")
          .Add(
              "Edit: Copy", [&] { log_msg = "Copied to clipboard"; },
              "Copy selection", "Edit")
          .Add(
              "Edit: Paste", [&] { log_msg = "Pasted from clipboard"; },
              "Paste from clipboard", "Edit")
          .Add(
              "Edit: Find", [&] { log_msg = "Find dialog opened"; },
              "Find in file", "Edit")
          .Add(
              "View: Toggle Sidebar", [&] { log_msg = "Sidebar toggled"; },
              "Show/hide sidebar", "View")
          .Add(
              "Theme: Default",
              [&] {
                SetTheme(Theme::Default());
                current_theme_name = "Default";
                log_msg = "Theme -> Default";
              },
              "", "Theme")
          .Add(
              "Theme: Dark",
              [&] {
                SetTheme(Theme::Dark());
                current_theme_name = "Dark";
                log_msg = "Theme -> Dark";
              },
              "", "Theme")
          .Add(
              "Theme: Nord",
              [&] {
                SetTheme(Theme::Nord());
                current_theme_name = "Nord";
                log_msg = "Theme -> Nord";
              },
              "", "Theme")
          .Add(
              "Theme: Dracula",
              [&] {
                SetTheme(Theme::Dracula());
                current_theme_name = "Dracula";
                log_msg = "Theme -> Dracula";
              },
              "", "Theme")
          .Add(
              "Theme: Monokai",
              [&] {
                SetTheme(Theme::Monokai());
                current_theme_name = "Monokai";
                log_msg = "Theme -> Monokai";
              },
              "", "Theme")
          .Add(
              "Help: About", [&] { log_msg = "FTXUI ui:: layer v0.1.0"; }, "",
              "Help")
          .Add(
              "Quit",
              [&] {
                if (App* a = App::Active()) {
                  a->Exit();
                }
              },
              "Exit the application", "")
          .Wrap(body);

  comp |= Keymap()
              .Bind(
                  "q",
                  [] {
                    if (App* a = App::Active()) {
                      a->Exit();
                    }
                  },
                  "Quit")
              .AsDecorator();

  App::Fullscreen().Loop(comp);
  return 0;
}
