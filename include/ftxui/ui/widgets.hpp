// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_WIDGETS_HPP
#define FTXUI_UI_WIDGETS_HPP

#include <string_view>

#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

// ── Badge ─────────────────────────────────────────────────────────────────────

/// @brief Add a small numeric badge to any element.
///
/// @code
/// text("Messages") | ui::Badge(3)
/// text("Alerts")   | ui::Badge(12, Color::Red)
/// @endcode
ftxui::Decorator Badge(int count, ftxui::Color color = ftxui::Color::Red);

/// @brief Add a small text badge to any element.
ftxui::Decorator Badge(std::string_view label,
                        ftxui::Color color = ftxui::Color::Red);

/// @brief Wrap @p base element with a numeric badge overlay.
ftxui::Element WithBadge(ftxui::Element base,
                          int count,
                          ftxui::Color color = ftxui::Color::Red);

// ── Empty state ───────────────────────────────────────────────────────────────

/// @brief Render a centered empty-state placeholder.
///
/// @code
/// if (items.empty()) {
///     return ui::EmptyState("🗒", "No items", "Add one to get started.");
/// }
/// @endcode
ftxui::Element EmptyState(std::string_view icon,
                           std::string_view title,
                           std::string_view subtitle = "");

// ── Divider with label ────────────────────────────────────────────────────────

/// @brief A horizontal separator with a centered label.
///
/// @code
/// ui::LabeledSeparator("Advanced Options")
/// @endcode
ftxui::Element LabeledSeparator(std::string_view label);

// ── Kbd shortcut hint ─────────────────────────────────────────────────────────

/// @brief Render a single keyboard shortcut hint box.
///
/// @code
/// hbox({ text("Press "), ui::Kbd("Ctrl+S"), text(" to save") })
/// @endcode
ftxui::Element Kbd(std::string_view key);

// ── Colored dot / status indicator ───────────────────────────────────────────

/// @brief A small colored status dot (●) with an optional label.
ftxui::Element StatusDot(ftxui::Color color, std::string_view label = "");

}  // namespace ftxui::ui

#endif  // FTXUI_UI_WIDGETS_HPP
