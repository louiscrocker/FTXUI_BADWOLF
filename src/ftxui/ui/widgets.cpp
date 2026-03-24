// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/widgets.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Badge ─────────────────────────────────────────────────────────────────────

Decorator Badge(int count, Color badge_color) {
  return [count, badge_color](Element base) -> Element {
    if (count <= 0) return base;
    std::string label = count > 99 ? "99+" : std::to_string(count);
    auto dot = text(label) | bold | color(Color::White) | bgcolor(badge_color) |
               size(WIDTH, EQUAL, static_cast<int>(label.size()) + 2);
    return hbox({std::move(base), dot});
  };
}

Decorator Badge(std::string_view label, Color badge_color) {
  return [label = std::string(label), badge_color](Element base) -> Element {
    if (label.empty()) return base;
    auto dot = text(" " + label + " ") | bold | color(Color::White) |
               bgcolor(badge_color);
    return hbox({std::move(base), dot});
  };
}

Element WithBadge(Element base, int count, Color badge_color) {
  return std::move(base) | Badge(count, badge_color);
}

// ── Empty state ───────────────────────────────────────────────────────────────

Element EmptyState(std::string_view icon,
                    std::string_view title,
                    std::string_view subtitle) {
  const Theme& t = GetTheme();
  Elements rows;
  if (!icon.empty()) {
    rows.push_back(text(std::string(icon)) | hcenter);
    rows.push_back(separatorEmpty());
  }
  rows.push_back(text(std::string(title)) | bold | hcenter |
                  color(t.text_muted));
  if (!subtitle.empty()) {
    rows.push_back(text(std::string(subtitle)) | dim | hcenter);
  }
  return vbox(std::move(rows)) | center;
}

// ── LabeledSeparator ─────────────────────────────────────────────────────────

Element LabeledSeparator(std::string_view label) {
  const Theme& t = GetTheme();
  if (label.empty()) return separatorLight();
  return hbox({
      separatorLight() | xflex,
      text(" " + std::string(label) + " ") | color(t.text_muted) | dim,
      separatorLight() | xflex,
  });
}

// ── Kbd ───────────────────────────────────────────────────────────────────────

Element Kbd(std::string_view key) {
  const Theme& t = GetTheme();
  return text(" " + std::string(key) + " ") | bold | color(t.text) |
         borderStyled(LIGHT, t.text_muted);
}

// ── StatusDot ─────────────────────────────────────────────────────────────────

Element StatusDot(Color dot_color, std::string_view label) {
  auto dot = text("●") | color(dot_color);
  if (label.empty()) return dot;
  return hbox({dot, text(" " + std::string(label))});
}

}  // namespace ftxui::ui
