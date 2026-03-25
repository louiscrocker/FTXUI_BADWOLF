// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_REACTIVE_DECORATORS_HPP
#define FTXUI_UI_REACTIVE_DECORATORS_HPP

#include <functional>
#include <memory>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/reactive.hpp"
#include "ftxui/ui/reactive_list.hpp"

namespace ftxui::ui {

// ---------------------------------------------------------------------------
// ShowIf / HideIf
// ---------------------------------------------------------------------------

/// @brief Return @p e when @p condition is true, otherwise emptyElement().
///
/// @note Call this inside a Renderer lambda (not at construction time) so the
///       reactive value is re-evaluated on every frame.
///
/// @code
/// Renderer([&]{ return vbox({ ShowIf(spinner, is_loading) }); })
/// @endcode
ftxui::Element ShowIf(ftxui::Element e,
                      std::shared_ptr<Reactive<bool>> condition);

/// @brief Return @p e when @p condition is false, otherwise emptyElement().
ftxui::Element HideIf(ftxui::Element e,
                      std::shared_ptr<Reactive<bool>> condition);

// ---------------------------------------------------------------------------
// BindText
// ---------------------------------------------------------------------------

/// @brief Render the current value of @p text_reactive as a text element.
///
/// @note Call inside a Renderer lambda so the text is refreshed each frame.
///
/// @code
/// Renderer([&]{ return BindText(username_reactive); })
/// @endcode
ftxui::Element BindText(std::shared_ptr<Reactive<std::string>> text_reactive);

/// @brief Render with optional bold decoration.
ftxui::Element BindText(std::shared_ptr<Reactive<std::string>> text_reactive,
                        bool bold);

// ---------------------------------------------------------------------------
// BindColor
// ---------------------------------------------------------------------------

/// @brief Apply the current color from @p color_reactive to element @p e.
ftxui::Element BindColor(
    ftxui::Element e,
    std::shared_ptr<Reactive<ftxui::Color>> color_reactive);

// ---------------------------------------------------------------------------
// ReactiveBox
// ---------------------------------------------------------------------------

/// @brief A Component that calls @p render_fn each frame.
///
/// The @p deps vector of type-erased shared_ptrs is reserved for future
/// explicit dependency tracking; the component already re-renders on every
/// UI event (including those posted by reactive updates).
ftxui::Component ReactiveBox(std::function<ftxui::Element()> render_fn,
                             std::vector<std::shared_ptr<void>> deps = {});

// ---------------------------------------------------------------------------
// ForEach<T>
// ---------------------------------------------------------------------------

/// @brief Create a scrollable list component backed by a ReactiveList<T>.
///
/// The component re-renders automatically whenever the list mutates.
///
/// @param list       The reactive collection to display.
/// @param renderer   Called with (item, is_selected) to produce each row.
/// @param on_select  Optional callback when the user presses Enter on a row.
///
/// @code
/// auto items = MakeReactiveList<std::string>({"alpha","beta","gamma"});
/// auto view  = ForEach(items, [](const std::string& s, bool sel) {
///     return sel ? text(s) | inverted : text(s);
/// });
/// @endcode
///
/// @ingroup ui
template <typename T>
ftxui::Component ForEach(
    std::shared_ptr<ReactiveList<T>> list,
    std::function<ftxui::Element(const T&, bool selected)> renderer,
    std::function<void(const T&)> on_select = nullptr) {
  struct State {
    std::shared_ptr<ReactiveList<T>> list;
    std::function<ftxui::Element(const T&, bool)> renderer;
    std::function<void(const T&)> on_select;
    int selected = 0;
  };

  auto state = std::make_shared<State>();
  state->list = list;
  state->renderer = std::move(renderer);
  state->on_select = std::move(on_select);

  // Subscribe for auto-refresh.
  auto weak_state = std::weak_ptr<State>(state);
  list->OnChange([weak_state](const std::vector<T>&) {
    if (auto s = weak_state.lock()) {
      // Clamp selection after list mutation.
      int n = static_cast<int>(s->list->Size());
      if (n > 0 && s->selected >= n) {
        s->selected = n - 1;
      }
    }
    // PostEvent is already called by ReactiveList::Notify().
  });

  auto comp = ftxui::Renderer([state]() -> ftxui::Element {
    const auto items = state->list->Items();
    const int n = static_cast<int>(items.size());

    if (n > 0 && state->selected >= n) {
      state->selected = n - 1;
    }

    ftxui::Elements elems;
    elems.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
      bool sel = (i == state->selected);
      elems.push_back(state->renderer(items[static_cast<size_t>(i)], sel));
    }
    if (elems.empty()) {
      return ftxui::text(" (empty)") | ftxui::dim;
    }
    return ftxui::vbox(std::move(elems));
  });

  return ftxui::CatchEvent(comp, [state](ftxui::Event event) -> bool {
    const int n = static_cast<int>(state->list->Size());
    if (n == 0) {
      return false;
    }
    if (event == ftxui::Event::ArrowUp && state->selected > 0) {
      state->selected--;
      return true;
    }
    if (event == ftxui::Event::ArrowDown && state->selected < n - 1) {
      state->selected++;
      return true;
    }
    if (event == ftxui::Event::Return && state->on_select) {
      const auto items = state->list->Items();
      if (state->selected < static_cast<int>(items.size())) {
        state->on_select(items[static_cast<size_t>(state->selected)]);
      }
      return true;
    }
    return false;
  });
}

}  // namespace ftxui::ui

#endif  // FTXUI_UI_REACTIVE_DECORATORS_HPP
