// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file list_demo.cpp
/// Demonstrates ui::List<T> — filterable, interactive item list.

#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

struct City {
  std::string name;
  std::string country;
  int population;  // thousands
};

int main() {
  SetTheme(Theme::Light());

  std::vector<City> cities = {
      {"Tokyo", "Japan", 13960},           {"Delhi", "India", 32226},
      {"Shanghai", "China", 24183},        {"São Paulo", "Brazil", 12330},
      {"Mexico City", "Mexico", 9209},     {"Cairo", "Egypt", 21750},
      {"Dhaka", "Bangladesh", 19578},      {"Mumbai", "India", 20667},
      {"Beijing", "China", 21707},         {"Osaka", "Japan", 2691},
      {"New York", "USA", 8335},           {"Karachi", "Pakistan", 14835},
      {"Buenos Aires", "Argentina", 3121}, {"Istanbul", "Turkey", 15462},
      {"Lagos", "Nigeria", 14862},         {"Kinshasa", "DRC", 14970},
      {"Manila", "Philippines", 1780},     {"Tianjin", "China", 13215},
      {"Guangzhou", "China", 16096},       {"Moscow", "Russia", 12506},
  };

  std::string selected_city;
  std::string activated_city;

  auto list = List<City>()
                  .Data(&cities)
                  .Label([](const City& c) { return c.name + " " + c.country; })
                  .Render([](const City& c, bool sel) -> Element {
                    const Theme& t = GetTheme();
                    auto name = text(c.name) | (sel ? bold : nothing);
                    auto country = text(c.country) | dim;
                    auto pop = text(std::to_string(c.population) + "k") | dim;
                    auto row = hbox({
                        name | size(WIDTH, EQUAL, 16),
                        country | size(WIDTH, EQUAL, 14),
                        pop,
                    });
                    if (sel) {
                      return row | color(t.primary);
                    }
                    return row;
                  })
                  .Filterable(true)
                  .OnSelect([&](const City& c, size_t) {
                    selected_city = c.name + ", " + c.country;
                  })
                  .OnActivate([&](const City& c, size_t) {
                    activated_city = "Activated: " + c.name + " (pop " +
                                     std::to_string(c.population) + "k)";
                  })
                  .Empty("No cities match your search")
                  .Build();

  auto detail = Renderer([&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        hbox({text(" Selected:  ") | dim,
              text(selected_city) | color(t.primary)}),
        hbox({text(" Activated: ") | dim,
              text(activated_city) | color(t.accent)}),
    });
  });

  auto layout = Container::Vertical({list});
  auto root = Renderer(layout, [&, list, detail]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               text("  City List Demo (List<T>)  ") | bold | hcenter |
                   color(t.primary),
               separatorLight(),
               hbox({
                   text(" Name") | bold | size(WIDTH, EQUAL, 16),
                   text(" Country") | bold | size(WIDTH, EQUAL, 14),
                   text(" Pop (k)") | bold,
               }) | color(t.secondary),
               separator(),
               list->Render() | flex,
               separator(),
               detail->Render(),
               separator(),
               text("  Type to filter · Enter=activate · q=quit  ") | dim |
                   hcenter,
           }) |
           borderStyled(t.border_style, t.border_color) | flex;
  });

  root |= Keymap()
              .Bind(
                  "q",
                  [] {
                    if (App* a = App::Active()) {
                      a->Exit();
                    }
                  },
                  "Quit")
              .AsDecorator();

  App::Fullscreen().Loop(root);
  return 0;
}
