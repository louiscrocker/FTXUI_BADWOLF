// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "ftxui/ui/grid.hpp"

#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

Grid::Grid(int columns) : cols_(columns) {}

Grid& Grid::Cell(ftxui::Element element) {
  cells_.push_back({std::move(element), nullptr});
  return *this;
}

Grid& Grid::CellComponent(ftxui::Component component) {
  cells_.push_back({nullptr, std::move(component)});
  return *this;
}

Grid& Grid::Gap(int gap) {
  gap_ = gap;
  return *this;
}

Grid& Grid::RowGap(int gap) {
  row_gap_ = gap;
  return *this;
}

ftxui::Component Grid::Build() {
  // Collect interactive children for Container::Horizontal navigation.
  ftxui::Components all_components;
  for (auto& c : cells_) {
    if (c.component) all_components.push_back(c.component);
  }

  // Snapshot config & cells for the renderer closure.
  auto cells    = cells_;
  int  cols     = cols_;
  int  gap      = gap_;
  int  row_gap  = row_gap_;

  ftxui::Component container;
  if (all_components.empty()) {
    // Pure element grid — no interactive children.
    container = ftxui::Renderer([=]() -> ftxui::Element {
      std::vector<ftxui::Element> rows;
      std::vector<ftxui::Element> row_cells;

      for (size_t i = 0; i < cells.size(); ++i) {
        // Render cell
        ftxui::Element cell = cells[i].element ? cells[i].element
                                               : ftxui::text("") | ftxui::flex;
        row_cells.push_back(cell | ftxui::flex);

        // Insert gap between cells (not after last in row)
        bool last_in_row = ((int(i) + 1) % cols == 0);
        bool last_cell   = (i + 1 == cells.size());

        if (!last_in_row && gap > 0) {
          row_cells.push_back(ftxui::text(std::string(gap, ' ')));
        }

        if (last_in_row || last_cell) {
          // Pad incomplete last row
          while ((int)row_cells.size() < cols + (cols - 1) * (gap > 0 ? 1 : 0)) {
            if (gap > 0) row_cells.push_back(ftxui::text(std::string(gap, ' ')));
            row_cells.push_back(ftxui::filler());
          }
          rows.push_back(ftxui::hbox(row_cells));
          row_cells.clear();
          if (!last_cell && row_gap > 0) {
            rows.push_back(ftxui::separatorEmpty());
          }
        }
      }

      if (rows.empty()) return ftxui::text("");
      return ftxui::vbox(rows);
    });
  } else {
    // Mixed / component grid.
    auto tab = ftxui::Container::Horizontal(all_components);

    container = ftxui::Renderer(tab, [=]() -> ftxui::Element {
      size_t comp_idx = 0;
      std::vector<ftxui::Element> rows;
      std::vector<ftxui::Element> row_cells;

      for (size_t i = 0; i < cells.size(); ++i) {
        ftxui::Element cell;
        if (cells[i].component) {
          cell = all_components[comp_idx++]->Render() | ftxui::flex;
        } else {
          cell = (cells[i].element ? cells[i].element : ftxui::text("")) |
                 ftxui::flex;
        }
        row_cells.push_back(cell);

        bool last_in_row = ((int(i) + 1) % cols == 0);
        bool last_cell   = (i + 1 == cells.size());

        if (!last_in_row && gap > 0) {
          row_cells.push_back(ftxui::text(std::string(gap, ' ')));
        }

        if (last_in_row || last_cell) {
          while ((int)row_cells.size() < cols) {
            row_cells.push_back(ftxui::filler());
          }
          rows.push_back(ftxui::hbox(row_cells));
          row_cells.clear();
          if (!last_cell && row_gap > 0) {
            rows.push_back(ftxui::separatorEmpty());
          }
        }
      }

      if (rows.empty()) return ftxui::text("");
      return ftxui::vbox(rows);
    });
  }

  return container;
}

}  // namespace ftxui::ui
