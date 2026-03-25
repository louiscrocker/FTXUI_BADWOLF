// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file reactive_list_demo.cpp
/// Demonstrates ReactiveList<T>:
///
///  - VirtualList bound to a ReactiveList<string>
///  - Buttons: Add, Remove selected, Clear all
///  - Live count badge via BindText
///  - Press 'q' or Escape to quit.

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/virtual_list.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Nord());
  const Theme& t = GetTheme();

  auto items = MakeReactiveList<std::string>(
      {"Apple", "Banana", "Cherry", "Date", "Elderberry"});

  // Track which item is selected so we can remove it.
  int selected_idx = 0;

  auto list_comp = VirtualList<std::string>()
                       .BindList(items)
                       .Render([&](const std::string& s, bool sel) -> Element {
                         if (sel) {
                           return text(" > " + s) | bold | color(Color::White) |
                                  bgcolor(t.secondary);
                         }
                         return text("   " + s);
                       })
                       .OnSelect([&](size_t idx, const std::string&) {
                         selected_idx = static_cast<int>(idx);
                       })
                       .Build();

  // Buttons – created OUTSIDE render lambda.
  static int counter = 6;
  auto btn_add = Button("[ + Add ]", [&] {
    items->Push("Item " + std::to_string(counter++));
  });

  auto btn_remove = Button("[ - Remove ]", [&] {
    if (!items->Empty()) {
      size_t idx = static_cast<size_t>(
          std::max(0, std::min(selected_idx,
                               static_cast<int>(items->Size()) - 1)));
      items->Remove(idx);
      if (selected_idx >= static_cast<int>(items->Size()) &&
          selected_idx > 0) {
        selected_idx--;
      }
    }
  });

  auto btn_clear = Button("[ Clear ]", [&] {
    items->Clear();
    selected_idx = 0;
  });

  auto count_reactive = std::make_shared<Reactive<std::string>>("5");
  items->OnChange([&](const std::vector<std::string>&) {
    count_reactive->Set(std::to_string(items->Size()) + " items");
  });

  auto buttons = Container::Horizontal({btn_add, btn_remove, btn_clear});
  auto root    = Container::Vertical({list_comp, buttons});

  root |= CatchEvent([&](Event event) -> bool {
    if (event == Event::Character('q') || event == Event::Escape) {
      if (App* app = App::Active()) app->Exit();
      return true;
    }
    return false;
  });

  auto ui = Renderer(root, [&]() -> Element {
    return vbox({
               hbox({
                   text(" ReactiveList Demo ") | bold | color(t.primary),
                   filler(),
                   BindText(count_reactive) | color(t.accent) | bold,
                   text(" "),
               }) | bgcolor(t.border_color),
               separatorLight(),
               list_comp->Render() | flex,
               separatorLight(),
               hbox({btn_add->Render(), text(" "), btn_remove->Render(),
                     text(" "), btn_clear->Render()}) | hcenter,
               separatorLight(),
               text(" q/Esc quit | ↑↓ navigate ") | color(t.text_muted) | hcenter,
           }) |
           border | size(WIDTH, EQUAL, 50);
  });

  RunFullscreen(ui);
  return 0;
}
