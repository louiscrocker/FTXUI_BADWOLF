// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_DATATABLE_HPP
#define FTXUI_UI_DATATABLE_HPP

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/theme.hpp"

namespace ftxui::ui {

/// @brief Fluent builder for an interactive, sortable, filterable data table.
///
/// Example:
/// @code
/// struct Employee { std::string name, dept; int salary; };
/// std::vector<Employee> employees = { ... };
///
/// auto table = ui::DataTable<Employee>()
///     .Column("Name",   [](const Employee& e){ return e.name; })
///     .Column("Dept",   [](const Employee& e){ return e.dept; })
///     .Column("Salary", [](const Employee& e){ return "$"+std::to_string(e.salary); }, 10)
///     .Data(&employees)
///     .Selectable(true)
///     .Sortable(true)
///     .OnSelect([](const Employee& e, size_t i){ /* ... */ })
///     .OnActivate([](const Employee& e, size_t i){ /* ... */ })
///     .Build();
/// @endcode
///
/// @tparam T  The data row type.
/// @ingroup ui
template <typename T>
class DataTable {
 public:
  // ── Column definition ──────────────────────────────────────────────────────

  /// @brief Add a column with a display header and value extractor.
  /// @param header  Column header text.
  /// @param getter  Function that extracts the display string from a row.
  /// @param width   Optional fixed column width in characters (-1 = auto).
  DataTable& Column(std::string header,
                    std::function<std::string(const T&)> getter,
                    int width = -1) {
    state_->columns.push_back({std::move(header), std::move(getter), width});
    return *this;
  }

  // ── Data binding ───────────────────────────────────────────────────────────

  /// @brief Bind the data vector. The pointer must outlive the component.
  DataTable& Data(const std::vector<T>* data) {
    state_->data = data;
    return *this;
  }

  // ── Behavior ───────────────────────────────────────────────────────────────

  /// @brief Enable keyboard row selection (arrow keys). Default: true.
  DataTable& Selectable(bool v) {
    state_->selectable = v;
    return *this;
  }

  /// @brief Enable column sorting with '<' / '>'. Default: false.
  DataTable& Sortable(bool v) {
    state_->sortable = v;
    return *this;
  }

  /// @brief Enable alternating row background tinting. Default: true.
  DataTable& AlternateRows(bool v) {
    state_->alternate_rows = v;
    return *this;
  }

  // ── Callbacks ──────────────────────────────────────────────────────────────

  /// @brief Called whenever the selected row changes.
  DataTable& OnSelect(std::function<void(const T&, size_t)> fn) {
    state_->on_select = std::move(fn);
    return *this;
  }

  /// @brief Called when the user presses Enter on a row.
  DataTable& OnActivate(std::function<void(const T&, size_t)> fn) {
    state_->on_activate = std::move(fn);
    return *this;
  }

  // ── Filter binding ─────────────────────────────────────────────────────────

  /// @brief Bind an external filter string.
  ///
  /// When set, rows whose cells do not contain the filter text
  /// (case-insensitive) are hidden. The table reacts to changes in *filter
  /// automatically because it re-reads the pointer on every render.
  DataTable& FilterText(std::string* filter) {
    state_->filter_text = filter;
    return *this;
  }

  // ── Build ──────────────────────────────────────────────────────────────────

  /// @brief Finalize and return the interactive Component.
  ftxui::Component Build() {
    auto state = state_;

    auto renderer = ftxui::Renderer([state]() -> ftxui::Element {
      return Render(state);
    });

    return ftxui::CatchEvent(renderer, [state](ftxui::Event event) -> bool {
      return HandleEvent(state, event);
    });
  }

 private:
  // ── Internal types ─────────────────────────────────────────────────────────

  struct ColumnDef {
    std::string header;
    std::function<std::string(const T&)> getter;
    int width;  // -1 = auto
  };

  struct State {
    // Configuration
    std::vector<ColumnDef> columns;
    const std::vector<T>* data = nullptr;
    bool selectable = true;
    bool sortable = false;
    bool alternate_rows = true;
    std::function<void(const T&, size_t)> on_select;
    std::function<void(const T&, size_t)> on_activate;
    std::string* filter_text = nullptr;

    // Runtime
    int sort_column = -1;
    bool sort_ascending = true;
    int selected = 0;
  };

  // ── Helpers ────────────────────────────────────────────────────────────────

  static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
  }

  // Compute the ordered list of data indices to show, applying filter and sort.
  static std::vector<size_t> ComputeVisible(const std::shared_ptr<State>& s) {
    if (!s->data) {
      return {};
    }

    std::string filter_lower;
    if (s->filter_text && !s->filter_text->empty()) {
      filter_lower = ToLower(*s->filter_text);
    }

    std::vector<size_t> indices;
    indices.reserve(s->data->size());

    for (size_t i = 0; i < s->data->size(); ++i) {
      if (filter_lower.empty()) {
        indices.push_back(i);
      } else {
        bool match = false;
        for (const auto& col : s->columns) {
          if (ToLower(col.getter((*s->data)[i])).find(filter_lower) !=
              std::string::npos) {
            match = true;
            break;
          }
        }
        if (match) {
          indices.push_back(i);
        }
      }
    }

    if (s->sort_column >= 0 &&
        s->sort_column < static_cast<int>(s->columns.size())) {
      const auto& col = s->columns[static_cast<size_t>(s->sort_column)];
      std::stable_sort(indices.begin(), indices.end(),
                       [&](size_t a, size_t b) {
                         auto va = col.getter((*s->data)[a]);
                         auto vb = col.getter((*s->data)[b]);
                         return s->sort_ascending ? (va < vb) : (va > vb);
                       });
    }

    return indices;
  }

