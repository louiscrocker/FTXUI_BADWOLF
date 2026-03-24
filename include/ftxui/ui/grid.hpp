// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_GRID_HPP
#define FTXUI_UI_GRID_HPP

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

/// @brief Fixed-column grid layout that auto-fills cells left-to-right.
///
/// Each cell gets an equal share of the horizontal space.  Rows are created
/// automatically as cells are added.
///
/// @code
/// auto grid = ui::Grid(3)
///     .Cell(text("A") | hcenter)
///     .Cell(text("B") | hcenter)
///     .Cell(text("C") | hcenter)
///     .Cell(text("D") | hcenter)   // starts row 2
///     .Gap(1)                       // px gap between cells
///     .Build();
/// @endcode
///
/// For interactive cells (buttons, inputs) use `CellComponent`:
/// @code
/// auto grid = ui::Grid(2)
///     .CellComponent(btn1)
///     .CellComponent(btn2)
///     .Build();
/// @endcode
///
/// @ingroup ui
class Grid {
 public:
  explicit Grid(int columns = 2);

  /// @brief Append a static Element cell.
  Grid& Cell(ftxui::Element element);

  /// @brief Append an interactive Component cell.
  Grid& CellComponent(ftxui::Component component);

  /// @brief Pixel gap between cells (default 0).
  Grid& Gap(int gap);

  /// @brief Pixel gap between rows (default 0).
  Grid& RowGap(int gap);

  /// @brief Build the final component.
  ftxui::Component Build();

 private:
  struct CellEntry {
    ftxui::Element    element;    // static cell (may be nullptr)
    ftxui::Component  component;  // interactive cell (may be nullptr)
  };

  int               cols_     = 2;
  int               gap_      = 0;
  int               row_gap_  = 0;
  std::vector<CellEntry> cells_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_GRID_HPP
