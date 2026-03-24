// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file datatable_demo.cpp
/// Demonstrates ui::DataTable<T> — sortable, selectable, filterable table.

#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

struct Package {
  std::string name;
  std::string version;
  std::string license;
  int downloads;
  bool installed;
};

int main() {
  SetTheme(Theme::Nord());

  std::vector<Package> packages = {
      {"ftxui",     "6.1.9", "MIT",    1200, true},
      {"bubbletea", "0.27",  "MIT",     890, false},
      {"tview",     "0.38",  "MIT",     420, true},
      {"termbox",   "1.1.1", "MIT",     310, false},
      {"gocui",     "1.0.0", "Apache",  180, false},
      {"tcell",     "2.7.1", "Apache",  560, true},
      {"ncurses",   "6.4",   "MIT",   42000, true},
      {"blessed",   "0.1.8", "MIT",     220, false},
      {"termui",    "3.1.0", "MIT",     150, false},
      {"charm",     "0.12",  "MIT",     800, true},
  };

  std::string selected_name;
  std::string status_msg = "Navigate with ↑↓, Enter to select, < > to sort";

  auto table = DataTable<Package>()
                   .Column("Name",      [](const Package& p){ return p.name; }, 12)
                   .Column("Version",   [](const Package& p){ return p.version; }, 8)
                   .Column("License",   [](const Package& p){ return p.license; }, 8)
                   .Column("Downloads", [](const Package& p){ return std::to_string(p.downloads); }, 10)
                   .Column("Installed", [](const Package& p){ return p.installed ? "✔" : ""; }, 10)
                   .Data(&packages)
                   .Selectable(true)
                   .Sortable(true)
                   .AlternateRows(true)
                   .OnSelect([&](const Package& p, size_t) {
                       selected_name = p.name;
                   })
                   .OnActivate([&](const Package& p, size_t) {
                       status_msg = "Activated: " + p.name + " " + p.version;
                   })
                   .Build();

  auto detail = Renderer([&]() -> Element {
    const Theme& t = GetTheme();
    if (selected_name.empty()) {
      return text("Select a package…") | dim | hcenter;
    }
    return hbox({
        text("Selected: ") | dim,
        text(selected_name) | bold | color(t.primary),
    }) | hcenter;
  });

  auto layout = Container::Vertical({table});

  auto comp = Renderer(layout, [&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               text("  Package Browser (DataTable demo)  ") | bold | hcenter |
                   color(t.primary),
               separatorLight(),
               table->Render() | flex,
               separator(),
               detail->Render(),
               separator(),
               text(" " + status_msg + " ") | dim | hcenter,
           }) |
           borderStyled(t.border_style, t.border_color) | flex;
  });

  comp |= Keymap()
              .Bind("q", [] { if (App* a = App::Active()) a->Exit(); }, "Quit")
              .AsDecorator();

  App::Fullscreen().Loop(comp);
  return 0;
}
