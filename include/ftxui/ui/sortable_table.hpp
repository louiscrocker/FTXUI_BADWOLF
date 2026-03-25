// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_SORTABLE_TABLE_HPP
#define FTXUI_UI_SORTABLE_TABLE_HPP

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/screen/box.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/theme.hpp"

namespace ftxui::ui {

/// @brief Sortable, filterable, paginated data table.
///
/// Provides click-column sort, type-to-filter, pagination, and row selection
/// without modifying the existing DataTable<T> component.
///
/// @code
/// struct Server { std::string name, status; float cpu; int mem; };
/// auto data = std::make_shared<std::vector<Server>>(...);
///
/// auto table = SortableTable<Server>()
///     .Columns({
///         {"Name",   [](const Server& s){ return s.name; },
///          [](const Server& a, const Server& b){ return a.name < b.name; }},
///         {"CPU %",  [](const Server& s){ return std::to_string(s.cpu); },
///          [](const Server& a, const Server& b){ return a.cpu < b.cpu; }},
///     })
///     .Rows(data)
///     .PageSize(20)
///     .ShowFilter(true)
///     .OnSelect([](size_t i, const Server& s){ ... })
///     .Build();
/// @endcode
///
/// @tparam T  The row data type.
/// @ingroup ui
template <typename T>
class SortableTable {
 public:
  // ── Column definition ──────────────────────────────────────────────────────

  struct Column {
    std::string header;
    /// Extract display string from a row.
    std::function<std::string(const T&)> value;
    /// Less-than comparator for sorting (optional — falls back to string sort).
    std::function<bool(const T&, const T&)> less;
    int width = 12;
    bool sortable = true;
  };

  // ── Builder methods ────────────────────────────────────────────────────────

  /// @brief Provide column definitions.
  SortableTable& Columns(std::vector<Column> cols) {
    state_->columns = std::move(cols);
    return *this;
  }

  /// @brief Provide row data via shared pointer (live reference).
  SortableTable& Rows(std::shared_ptr<std::vector<T>> data) {
    state_->data = std::move(data);
    return *this;
  }

  /// @brief Number of rows per page (0 = no pagination, show all). Default: 0.
  SortableTable& PageSize(int n) {
    state_->page_size = std::max(0, n);
    return *this;
  }

  /// @brief Show a type-to-filter search bar above the table. Default: true.
  SortableTable& ShowFilter(bool v = true) {
    state_->show_filter = v;
    return *this;
  }

  /// @brief Called when the selected row changes.
  SortableTable& OnSelect(std::function<void(size_t, const T&)> fn) {
    state_->on_select = std::move(fn);
    return *this;
  }

  /// @brief Override the header text color.
  SortableTable& HeaderColor(ftxui::Color c) {
    state_->header_color = c;
    return *this;
  }

  /// @brief Override the selected-row highlight color.
  SortableTable& SelectionColor(ftxui::Color c) {
    state_->selection_color = c;
    return *this;
  }

  // ── Build ──────────────────────────────────────────────────────────────────

  /// @brief Finalize and return the interactive Component.
  ftxui::Component Build() {
    auto s = state_;

    auto renderer =
        ftxui::Renderer([s]() -> ftxui::Element { return DoRender(s); });

    return ftxui::CatchEvent(renderer, [s](ftxui::Event event) -> bool {
      return HandleEvent(s, event);
    });
  }

 private:
  // ── Internal state ─────────────────────────────────────────────────────────

  struct State {
    // Configuration
    std::vector<Column> columns;
    std::shared_ptr<std::vector<T>> data;
    int page_size = 0;
    bool show_filter = true;
    std::function<void(size_t, const T&)> on_select;
    ftxui::Color header_color = ftxui::Color::Default;  // resolved in DoRender
    ftxui::Color selection_color = ftxui::Color::Default;
    bool header_color_set = false;
    bool selection_color_set = false;

