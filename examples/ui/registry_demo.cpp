// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file registry_demo.cpp
/// Demonstrates the ftxui::ui::Registry — live browsable component catalogue.
///
/// Layout:
///   ┌─ search bar ──────────────────────────────────────────────────┐
///   │ [ filter...                                                   ]│
///   ├─ list (left) ───────────────┬─ metadata panel (right) ────────┤
///   │  sparkline-widget  1.0.0    │  Name:    sparkline-widget      │
///   │  file-tree         1.0.0    │  Version: 1.0.0                 │
///   │  ...                        │  Author:  badwolf-team          │
///   │                             │  Tags:    chart  visualization  │
///   │                             │  Desc:    Compact sparkline … │
///   ├─────────────────────────────┴─────────────────────────────────┤
///   │  badwolf list   badwolf install <component>                   │
///   └───────────────────────────────────────────────────────────────┘

#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/registry.hpp"

// Register a handful of demo components so the registry is non-empty even if
// no external components have been installed.
BADWOLF_REGISTER_COMPONENT(sparkline_widget,
                           "1.0.0",
                           "Compact sparkline charts",
                           []() -> ftxui::Component {
                             return ftxui::Renderer([] {
                               return ftxui::vbox(
                                   {ftxui::text("▁▂▃▄▅▆▇█▇▆▅▄▃▂▁") |
                                    ftxui::color(ftxui::Color::Cyan)});
                             });
                           });

BADWOLF_REGISTER_COMPONENT(file_tree,
                           "1.0.0",
                           "Interactive filesystem tree browser",
                           []() -> ftxui::Component {
                             return ftxui::Renderer([] {
                               return ftxui::vbox({
                                   ftxui::text("📁 src/") | ftxui::bold,
                                   ftxui::text("  📄 main.cpp"),
                                   ftxui::text("  📄 app.cpp"),
                                   ftxui::text("📁 include/"),
                                   ftxui::text("  📄 app.hpp"),
                               });
                             });
                           });

BADWOLF_REGISTER_COMPONENT(
    hex_viewer,
    "1.0.0",
    "Hex dump viewer with ASCII panel",
    []() -> ftxui::Component {
      return ftxui::Renderer([] {
        return ftxui::vbox({
            ftxui::text("00000000  48 65 6c 6c 6f 2c 20 57  6f 72 6c 64 21 0a "
                        "00 00  │Hello, World!..│") |
                ftxui::color(ftxui::Color::GreenLight),
        });
      });
    });

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Monokai());

  // ── State ────────────────────────────────────────────────────────────────
  std::string filter;
  int selected = 0;

  // ── Search input ─────────────────────────────────────────────────────────
  InputOption input_opt;
  input_opt.placeholder = "Filter components…";
  auto search_input = Input(&filter, input_opt);

  // ── Component list menu ──────────────────────────────────────────────────
  // We rebuild the visible entries on every render.
  auto list_renderer = Renderer(search_input, [&]() -> Element {
    const Theme& t = GetTheme();
    auto entries = filter.empty() ? Registry::Get().List()
                                  : Registry::Get().Search(filter);

    // Clamp selection
    if (!entries.empty()) {
      selected = std::max(0, std::min(selected, (int)entries.size() - 1));
    } else {
      selected = 0;
    }

    // ── Left panel: list ────────────────────────────────────────────────
    std::vector<Element> rows;
    for (int i = 0; i < (int)entries.size(); ++i) {
      const auto& m = entries[i];
      auto name_el = text(" " + m.name + " ") | xflex;
      auto ver_el = text(m.version + " ") | color(t.text_muted);
      auto row = hbox({name_el, ver_el});
      if (i == selected) {
        row = row | inverted;
      }
      rows.push_back(row);
    }
    if (rows.empty()) {
      rows.push_back(text("  (no results)") | dim);
    }

    Element left = vbox(rows) | vscroll_indicator | frame | flex;

    // ── Right panel: metadata ────────────────────────────────────────────
    Element right;
    if (entries.empty()) {
      right = text("  No component selected") | dim | vcenter | hcenter | flex;
    } else {
      const auto& m = entries[selected];

      // tags
      std::vector<Element> tag_els;
      for (const auto& tag : m.tags) {
        tag_els.push_back(text(" " + tag + " ") | border | color(t.secondary));
      }
      Element tags_el =
          tag_els.empty() ? text("—") | dim : hbox(tag_els) | xflex;

      right = vbox({
                  separatorEmpty(),
                  hbox({text("  Name:    ") | dim,
                        text(m.name) | bold | color(t.primary)}),
                  hbox({text("  Version: ") | dim,
                        text(m.version) | color(t.accent)}),
                  hbox({text("  Author:  ") | dim, text(m.author)}),
                  separatorEmpty(),
                  hbox({text("  Tags:    ") | dim, tags_el | xflex}),
                  separatorEmpty(),
                  separatorLight(),
                  separatorEmpty(),
                  paragraph("  " + m.description) | xflex,
                  separatorEmpty(),
              }) |
              flex;
    }

    // ── Search bar ───────────────────────────────────────────────────────
    Element search_box =
        hbox({text(" 🔍 "), search_input->Render() | xflex}) | border;

    // ── Footer ───────────────────────────────────────────────────────────
    std::string install_hint =
        entries.empty() ? "badwolf install <component>"
                        : "badwolf install " + entries[selected].name;

    Element footer = hbox({
                         text("  "),
                         text("badwolf list") | bold | color(t.secondary),
                         text("   "),
                         text(install_hint) | bold | color(t.accent),
                         text("   "),
                         text("↑↓ navigate  Enter select") | dim,
                         filler(),
                         text("BadWolf Registry v1.0.0  ") | dim,
                     }) |
                     size(HEIGHT, EQUAL, 1);

    return vbox({
        search_box,
        hbox({left | border | flex, right | border | flex}) | flex,
        separator(),
        footer,
    });
  });

  // ── Keyboard navigation ──────────────────────────────────────────────────
  auto app = CatchEvent(list_renderer, [&](Event event) -> bool {
    auto entries = filter.empty() ? Registry::Get().List()
                                  : Registry::Get().Search(filter);
    if (event == Event::ArrowUp) {
      selected = std::max(0, selected - 1);
      return true;
    }
    if (event == Event::ArrowDown) {
      selected = std::min((int)entries.size() - 1, selected + 1);
      return true;
    }
    if (event == Event::Escape) {
      App::Active()->Exit();
      return true;
    }
    return false;
  });

  RunFullscreen(app);
}
