// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/lcars.hpp"

#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

namespace {
Color EffectiveColor(Color c) {
  if (c == Color::Default) {
    return GetTheme().primary;
  }
  return c;
}
}  // namespace

// ── LCARSPanel
// ────────────────────────────────────────────────────────────────

Element LCARSPanel(std::string title, Element content, Color c) {
  c = EffectiveColor(c);

  // Left vertical bar (the signature LCARS elbow column).
  auto left_bar = vbox({
      text("") | bgcolor(c) | size(WIDTH, EQUAL, 3),
      text("") | bgcolor(c) | size(WIDTH, EQUAL, 3) | flex,
  });

  // Title header row.
  auto title_row = hbox({
      text("") | bgcolor(c) | size(WIDTH, EQUAL, 3),
      text(" " + title + " ") | bold | color(Color::Black) | bgcolor(c),
      filler() | bgcolor(c),
  });

  // Main panel body: narrow left bar + content.
  auto body = hbox({
      text("  ") | bgcolor(c),
      separatorEmpty(),
      std::move(content) | flex,
  });

  return vbox({
      title_row,
      body | flex,
  });
}

// ── LCARSButton
// ───────────────────────────────────────────────────────────────

Component LCARSButton(std::string label,
                      std::function<void()> on_click,
                      Color c) {
  c = EffectiveColor(c);
  auto opt = ButtonOption::Animated(Color::Default, c, c, Color::Black);
  opt.transform = [label, c](const EntryState& state) -> Element {
    auto elem = text(" " + label + " ");
    if (state.focused) {
      return elem | bold | color(Color::Black) | bgcolor(c);
    }
    return elem | color(c) | borderStyled(ROUNDED, c);
  };
  return Button(label, std::move(on_click), opt);
}

// ── LCARSBar
// ──────────────────────────────────────────────────────────────────

Element LCARSBar(std::vector<std::pair<std::string, Color>> segments) {
  Elements parts;
  for (auto& [label, seg_color] : segments) {
    parts.push_back(text(" " + label + " ") | bold | color(Color::Black) |
                    bgcolor(seg_color));
    parts.push_back(text(" "));
  }
  return hbox(std::move(parts));
}

// ── LCARSReadout ─────────────────────────────────────────────────────────────

Element LCARSReadout(std::string label, std::string value, Color label_color) {
  label_color = EffectiveColor(label_color);
  const Theme& t = GetTheme();
  return hbox({
      text(label) | color(label_color) | dim,
      text(" "),
      filler(),
      text(value) | bold | color(t.text),
  });
}

// ── LCARSScreen ──────────────────────────────────────────────────────────────

Component LCARSScreen(std::string title,
                      Component sidebar,
                      Component main_content) {
  const Theme& t = GetTheme();
  Color primary = t.primary;

  return Renderer(Container::Horizontal({sidebar, main_content}),
                  [title, sidebar, main_content, primary]() -> Element {
                    // Top title bar.
                    auto title_bar = hbox({
                        text("") | bgcolor(primary) | size(WIDTH, EQUAL, 3),
                        text(" " + title + " ") | bold | color(Color::Black) |
                            bgcolor(primary),
                        filler() | bgcolor(primary),
                    });

                    // Body: sidebar + separator + main.
                    auto body = hbox({
                        sidebar->Render() | size(WIDTH, LESS_THAN, 22),
                        separator() | color(primary),
                        main_content->Render() | flex,
                    });

                    return vbox({title_bar, body | flex});
                  });
}

}  // namespace ftxui::ui