    // Runtime
    int sort_col = -1;  // active sort column index (-1 = none)
    bool sort_asc = true;
    int focused_col = 0;  // column highlighted for sort cycling
    std::string filter_str;
    int page = 0;
    int selected = 0;

    std::vector<size_t> filtered;  // filtered+sorted row indices
    bool filtered_dirty = true;

    // Mouse hit-testing boxes (updated each render).
    std::vector<ftxui::Box> header_boxes;
    std::vector<ftxui::Box> row_boxes;
  };

  // ── Helpers ────────────────────────────────────────────────────────────────

  static std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return out;
  }

  // Recompute filtered+sorted index list.
  static void RebuildFiltered(const std::shared_ptr<State>& s) {
    s->filtered.clear();
    if (!s->data) {
      s->filtered_dirty = false;
      return;
    }

    const std::string fl = ToLower(s->filter_str);
    s->filtered.reserve(s->data->size());

    for (size_t i = 0; i < s->data->size(); ++i) {
      if (fl.empty()) {
        s->filtered.push_back(i);
      } else {
        bool match = false;
        for (const auto& col : s->columns) {
          if (ToLower(col.value((*s->data)[i])).find(fl) != std::string::npos) {
            match = true;
            break;
          }
        }
        if (match) {
          s->filtered.push_back(i);
        }
      }
    }

    // Apply sort.
    if (s->sort_col >= 0 && s->sort_col < static_cast<int>(s->columns.size())) {
      const auto& col = s->columns[static_cast<size_t>(s->sort_col)];
      std::stable_sort(
          s->filtered.begin(), s->filtered.end(), [&](size_t a, size_t b) {
            const T& ra = (*s->data)[a];
            const T& rb = (*s->data)[b];
            if (col.less) {
              return s->sort_asc ? col.less(ra, rb) : col.less(rb, ra);
            }
            // Fallback: string comparison.
            auto va = col.value(ra);
            auto vb = col.value(rb);
            return s->sort_asc ? (va < vb) : (va > vb);
          });
    }

    s->filtered_dirty = false;
  }

  // Compute page-windowed view of filtered rows.
  static std::vector<size_t> PageView(const std::shared_ptr<State>& s) {
    const int n = static_cast<int>(s->filtered.size());
    if (s->page_size <= 0 || n == 0) {
      return s->filtered;
    }
    const int max_pages = std::max(1, (n + s->page_size - 1) / s->page_size);
    s->page = std::clamp(s->page, 0, max_pages - 1);
    const int start = s->page * s->page_size;
    const int end = std::min(start + s->page_size, n);
    return std::vector<size_t>(s->filtered.begin() + start,
                               s->filtered.begin() + end);
  }