  // Build and return the rendered table Element.
  static ftxui::Element Render(const std::shared_ptr<State>& s) {
    const Theme& theme = GetTheme();

    if (!s->data || s->columns.empty()) {
      return ftxui::text("(no data)") | ftxui::color(theme.text_muted);
    }

    const auto visible = ComputeVisible(s);

    // Clamp selection into range.
    if (!visible.empty()) {
      s->selected =
          std::max(0, std::min(s->selected, static_cast<int>(visible.size()) - 1));
    }

    // ── Build Element matrix ──────────────────────────────────────────────

    std::vector<std::vector<ftxui::Element>> rows;
    rows.reserve(visible.size() + 1);

    // Header row
    {
      std::vector<ftxui::Element> header;
      header.reserve(s->columns.size());
      for (int ci = 0; ci < static_cast<int>(s->columns.size()); ++ci) {
        std::string h = s->columns[static_cast<size_t>(ci)].header;
        if (s->sortable && s->sort_column == ci) {
          h += s->sort_ascending ? " \u25b2" : " \u25bc";
        }
        ftxui::Element cell = ftxui::text(h) | ftxui::bold |
                              ftxui::color(theme.primary);
        const int w = s->columns[static_cast<size_t>(ci)].width;
        if (w > 0) {
          cell = cell | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, w);
        }
        header.push_back(std::move(cell));
      }
      rows.push_back(std::move(header));
    }

    // Data rows
    for (size_t vi = 0; vi < visible.size(); ++vi) {
      const size_t di = visible[vi];
      const bool is_selected =
          s->selectable && static_cast<int>(vi) == s->selected;
      const bool is_alternate =
          s->alternate_rows && !is_selected && (vi % 2 == 1);

      std::vector<ftxui::Element> row;
      row.reserve(s->columns.size());

      for (const auto& col : s->columns) {
        std::string val = col.getter((*s->data)[di]);
        if (col.width > 0 && static_cast<int>(val.size()) > col.width) {
          // Truncate with ellipsis (UTF-8 '…' = 3 bytes)
          val = val.substr(0, static_cast<size_t>(col.width) - 1) + "\xe2\x80\xa6";
        }

        ftxui::Element cell = ftxui::text(val);

        if (col.width > 0) {
          cell = cell | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, col.width);
        }

        if (is_selected) {
          cell = cell | ftxui::bgcolor(theme.secondary) |
                 ftxui::color(ftxui::Color::White);
        } else if (is_alternate) {
          cell = cell | ftxui::color(theme.text_muted);
        }

        row.push_back(std::move(cell));
      }
      rows.push_back(std::move(row));
    }

    // Empty state
    if (visible.empty()) {
      std::string msg = s->filter_text && !s->filter_text->empty()
                            ? "No matching rows"
                            : "No data";
      rows.push_back({ftxui::text(msg) | ftxui::color(theme.text_muted) |
                      ftxui::hcenter});
    }

    // ── Table decorators ──────────────────────────────────────────────────

    ftxui::Table table(std::move(rows));

    // Outer border
    table.SelectAll().Border(theme.border_style);

    // Separator under header
    if (!visible.empty()) {
      table.SelectRow(0).BorderBottom(ftxui::LIGHT);
    }

    // Vertical column separators
    table.SelectAll().SeparatorVertical(ftxui::LIGHT);

    return table.Render() | ftxui::vscroll_indicator | ftxui::yframe |
           ftxui::flex;
  }

  // Handle keyboard events; return true if consumed.
  static bool HandleEvent(const std::shared_ptr<State>& s,
                          ftxui::Event event) {
    if (!s->data) {
      return false;
    }

    const auto visible = ComputeVisible(s);
    const int n = static_cast<int>(visible.size());

    // Row navigation
    if (s->selectable) {
      if (event == ftxui::Event::ArrowUp && s->selected > 0) {
        s->selected--;
        if (s->on_select && s->selected < n) {
          s->on_select((*s->data)[visible[static_cast<size_t>(s->selected)]],
                       visible[static_cast<size_t>(s->selected)]);
        }
        return true;
      }
      if (event == ftxui::Event::ArrowDown && s->selected < n - 1) {
        s->selected++;
        if (s->on_select && s->selected < n) {
          s->on_select((*s->data)[visible[static_cast<size_t>(s->selected)]],
                       visible[static_cast<size_t>(s->selected)]);
        }
        return true;
      }
      if (event == ftxui::Event::Return && s->on_activate && n > 0 &&
          s->selected >= 0 && s->selected < n) {
        s->on_activate((*s->data)[visible[static_cast<size_t>(s->selected)]],
                       visible[static_cast<size_t>(s->selected)]);
        return true;
      }
    }

    // Sort column navigation
    if (s->sortable && !s->columns.empty()) {
      if (event.is_character() && event.character() == "<") {
        s->sort_column--;
        if (s->sort_column < -1) {
          s->sort_column = static_cast<int>(s->columns.size()) - 1;
        }
        s->selected = 0;
        return true;
      }
      if (event.is_character() && event.character() == ">") {
        s->sort_column++;
        if (s->sort_column >= static_cast<int>(s->columns.size())) {
          s->sort_column = -1;
        }
        s->selected = 0;
        return true;
      }
      // Toggle sort direction when pressing Enter on a sorted column
      if (event.is_character() && event.character() == "s" &&
          s->sort_column >= 0) {
        s->sort_ascending = !s->sort_ascending;
        s->selected = 0;
        return true;
      }
    }

    return false;
  }

  // ── Data ───────────────────────────────────────────────────────────────────

  std::shared_ptr<State> state_ = std::make_shared<State>();
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_DATATABLE_HPP
