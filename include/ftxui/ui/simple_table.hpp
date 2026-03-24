// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_SIMPLE_TABLE_HPP
#define FTXUI_UI_SIMPLE_TABLE_HPP

#include <string>
#include <string_view>
#include <vector>

#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

/// @brief A themed, auto-styled static table.
///
/// Wraps the raw FTXUI `Table` DOM class and automatically applies:
///  - Bold header row with theme primary color
///  - Alternate row shading
///  - Theme-styled border and vertical separators
///
/// @code
/// auto table = ui::SimpleTable(
///     {"Name", "Department", "Salary"},
///     {{"Alice", "Engineering", "$120k"},
///      {"Bob",   "Marketing",   "$95k"},
///      {"Carol", "Design",      "$105k"}})
///     .AlternateRows(true)
///     .Build();
/// @endcode
///
/// @ingroup ui
class SimpleTable {
 public:
  SimpleTable(std::vector<std::string> headers,
              std::vector<std::vector<std::string>> rows);

  /// @brief Enable alternating row background tinting.
  SimpleTable& AlternateRows(bool enable = true);

  /// @brief Show vertical column separators.
  SimpleTable& ColumnSeparators(bool enable = true);

  /// @brief Override the column widths (0 = auto).
  SimpleTable& ColumnWidths(std::vector<int> widths);

  /// @brief Render to a DOM Element.
  ftxui::Element Build() const;

  /// @brief Implicit conversion to Element.
  operator ftxui::Element() const { return Build(); }  // NOLINT

 private:
  std::vector<std::string> headers_;
  std::vector<std::vector<std::string>> rows_;
  std::vector<int> col_widths_;
  bool alternate_rows_ = true;
  bool col_separators_ = true;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_SIMPLE_TABLE_HPP
