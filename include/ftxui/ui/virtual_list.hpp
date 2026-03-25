// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_VIRTUAL_LIST_HPP
#define FTXUI_UI_VIRTUAL_LIST_HPP

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/box.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/reactive_list.hpp"
#include "ftxui/ui/theme.hpp"

namespace ftxui::ui {

/// @brief High-performance scrollable list that renders only visible rows.
///
/// Suitable for log files, search results, packet captures (1M+ items).
/// Only the items within the visible viewport are rendered; the rest are
/// skipped entirely, making scrolling O(visible) regardless of total size.
///
/// @code
/// auto list = VirtualList<std::string>()
///     .Items(my_million_lines)
///     .RowHeight(1)
///     .Render([](const std::string& s, bool selected) {
///         return selected ? text(s) | inverted : text(s);
///     })
///     .Filter([](const std::string& s, const std::string& q) {
///         return s.find(q) != std::string::npos;
///     })
///     .OnSelect([](size_t idx, const std::string& item) { ... })
///     .ShowSearch(true)
///     .Build();
/// @endcode
///
/// @tparam T  The item type.
/// @ingroup ui
template <typename T>
class VirtualList {
 public:
  // ── Data binding ───────────────────────────────────────────────────────────

  /// @brief Provide items by value (VirtualList takes ownership).
  VirtualList& Items(std::vector<T> items) {
    state_->owned_items = std::make_shared<std::vector<T>>(std::move(items));
    state_->items = state_->owned_items.get();
    state_->filtered_dirty = true;
    return *this;
  }

  /// @brief Provide items via shared pointer (live reference — pointer must
  ///        remain valid; changes are reflected automatically).
  VirtualList& Items(std::shared_ptr<std::vector<T>> items) {
    state_->owned_items = std::move(items);
    state_->items = state_->owned_items.get();
    state_->filtered_dirty = true;
    return *this;
  }

  /// @brief Bind a ReactiveList<T>; re-renders automatically on changes.
  VirtualList& BindList(std::shared_ptr<ReactiveList<T>> list) {
    state_->reactive_list = list;
    return *this;
  }

  // ── Layout ─────────────────────────────────────────────────────────────────

  /// @brief Number of terminal rows each item occupies. Default: 1.
  VirtualList& RowHeight(int h) {
    state_->row_height = std::max(1, h);
    return *this;
  }

  // ── Rendering ──────────────────────────────────────────────────────────────

  /// @brief Custom item renderer.
  ///
  /// @param fn  Called with (item, is_selected).  Responsible for highlighting.
  VirtualList& Render(std::function<Element(const T&, bool)> fn) {
    state_->render_fn = std::move(fn);
    return *this;
  }

  // ── Filtering ──────────────────────────────────────────────────────────────

  /// @brief Predicate deciding whether an item matches a query string.
  ///
  /// When not provided and ShowSearch(true), defaults to substring search on
  /// the string representation of T.
  VirtualList& Filter(std::function<bool(const T&, const std::string&)> fn) {
    state_->filter_fn = std::move(fn);
    return *this;
  }

  // ── Callbacks ──────────────────────────────────────────────────────────────

  /// @brief Called when the selected item changes.
  ///
  /// @param fn  Receives (original_index, item).
  VirtualList& OnSelect(std::function<void(size_t, const T&)> fn) {
    state_->on_select = std::move(fn);
    return *this;
  }

  // ── Search bar ─────────────────────────────────────────────────────────────

  /// @brief Show a type-to-search box at the top of the list. Default: false.
  VirtualList& ShowSearch(bool v = true) {
    state_->show_search = v;
    return *this;
  }

  // ── Build ──────────────────────────────────────────────────────────────────