  // Render the full component.
  static Element DoRender(const std::shared_ptr<State>& s) {
    if (s->filtered_dirty) {
      RebuildFiltered(s);
    }

    const Theme& theme = GetTheme();
    const ftxui::Color hdr_color =
        s->header_color_set ? s->header_color : theme.primary;
    const ftxui::Color sel_color =
        s->selection_color_set ? s->selection_color : theme.secondary;

    if (!s->data || s->columns.empty()) {
      return text("(no data)") | color(theme.text_muted);
    }

    const auto page_view = PageView(s);
    const int pv_size = static_cast<int>(page_view.size());

    // Clamp selection to page view.
    if (pv_size > 0) {
      s->selected = std::clamp(s->selected, 0, pv_size - 1);
    } else {
      s->selected = 0;
    }

    Elements outer;

    // ── Filter bar ────────────────────────────────────────────────────────
    if (s->show_filter) {
      outer.push_back(hbox({
          text(" ") | color(theme.text_muted),
          text("Filter: ") | color(theme.text_muted),
          text(s->filter_str) | bold | color(hdr_color),
          text("\u2587") | color(hdr_color),  // ▇ cursor
          filler(),
          text(std::to_string(static_cast<int>(s->filtered.size())) + "/" +
               std::to_string(static_cast<int>(s->data->size())) + " rows") |
              color(theme.text_muted),
          text(" "),
      }));
      outer.push_back(separatorLight());
    }

    // ── Build table matrix ────────────────────────────────────────────────
    std::vector<std::vector<Element>> rows;
    rows.reserve(page_view.size() + 1);

    // Header row.
    {
      s->header_boxes.resize(s->columns.size());
      std::vector<Element> header;
      header.reserve(s->columns.size());
      for (int ci = 0; ci < static_cast<int>(s->columns.size()); ++ci) {
        const auto& col = s->columns[static_cast<size_t>(ci)];
        std::string h = col.header;
        if (col.sortable) {
          if (s->sort_col == ci) {
            h += s->sort_asc ? " \u25b2" : " \u25bc";  // ▲ ▼
          } else if (s->focused_col == ci) {
            h += " \u2022";  // • indicates focus
          }
        }
        Element cell = text(h) | bold | color(hdr_color);
        if (col.width > 0) {
          cell = cell | size(WIDTH, EQUAL, col.width);
        }
        cell = cell | reflect(s->header_boxes[static_cast<size_t>(ci)]);
        header.push_back(std::move(cell));
      }
      rows.push_back(std::move(header));
    }

    // Data rows.
    s->row_boxes.resize(static_cast<size_t>(pv_size));
    for (int vi = 0; vi < pv_size; ++vi) {
      const size_t di = page_view[static_cast<size_t>(vi)];
      const bool is_sel = (vi == s->selected);
      const bool is_alt = !is_sel && (vi % 2 == 1);

      std::vector<Element> row;
      row.reserve(s->columns.size());

      for (const auto& col : s->columns) {
        std::string val = col.value((*s->data)[di]);
        if (col.width > 0 && static_cast<int>(val.size()) > col.width) {
          val = val.substr(0, static_cast<size_t>(col.width) - 1) +
                "\xe2\x80\xa6";  // …
        }

        Element cell = text(val);
        if (col.width > 0) {
          cell = cell | size(WIDTH, EQUAL, col.width);
        }
        if (is_sel) {
          cell = cell | bgcolor(sel_color) | color(Color::White);
        } else if (is_alt) {
          cell = cell | color(theme.text_muted);
        }
        row.push_back(std::move(cell));
      }
      rows.push_back(std::move(row));
    }

    // Empty state.
    if (pv_size == 0) {
      std::string msg = !s->filter_str.empty() ? "No matching rows" : "No data";
      rows.push_back({text(msg) | color(theme.text_muted) | hcenter | flex});
    }

    // Build ftxui Table.
    ftxui::Table tbl(std::move(rows));
    tbl.SelectAll().Border(theme.border_style);
    if (pv_size > 0) {
      tbl.SelectRow(0).BorderBottom(ftxui::LIGHT);
    }
    tbl.SelectAll().SeparatorVertical(ftxui::LIGHT);

    outer.push_back(tbl.Render() | vscroll_indicator | yframe | flex);

    // ── Pagination bar ────────────────────────────────────────────────────
    if (s->page_size > 0) {
      const int total = static_cast<int>(s->filtered.size());
      const int max_pages =
          std::max(1, (total + s->page_size - 1) / s->page_size);
      const int cur_page = s->page + 1;

      outer.push_back(separatorLight());
      outer.push_back(hbox({
          text(" ") | color(theme.text_muted),
          text("PgUp") | dim,
          text("/"),
          text("PgDn") | dim,
          text(" to page   Page ") | color(theme.text_muted),
          text(std::to_string(cur_page)) | bold | color(hdr_color),
          text(" / " + std::to_string(max_pages)) | color(theme.text_muted),
          filler(),
          text("Showing " + std::to_string(static_cast<int>(page_view.size())) +
               " of " + std::to_string(total) + " rows ") |
              color(theme.text_muted),
      }));
    }

    return vbox(std::move(outer)) | flex;
  }

