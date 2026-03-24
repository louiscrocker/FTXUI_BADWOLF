// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_MARKDOWN_HPP
#define FTXUI_UI_MARKDOWN_HPP

#include <string>
#include <string_view>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

/// @brief Render Markdown text as a vertically stacked FTXUI Element.
///
/// Supports:
///   # H1   ## H2   ### H3       — headers (bold, colored by level)
///   **bold**  __bold__          — bold text
///   *italic*  _italic_          — italic (dim in terminal)
///   `inline code`               — highlighted code span
///   ```lang\n...\n```           — fenced code block
///   - item  * item  + item      — unordered lists
///   1. item  2. item            — ordered lists
///   ---  ___  ***               — horizontal rule
///   > quote text                — blockquote (indented, dimmed)
///   [link text](url)            — link (show text, dim url hint)
///   Plain paragraphs            — text with inline formatting
///   Blank line                  — paragraph break
///
/// @code
/// auto el = Markdown(R"(
/// # Hello World
/// This is **bold** and *italic* text with `inline code`.
///
/// ## Section
/// - First item
/// - Second item
/// )");
/// @endcode
///
/// @returns A vertically stacked ftxui::Element.
ftxui::Element Markdown(std::string_view md);

/// @brief Render Markdown with explicit width for word wrapping.
ftxui::Element Markdown(std::string_view md, int width);

/// @brief A scrollable, interactive Markdown viewer Component.
ftxui::Component MarkdownComponent(std::string_view md);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_MARKDOWN_HPP
