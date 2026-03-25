// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_DATA_CONTEXT_HPP
#define FTXUI_UI_DATA_CONTEXT_HPP

#include <memory>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/reactive.hpp"

namespace ftxui::ui {

// ---------------------------------------------------------------------------
// DataContext<T>
// ---------------------------------------------------------------------------

/// @brief Provides a typed reactive value to an entire component subtree
/// without threading it through every level.
///
/// Inspired by React's Context API.
///
/// ### Provider side
/// @code
/// struct AppState { std::string user; bool dark_mode = true; };
///
/// auto ctx  = DataContext<AppState>::Create(AppState{"admin", true});
/// auto root = ctx->Provide(my_app_component);
/// RunFullscreen(root);
/// @endcode
///
/// ### Consumer side (from anywhere inside the subtree during render/event)
/// @code
/// auto state = DataContext<AppState>::Use();
/// if (state) {
///     state->Subscribe([](const AppState& s){ /* react */ });
///     AppState current = state->Get();
/// }
/// @endcode
///
/// @note Context is thread-local.  Provide() pushes the value onto the
///       per-thread stack before delegating Render / OnEvent to the inner
///       component, then pops it afterwards.  Nested contexts of the same
///       type shadow outer ones (inner wins).
///
/// @tparam T  The context value type.
/// @ingroup ui
template <typename T>
class DataContext {
 public:
  // ── Factory ────────────────────────────────────────────────────────────────

  /// @brief Create a new context with the given initial value.
  static std::shared_ptr<DataContext<T>> Create(T initial = T{}) {
    return std::make_shared<DataContext<T>>(std::move(initial));
  }

  // ── Constructors ───────────────────────────────────────────────────────────

  DataContext()
      : reactive_(std::make_shared<ftxui::ui::Reactive<T>>()) {}

  explicit DataContext(T initial)
      : reactive_(std::make_shared<ftxui::ui::Reactive<T>>(std::move(initial))) {
  }

  // ── Provide ────────────────────────────────────────────────────────────────

  /// @brief Wrap @p inner in a component that makes this context visible to
  /// all child components during Render and OnEvent.
  ftxui::Component Provide(ftxui::Component inner) const {
    return ftxui::Make<ContextWrapper>(std::move(inner), reactive_);
  }

  // ── Consume ────────────────────────────────────────────────────────────────

  /// @brief Read the innermost DataContext<T> value on the current thread.
  ///
  /// Returns nullptr if no provider is active (i.e., called outside a
  /// component subtree wrapped with Provide()).
  static std::shared_ptr<ftxui::ui::Reactive<T>> Use() {
    auto& stack = GetStack();
    if (stack.empty()) {
      return nullptr;
    }
    return stack.back();
  }

  // ── Direct reactive access ─────────────────────────────────────────────────

  /// @brief Access the underlying reactive value.
  std::shared_ptr<ftxui::ui::Reactive<T>> GetReactive() const {
    return reactive_;
  }

 private:
  // ── Thread-local context stack ─────────────────────────────────────────────

  static std::vector<std::shared_ptr<ftxui::ui::Reactive<T>>>& GetStack() {
    static thread_local std::vector<std::shared_ptr<ftxui::ui::Reactive<T>>> stack;
    return stack;
  }

  // ── Inner wrapper component ────────────────────────────────────────────────

  class ContextWrapper : public ftxui::ComponentBase {
   public:
    ContextWrapper(ftxui::Component child,
                   std::shared_ptr<ftxui::ui::Reactive<T>> ctx)
        : child_(child), ctx_(std::move(ctx)) {
      Add(child_);
    }

    ftxui::Element OnRender() override {
      GetStack().push_back(ctx_);
      auto e = child_->Render();
      GetStack().pop_back();
      return e;
    }

    bool OnEvent(ftxui::Event event) override {
      GetStack().push_back(ctx_);
      bool handled = ComponentBase::OnEvent(event);
      GetStack().pop_back();
      return handled;
    }

    bool Focusable() const override { return child_->Focusable(); }

   private:
    ftxui::Component child_;
    std::shared_ptr<ftxui::ui::Reactive<T>> ctx_;
  };

  // ── Data ───────────────────────────────────────────────────────────────────

  std::shared_ptr<ftxui::ui::Reactive<T>> reactive_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_DATA_CONTEXT_HPP
