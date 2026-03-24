// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_LIST_HPP
#define FTXUI_UI_LIST_HPP

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/theme.hpp"

namespace ftxui::ui {

/// @brief Fluent builder for an interactive, optionally-filterable item list.
///
/// Example:
/// @code
/// std::vector<std::string> items = {"Apple", "Banana", "Cherry"};
///
/// auto list = ui::List<std::string>()
///     .Data(&items)
///     .Render([](const std::string& s, bool sel) -> ftxui::Element {
///         auto e = ftxui::text(s);
///         return sel ? e | ftxui::bold | ftxui::color(ftxui::ui::GetTheme().primary) : e;
///     })
///     .Filterable(true)
///     .OnActivate([](const std::string& s, size_t i){ /* ... */ })
///     .Empty("No items found")
///     .Build();
/// @endcode
///
/// @tparam T  The item type.
/// @ingroup ui
template <typename T>
class List {
 public:
  // ── Data binding ───────────────────────────────────────────────────────────

  /// @brief Bind the data vector. The pointer must outlive the component.
  List& Data(const std::vector<T>* data) {
    state_->data = data;
    return *this;
  }

  // ── Rendering ──────────────────────────────────────────────────────────────

  /// @brief Provide a custom item renderer.
  ///
  /// @param fn  Called for each item with (item, is_selected).  The function
  ///            is responsible for applying any selection highlighting.
  List& Render(std::function<ftxui::Element(const T&, bool)> fn) {
    state_->renderer = std::move(fn);
    return *this;
  }

  // ── Behavior ───────────────────────────────────────────────────────────────

  /// @brief Show a search bar and filter items as the user types. Default: false.
  List& Filterable(bool v) {
    state_->filterable = v;
    return *this;
  }

  // ── Callbacks ──────────────────────────────────────────────────────────────

  /// @brief Called whenever the selected item changes.
  List& OnSelect(std::function<void(const T&, size_t)> fn) {
    state_->on_select = std::move(fn);
    return *this;
  }

  /// @brief Called when the user presses Enter on an item.
  List& OnActivate(std::function<void(const T&, size_t)> fn) {
    state_->on_activate = std::move(fn);
    return *this;
  }

  // ── Empty state ────────────────────────────────────────────────────────────

  /// @brief Text shown when the list is empty (or all items are filtered out).
  List& Empty(std::string_view msg) {
    state_->empty_msg = std::string(msg);
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
  // ── Internal state ─────────────────────────────────────────────────────────

  struct State {
    // Configuration
    const std::vector<T>* data = nullptr;
    std::function<ftxui::Element(const T&, bool)> renderer;
    bool filterable = false;
    std::function<void(const T&, size_t)> on_select;
    std::function<void(const T&, size_t)> on_activate;
    std::string empty_msg = "No items";

    // Runtime
    int selected = 0;
    std::string filter;
  };

  // ── Helpers ────────────────────────────────────────────────────────────────

  // Convert an item to a string for default rendering and filtering.
  static std::string ItemToString(const T& item) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
      return item;
    } else if constexpr (std::is_convertible_v<T, std::string_view>) {
      return std::string(static_cast<std::string_view>(item));
    } else {
      return std::to_string(item);
    }
  }

  static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
  }

  // Return the data indices that pass the current filter.
  static std::vector<size_t> ComputeVisible(const std::shared_ptr<State>& s) {
    if (!s->data) {
      return {};
    }

    std::vector<size_t> indices;
    indices.reserve(s->data->size());

    if (!s->filterable || s->filter.empty()) {
      for (size_t i = 0; i < s->data->size(); ++i) {
        indices.push_back(i);
      }
    } else {
      const std::string filter_lower = ToLower(s->filter);
      for (size_t i = 0; i < s->data->size(); ++i) {
        if (ToLower(ItemToString((*s->data)[i])).find(filter_lower) !=
            std::string::npos) {
          indices.push_back(i);
        }
      }
    }

    return indices;
  }

  // Default item renderer: highlight selected with theme secondary + bold.
  static ftxui::Element DefaultRenderItem(const T& item,
                                          bool selected,
                                          const Theme& theme) {
    ftxui::Element e = ftxui::text(" " + ItemToString(item) + " ");
    if (selected) {
      return e | ftxui::bgcolor(theme.secondary) |
             ftxui::color(ftxui::Color::White) | ftxui::bold;
    }
    return e;
  }

  // Build and return the list Element.
  static ftxui::Element Render(const std::shared_ptr<State>& s) {
    const Theme& theme = GetTheme();
    const auto visible = ComputeVisible(s);

    // Clamp selection into range.
    if (!visible.empty()) {
      s->selected =
          std::max(0, std::min(s->selected, static_cast<int>(visible.size()) - 1));
    }

    ftxui::Elements items;

    // Filter bar
    if (s->filterable) {
      // Show a simple inline search bar with a block cursor.
      ftxui::Element filter_bar = ftxui::hbox({
          ftxui::text(" Search: ") | ftxui::color(theme.text_muted),
          ftxui::text(s->filter) | ftxui::bold | ftxui::color(theme.primary),
          ftxui::text("\u2587") | ftxui::color(theme.primary),  // ▇ cursor
          ftxui::filler(),
      });
      items.push_back(std::move(filter_bar));
      items.push_back(ftxui::separatorLight());
    }

    // Item list
    if (visible.empty()) {
      items.push_back(ftxui::text(s->empty_msg) |
                      ftxui::color(theme.text_muted) | ftxui::hcenter);
    } else {
      for (size_t vi = 0; vi < visible.size(); ++vi) {
        const size_t di = visible[vi];
        const bool sel = (static_cast<int>(vi) == s->selected);
        const T& item = (*s->data)[di];

        ftxui::Element row;
        if (s->renderer) {
          row = s->renderer(item, sel);
        } else {
          row = DefaultRenderItem(item, sel, theme);
        }

        items.push_back(std::move(row));
      }
    }

    return ftxui::vbox(std::move(items)) | ftxui::vscroll_indicator |
           ftxui::yframe | ftxui::flex;
  }

  // Handle keyboard events; return true if consumed.
  static bool HandleEvent(const std::shared_ptr<State>& s,
                          ftxui::Event event) {
    const auto visible = ComputeVisible(s);
    const int n = static_cast<int>(visible.size());

    // Navigation
    if (event == ftxui::Event::ArrowUp && s->selected > 0) {
      s->selected--;
      if (s->on_select && !visible.empty()) {
        s->on_select((*s->data)[visible[static_cast<size_t>(s->selected)]],
                     visible[static_cast<size_t>(s->selected)]);
      }
      return true;
    }
    if (event == ftxui::Event::ArrowDown && s->selected < n - 1) {
      s->selected++;
      if (s->on_select && !visible.empty()) {
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

    // Filter input
    if (s->filterable) {
      if (event == ftxui::Event::Escape) {
        s->filter.clear();
        s->selected = 0;
        return true;
      }
      if (event == ftxui::Event::Backspace) {
        if (!s->filter.empty()) {
          // Remove last UTF-8 code unit (safe for ASCII filter strings).
          s->filter.pop_back();
          s->selected = 0;
        }
        return true;
      }
      if (event.is_character()) {
        s->filter += event.character();
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

#endif  // FTXUI_UI_LIST_HPP
