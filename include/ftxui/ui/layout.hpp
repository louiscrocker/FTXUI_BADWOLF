// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_LAYOUT_HPP
#define FTXUI_UI_LAYOUT_HPP

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

// ── Panels
// ────────────────────────────────────────────────────────────────────

/// @brief Wrap a component in a titled, bordered panel.
ftxui::Component Panel(std::string_view title, ftxui::Component content);

/// @brief Create a stateless panel from a render function.
ftxui::Component Panel(std::string_view title,
                       std::function<ftxui::Element()> render);

// ── Layout containers
// ─────────────────────────────────────────────────────────

/// @brief Horizontal container where all children grow equally.
/// Equivalent to Container::Horizontal with each child wrapped in flex.
ftxui::Component Row(ftxui::Components children);

/// @brief Vertical container where all children are stacked.
ftxui::Component Column(ftxui::Components children);

// ── Status bar
// ────────────────────────────────────────────────────────────────

/// @brief A fixed 1-row bar at the bottom of a layout.
///
/// The bar renders the result of @p render_fn on every frame.
/// Combine with a Column:
/// @code
/// auto layout = ui::Column({
///     main_content | flex,
///     ui::StatusBar([&]{ return "Mode: Normal | " + status; }),
/// });
/// @endcode
ftxui::Component StatusBar(std::function<std::string()> text_fn);
ftxui::Component StatusBar(std::function<ftxui::Element()> render_fn);

// ── Scrollable frame
// ──────────────────────────────────────────────────────────

/// @brief Wrap a component in a vertically-scrollable frame.
ftxui::Component ScrollView(ftxui::Component content);

/// @brief Scrollable frame with a titled border.
ftxui::Component ScrollView(std::string_view title, ftxui::Component content);

// ── Labeled row
// ───────────────────────────────────────────────────────────────

/// @brief Wrap a component in a "Label :  [component]" row.
///
/// Useful when laying out individual fields without using Form.
ftxui::Component Labeled(std::string_view label,
                         ftxui::Component content,
                         int label_width = 14);

// ── Resizable splits
// ──────────────────────────────────────────────────────────

/// @brief Side-by-side resizable split.
///
/// @param split_pos  Pointer to the divider column position (may be nullptr for
///                   a fixed 50/50 split at construction time).
ftxui::Component HSplit(ftxui::Component left,
                        ftxui::Component right,
                        int* split_pos = nullptr);

/// @brief Top/bottom resizable split.
ftxui::Component VSplit(ftxui::Component top,
                        ftxui::Component bottom,
                        int* split_pos = nullptr);

// ── Tab view
// ──────────────────────────────────────────────────────────────────

/// @brief Horizontal tab bar that switches between pages.
///
/// @param tabs      Display labels for each tab.
/// @param pages     One component per tab (must have same size as @p tabs).
/// @param selected  Pointer to the index of the currently visible tab.
ftxui::Component TabView(const std::vector<std::string>& tabs,
                         ftxui::Components pages,
                         int* selected = nullptr);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_LAYOUT_HPP
