// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/simple_table.hpp"

#include <algorithm>
#include <utility>

#include "ftxui/dom/table.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

SimpleTable::SimpleTable(std::vector<std::string> headers,
                          std::vector<std::vector<std::string>> rows)
    : headers_(std::move(headers)), rows_(std::move(rows)) {}

SimpleTable& SimpleTable::AlternateRows(bool enable) {
  alternate_rows_ = enable;
  return *this;
}

SimpleTable& SimpleTable::ColumnSeparators(bool enable) {
  col_separators_ = enable;
  return *this;
}

SimpleTable& SimpleTable::ColumnWidths(std::vector<int> widths) {
  col_widths_ = std::move(widths);
  return *this;
}

Element SimpleTable::Build() const {
  const Theme& t = GetTheme();
  const int num_cols = static_cast<int>(headers_.size());
  const int num_rows = static_cast<int>(rows_.size());

  // Build the cell matrix.  Row 0 = header.
  std::vector<std::vector<Element>> cells;
  cells.reserve(1 + static_cast<size_t>(num_rows));

  // Header row.
  {
    std::vector<Element> header_cells;
    header_cells.reserve(static_cast<size_t>(num_cols));
    for (int c = 0; c < num_cols; ++c) {
      Element cell = text(headers_[c]) | bold | color(t.primary);
      if (c < static_cast<int>(col_widths_.size()) && col_widths_[c] > 0) {
        cell = cell | size(WIDTH, EQUAL, col_widths_[c]);
      }
      header_cells.push_back(std::move(cell));
    }
    cells.push_back(std::move(header_cells));
  }

  // Data rows.
  for (int r = 0; r < num_rows; ++r) {
    std::vector<Element> row_cells;
    row_cells.reserve(static_cast<size_t>(num_cols));
    for (int c = 0; c < num_cols; ++c) {
      const std::string& val =
          (c < static_cast<int>(rows_[r].size())) ? rows_[r][c] : "";
      Element cell = text(val);
      if (c < static_cast<int>(col_widths_.size()) && col_widths_[c] > 0) {
        cell = cell | size(WIDTH, EQUAL, col_widths_[c]);
      }
      row_cells.push_back(std::move(cell));
    }
    cells.push_back(std::move(row_cells));
  }

  Table table(std::move(cells));

  // Outer border.
  table.SelectAll().Border(t.border_style);

  // Header separator.
  if (num_rows > 0) {
    table.SelectRow(0).BorderBottom(LIGHT);
  }

  // Alternate row shading.
  if (alternate_rows_ && num_rows > 1) {
    table.SelectRows(1, num_rows).DecorateAlternateRow(
        bgcolor(Color::RGB(30, 30, 30)), 2, 1);
  }

  // Vertical column separators.
  if (col_separators_) {
    table.SelectAll().SeparatorVertical(LIGHT);
  }

  return table.Render();
}

}  // namespace ftxui::ui
