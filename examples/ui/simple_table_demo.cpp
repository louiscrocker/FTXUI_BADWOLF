// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file simple_table_demo.cpp
/// Demonstrates ui::SimpleTable and the raw FTXUI Table DOM.

#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Dracula());

  // ── Data ───────────────────────────────────────────────────────────────────
  const std::vector<std::string> headers = {
      "Package", "Version", "License", "Downloads"};
  const std::vector<std::vector<std::string>> rows = {
      {"ftxui",     "6.1.9", "MIT",     "1.2M"},
      {"bubbletea", "0.27",  "MIT",     "890K"},
      {"tview",     "0.38",  "MIT",     "420K"},
      {"termbox",   "1.1.1", "MIT",     "310K"},
      {"gocui",     "1.0.0", "Apache",  "180K"},
      {"tcell",     "2.7.1", "Apache",  "560K"},
      {"ncurses",   "6.4",   "MIT",     "42M"},
      {"blessed",   "0.1.8", "MIT",     "220K"},
  };

  // ── Widgets demo ───────────────────────────────────────────────────────────
  auto comp = Renderer([&]() -> Element {
    const Theme& t = GetTheme();

    auto table1 = SimpleTable(headers, rows)
                      .AlternateRows(true)
                      .ColumnSeparators(true)
                      .Build();

    auto table2 = SimpleTable(headers, rows)
                      .AlternateRows(false)
                      .ColumnSeparators(false)
                      .Build();

    // Badge demo
    auto badge_row = hbox({
        text("Alerts") | ui::Badge(3, Color::Red),
        text("  "),
        text("Messages") | ui::Badge(42),
        text("  "),
        text("Updates") | ui::Badge("NEW", Color::Green),
        text("  "),
        ui::StatusDot(Color::Green, "Online"),
        text("  "),
        ui::StatusDot(Color::Red, "Error"),
    });

    // Widget demos
    auto widgets_section = vbox({
        ui::LabeledSeparator("Status Indicators"),
        badge_row | hcenter,
        separatorEmpty(),
        ui::LabeledSeparator("Keyboard Hints"),
        hbox({
            text("Press "),
            ui::Kbd("Ctrl+S"),
            text(" to save  "),
            ui::Kbd("?"),
            text(" for help  "),
            ui::Kbd("q"),
            text(" to quit"),
        }) | hcenter,
        separatorEmpty(),
        ui::EmptyState("📦", "No packages selected", "Use arrow keys to select"),
    });

    return vbox({
               text("  SimpleTable + Widget Demo  ") | bold | hcenter |
                   color(t.primary),
               separatorLight(),
               separatorEmpty(),
               text("With alternate rows + separators:") | bold | color(t.secondary),
               table1,
               separatorEmpty(),
               text("Plain (no alt rows, no separators):") | bold | color(t.secondary),
               table2,
               separatorEmpty(),
               widgets_section,
               separatorEmpty(),
               text("Press 'q' to quit") | dim | hcenter,
           }) |
           borderStyled(t.border_style, t.border_color) | center;
  });

  comp |= Keymap()
              .Bind("q", [] { if (App* a = App::Active()) a->Exit(); }, "Quit")
              .AsDecorator();

  App::TerminalOutput().Loop(comp);
  return 0;
}