  // Handle keyboard events; return true if consumed.
  static bool HandleEvent(const std::shared_ptr<State>& s, ftxui::Event event) {
    if (s->filtered_dirty) {
      RebuildFiltered(s);
    }

    const auto page_view = PageView(s);
    const int pv_size = static_cast<int>(page_view.size());
    const int num_cols = static_cast<int>(s->columns.size());
    const int total = static_cast<int>(s->filtered.size());
    const int max_pages =
        s->page_size > 0
            ? std::max(1, (total + s->page_size - 1) / s->page_size)
            : 1;

    // ── Row navigation ────────────────────────────────────────────────────
    if (event == ftxui::Event::ArrowUp && s->selected > 0) {
      s->selected--;
      FireSelect(s, page_view);
      return true;
    }
    if (event == ftxui::Event::ArrowDown && s->selected < pv_size - 1) {
      s->selected++;
      FireSelect(s, page_view);
      return true;
    }

    // ── Sort column cycling ───────────────────────────────────────────────
    if (event == ftxui::Event::ArrowLeft && num_cols > 0) {
      // Cycle focused column left.
      s->focused_col = (s->focused_col - 1 + num_cols) % num_cols;
      // Skip non-sortable columns.
      int attempts = num_cols;
      while (!s->columns[static_cast<size_t>(s->focused_col)].sortable &&
             attempts-- > 0) {
        s->focused_col = (s->focused_col - 1 + num_cols) % num_cols;
      }
      return true;
    }
    if (event == ftxui::Event::ArrowRight && num_cols > 0) {
      s->focused_col = (s->focused_col + 1) % num_cols;
      int attempts = num_cols;
      while (!s->columns[static_cast<size_t>(s->focused_col)].sortable &&
             attempts-- > 0) {
        s->focused_col = (s->focused_col + 1) % num_cols;
      }
      return true;
    }

    // ── Sort on Enter ─────────────────────────────────────────────────────
    if (event == ftxui::Event::Return && num_cols > 0) {
      if (s->sort_col == s->focused_col) {
        // Already sorting by this column — toggle direction.
        s->sort_asc = !s->sort_asc;
      } else {
        s->sort_col = s->focused_col;
        s->sort_asc = true;
      }
      s->selected = 0;
      s->filtered_dirty = true;
      return true;
    }

    // ── Pagination ────────────────────────────────────────────────────────
    if (event == ftxui::Event::PageUp && s->page_size > 0) {
      if (s->page > 0) {
        s->page--;
        s->selected = 0;
      }
      return true;
    }
    if (event == ftxui::Event::PageDown && s->page_size > 0) {
      if (s->page < max_pages - 1) {
        s->page++;
        s->selected = 0;
      }
      return true;
    }

    // ── Filter input ──────────────────────────────────────────────────────
    if (s->show_filter) {
      if (event == ftxui::Event::Escape) {
        s->filter_str.clear();
        s->page = 0;
        s->selected = 0;
        s->filtered_dirty = true;
        return true;
      }
      if (event == ftxui::Event::Backspace) {
        if (!s->filter_str.empty()) {
          s->filter_str.pop_back();
          s->page = 0;
          s->selected = 0;
          s->filtered_dirty = true;
        }
        return true;
      }
      if (event.is_character()) {
        const std::string ch = event.character();
        // Ignore navigation-like single chars that collide with sort keys.
        if (ch != "\t") {
          s->filter_str += ch;
          s->page = 0;
          s->selected = 0;
          s->filtered_dirty = true;
          return true;
        }
      }
    }

    return false;
  }

  static void FireSelect(const std::shared_ptr<State>& s,
                         const std::vector<size_t>& page_view) {
    const int pv = static_cast<int>(page_view.size());
    if (s->on_select && pv > 0 && s->selected >= 0 && s->selected < pv) {
      const size_t di = page_view[static_cast<size_t>(s->selected)];
      s->on_select(di, (*s->data)[di]);
    }
  }

  // ── Data ───────────────────────────────────────────────────────────────────

  std::shared_ptr<State> state_ = std::make_shared<State>();
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_SORTABLE_TABLE_HPP