  /// @brief Finalize and return the interactive Component.
  ftxui::Component Build() {
    auto s = state_;

    // Wire up ReactiveList if provided.
    if (s->reactive_list) {
      auto items = std::make_shared<std::vector<T>>(s->reactive_list->Items());
      s->owned_items    = items;
      s->items          = items.get();
      s->filtered_dirty = true;

      auto weak_s = std::weak_ptr<State>(s);
      s->reactive_list->OnChange([weak_s](const std::vector<T>& new_items) {
        if (auto state = weak_s.lock()) {
          state->owned_items    = std::make_shared<std::vector<T>>(new_items);
          state->items          = state->owned_items.get();
          state->filtered_dirty = true;
        }
        // PostEvent already called by ReactiveList::Notify().
      });
    }

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
    std::shared_ptr<std::vector<T>> owned_items;
    const std::vector<T>* items = nullptr;
    int row_height = 1;
    std::function<Element(const T&, bool)> render_fn;
    std::function<bool(const T&, const std::string&)> filter_fn;
    std::function<void(size_t, const T&)> on_select;
    bool show_search = false;

    // ReactiveList binding (optional).
    std::shared_ptr<ReactiveList<T>> reactive_list;

    // Runtime
    int selected = 0;
    int scroll_offset = 0;
    std::string query;
    std::vector<size_t> filtered;
    bool filtered_dirty = true;

    // Dimension tracking (updated via reflect() each frame)
    Box box;
  };

  // ── Helpers ────────────────────────────────────────────────────────────────

  static std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return out;
  }

