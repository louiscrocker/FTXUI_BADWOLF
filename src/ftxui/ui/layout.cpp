// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/layout.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Panel
// ─────────────────────────────────────────────────────────────────────

Component Panel(std::string_view title, Component content) {
  std::string t{title};
  return Renderer(content, [t, content]() -> Element {
    const Theme& theme = GetTheme();
    return window(text(" " + t + " ") | bold, content->Render(),
                  theme.border_style);
  });
}

Component Panel(std::string_view title, std::function<Element()> render) {
  std::string t{title};
  return Renderer([t, render]() -> Element {
    const Theme& theme = GetTheme();
    return window(text(" " + t + " ") | bold, render(), theme.border_style);
  });
}

// ── Row
// ───────────────────────────────────────────────────────────────────────

Component Row(Components children) {
  auto captured = std::make_shared<Components>(children);
  auto container = Container::Horizontal(std::move(children));
  return Renderer(container, [captured]() -> Element {
    Elements elems;
    elems.reserve(captured->size());
    for (auto& c : *captured) {
      elems.push_back(c->Render() | flex);
    }
    return hbox(std::move(elems));
  });
}

// ── Column
// ────────────────────────────────────────────────────────────────────

Component Column(Components children) {
  auto captured = std::make_shared<Components>(children);
  auto container = Container::Vertical(std::move(children));
  return Renderer(container, [captured]() -> Element {
    Elements elems;
    elems.reserve(captured->size());
    for (auto& c : *captured) {
      elems.push_back(c->Render());
    }
    return vbox(std::move(elems));
  });
}

// ── Status bar
// ────────────────────────────────────────────────────────────────

Component StatusBar(std::function<std::string()> text_fn) {
  return StatusBar([text_fn]() -> Element {
    const Theme& t = GetTheme();
    return hbox({text(" " + text_fn() + " ") | xflex}) | bgcolor(t.secondary) |
           color(t.button_fg_active);
  });
}

Component StatusBar(std::function<Element()> render_fn) {
  return Renderer([render_fn]() -> Element { return render_fn(); });
}

// ── Scroll view
// ───────────────────────────────────────────────────────────────

Component ScrollView(Component content) {
  return Renderer(content, [content]() -> Element {
    return content->Render() | vscroll_indicator | yframe | flex;
  });
}

Component ScrollView(std::string_view title, Component content) {
  std::string t{title};
  return Renderer(content, [t, content]() -> Element {
    const Theme& theme = GetTheme();
    return window(text(" " + t + " ") | bold,
                  content->Render() | vscroll_indicator | yframe | flex,
                  theme.border_style);
  });
}

// ── Labeled
// ───────────────────────────────────────────────────────────────────

Component Labeled(std::string_view label, Component content, int label_width) {
  std::string lbl{label};
  int lw = label_width;
  return Renderer(content, [lbl, lw, content]() -> Element {
    return hbox({
        text(lbl + " :") | size(WIDTH, EQUAL, lw),
        text(" "),
        content->Render() | xflex,
    });
  });
}

// ── Splits
// ────────────────────────────────────────────────────────────────────

Component HSplit(Component left, Component right, int* split_pos) {
  if (split_pos) {
    return ResizableSplitLeft(left, right, split_pos);
  }
  // Fixed 50/50.
  auto captured_left = left;
  auto captured_right = right;
  auto container = Container::Horizontal({std::move(left), std::move(right)});
  return Renderer(container, [captured_left, captured_right]() -> Element {
    return hbox({
        captured_left->Render() | flex,
        separator(),
        captured_right->Render() | flex,
    });
  });
}

Component VSplit(Component top, Component bottom, int* split_pos) {
  if (split_pos) {
    return ResizableSplitTop(top, bottom, split_pos);
  }
  auto captured_top = top;
  auto captured_bot = bottom;
  auto container = Container::Vertical({std::move(top), std::move(bottom)});
  return Renderer(container, [captured_top, captured_bot]() -> Element {
    return vbox({
        captured_top->Render() | flex,
        separator(),
        captured_bot->Render() | flex,
    });
  });
}

// ── Tab view
// ──────────────────────────────────────────────────────────────────

Component TabView(const std::vector<std::string>& tabs,
                  Components pages,
                  int* selected) {
  // Own a copy of the tab labels so the vector outlives this call.
  auto tabs_copy = std::make_shared<std::vector<std::string>>(tabs);
  auto owned_sel = std::make_shared<int>(0);
  int* sel = selected ? selected : owned_sel.get();

  auto tab_bar = Toggle(tabs_copy.get(), sel);
  auto tab_content = Container::Tab(std::move(pages), sel);
  auto container = Container::Vertical({tab_bar, tab_content});

  return Renderer(container,
                  [tab_bar, tab_content, tabs_copy, owned_sel]() -> Element {
                    (void)tabs_copy;
                    (void)owned_sel;
                    return vbox({
                        tab_bar->Render(),
                        separator(),
                        tab_content->Render() | flex,
                    });
                  });
}

}  // namespace ftxui::ui