  // Convert item to string for default filter/render.
  static std::string ItemStr(const T& item) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
      return item;
    } else if constexpr (std::is_arithmetic_v<T>) {
      return std::to_string(item);
    } else {
      return "";
    }
  }

  // Rebuild the filtered index vector.
  static void RebuildFiltered(const std::shared_ptr<State>& s) {
    s->filtered.clear();
    if (!s->items) {
      s->filtered_dirty = false;
      return;
    }
    s->filtered.reserve(s->items->size());

    if (s->query.empty()) {
      for (size_t i = 0; i < s->items->size(); ++i) {
        s->filtered.push_back(i);
      }
    } else {
      const std::string ql = ToLower(s->query);
      for (size_t i = 0; i < s->items->size(); ++i) {
        bool match = false;
        if (s->filter_fn) {
          match = s->filter_fn((*s->items)[i], s->query);
        } else {
          match =
              ToLower(ItemStr((*s->items)[i])).find(ql) != std::string::npos;
        }
        if (match) {
          s->filtered.push_back(i);
        }
      }
    }
    s->filtered_dirty = false;
  }

  // Default single-row item renderer.
  static Element DefaultRenderItem(const std::shared_ptr<State>& s,
                                   const T& item,
                                   bool selected,
                                   const Theme& theme) {
    Element e = text(" " + ItemStr(item));
    if (selected) {
      return e | bgcolor(theme.secondary) | color(Color::White) | bold;
    }
    return e;
  }

  // Render the component element (called each frame).
  static Element DoRender(const std::shared_ptr<State>& s) {
    const Theme& theme = GetTheme();

    if (s->filtered_dirty) {
      RebuildFiltered(s);
    }

    const int n = static_cast<int>(s->filtered.size());

    // Clamp selection.
    if (n > 0) {
      s->selected = std::clamp(s->selected, 0, n - 1);
    } else {
      s->selected = 0;
    }

    // Compute visible row count from previous frame's box.
    const int box_h = std::max(1, s->box.y_max - s->box.y_min + 1);
    // Reserve rows for search bar + separator.
    const int header_rows = s->show_search ? 2 : 0;
    const int content_h = std::max(1, box_h - header_rows);
    const int visible_rows = std::max(1, content_h / s->row_height);

    // Auto-scroll to keep selected item visible.
    if (s->selected < s->scroll_offset) {
      s->scroll_offset = s->selected;
    }
    if (s->selected >= s->scroll_offset + visible_rows) {
      s->scroll_offset = s->selected - visible_rows + 1;
    }
    const int max_offset = std::max(0, n - visible_rows);
    s->scroll_offset = std::clamp(s->scroll_offset, 0, max_offset);

    Elements rows;
    rows.reserve(static_cast<size_t>(visible_rows + header_rows + 1));

    // ── Search bar ────────────────────────────────────────────────────────
    if (s->show_search) {
      std::string count_str =
          std::to_string(n) + "/" +
          (s->items ? std::to_string(s->items->size()) : "0");
      rows.push_back(hbox({
          text(" ") | color(theme.text_muted),
          text("Search: ") | color(theme.text_muted),
          text(s->query) | bold | color(theme.primary),
          text("\u2587") | color(theme.primary),  // ▇ cursor
          filler(),
          text(count_str) | color(theme.text_muted),
          text(" "),
      }));
      rows.push_back(separatorLight());
    }

    // ── Visible items ─────────────────────────────────────────────────────
    if (n == 0) {
      rows.push_back(text(s->query.empty() ? " (no items)" : " (no matches)") |
                     color(theme.text_muted));
    } else {
      const int start = s->scroll_offset;
      const int end = std::min(start + visible_rows, n);
      for (int i = start; i < end; ++i) {
        const size_t di = s->filtered[static_cast<size_t>(i)];
        const bool sel = (i == s->selected);
        const T& item = (*s->items)[di];
        if (s->render_fn) {
          rows.push_back(s->render_fn(item, sel));
        } else {
          rows.push_back(DefaultRenderItem(s, item, sel, theme));
        }
      }
      // Pad remaining rows so the component keeps a stable height.
      const int rendered = end - start;
      for (int p = rendered; p < visible_rows; ++p) {
        rows.push_back(text(""));
      }
    }

    // Compose: vbox + reflect to capture box size for the next frame.
    return vbox(std::move(rows)) | reflect(s->box) | flex;
  }

  // Handle keyboard events; return true if consumed.
  static bool HandleEvent(const std::shared_ptr<State>& s, ftxui::Event event) {
    if (s->filtered_dirty) {
      RebuildFiltered(s);
    }

    const int n = static_cast<int>(s->filtered.size());
    const int box_h = std::max(1, s->box.y_max - s->box.y_min + 1);
    const int header_rows = s->show_search ? 2 : 0;
    const int content_h = std::max(1, box_h - header_rows);
    const int visible_rows = std::max(1, content_h / s->row_height);
    const int page_step = std::max(1, visible_rows - 1);

    // ── Navigation ────────────────────────────────────────────────────────
    if (event == ftxui::Event::ArrowUp && s->selected > 0) {
      s->selected--;
      FireSelect(s, n);
      return true;
    }
    if (event == ftxui::Event::ArrowDown && s->selected < n - 1) {
      s->selected++;
      FireSelect(s, n);
      return true;
    }
    if (event == ftxui::Event::PageUp) {
      s->selected = std::max(0, s->selected - page_step);
      FireSelect(s, n);
      return true;
    }
    if (event == ftxui::Event::PageDown) {
      s->selected = std::min(n - 1, s->selected + page_step);
      FireSelect(s, n);
      return true;
    }
    if (event == ftxui::Event::Home) {
      s->selected = 0;
      s->scroll_offset = 0;
      FireSelect(s, n);
      return true;
    }
    if (event == ftxui::Event::End) {
      s->selected = std::max(0, n - 1);
      FireSelect(s, n);
      return true;
    }

    // ── Search / filter input ─────────────────────────────────────────────
    if (s->show_search) {
      if (event == ftxui::Event::Escape) {
        s->query.clear();
        s->selected = 0;
        s->scroll_offset = 0;
        s->filtered_dirty = true;
        return true;
      }
      if (event == ftxui::Event::Backspace) {
        if (!s->query.empty()) {
          s->query.pop_back();
          s->selected = 0;
          s->scroll_offset = 0;
          s->filtered_dirty = true;
        }
        return true;
      }
      if (event.is_character()) {
        s->query += event.character();
        s->selected = 0;
        s->scroll_offset = 0;
        s->filtered_dirty = true;
        return true;
      }
    }

    return false;
  }

  static void FireSelect(const std::shared_ptr<State>& s, int n) {
    if (s->on_select && n > 0 && s->selected >= 0 && s->selected < n) {
      const size_t di = s->filtered[static_cast<size_t>(s->selected)];
      s->on_select(di, (*s->items)[di]);
    }
  }

  // ── Data ───────────────────────────────────────────────────────────────────

  std::shared_ptr<State> state_ = std::make_shared<State>();
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_VIRTUAL_LIST_HPP
