// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_STATE_HPP
#define FTXUI_UI_STATE_HPP

#include <functional>
#include <memory>

#include "ftxui/component/app.hpp"
#include "ftxui/component/event.hpp"

namespace ftxui::ui {

/// @brief A reactive value wrapper that triggers a UI redraw on every mutation.
///
/// `State<T>` wraps a value in a `shared_ptr` so that it can safely be
/// captured by lambdas.  Any call to `Set()` or `Mutate()` automatically posts
/// `Event::Custom` to the active `App`, causing the screen to redraw.
///
/// @code
/// auto count = ui::State<int>(0);
///
/// auto comp = Renderer([&count] {
///     return text("Count: " + std::to_string(*count)) | border | center;
/// });
/// comp |= CatchEvent([&count](Event event) {
///     if (event.is_character() && event.character() == "+") {
///         count.Mutate([](int& v){ v++; });
///         return true;
///     }
///     return false;
/// });
/// @endcode
///
/// @ingroup ui
template <typename T>
class State {
 public:
  explicit State(T initial = T{})
      : value_(std::make_shared<T>(std::move(initial))) {}

  // ── Value access ───────────────────────────────────────────────────────────

  T Get() const { return *value_; }

  const T& Ref() const { return *value_; }

  T* Ptr() { return value_.get(); }
  const T* Ptr() const { return value_.get(); }

  T& operator*() { return *value_; }
  const T& operator*() const { return *value_; }

  T* operator->() { return value_.get(); }
  const T* operator->() const { return value_.get(); }

  // ── Mutation (triggers redraw) ─────────────────────────────────────────────

  /// @brief Replace the value and request a redraw.
  void Set(T new_value) {
    *value_ = std::move(new_value);
    Invalidate();
  }

  /// @brief Apply @p fn to the value in-place and request a redraw.
  void Mutate(std::function<void(T&)> fn) {
    fn(*value_);
    Invalidate();
  }

  // ── Sharing ────────────────────────────────────────────────────────────────

  /// @brief Return a shared handle that co-owns the same value.
  ///
  /// Useful when the state must be readable from a background thread:
  /// @code
  /// auto handle = my_state.Share();
  /// std::thread([handle]{ handle->Set(42); }).detach();
  /// @endcode
  State<T> Share() const { return State<T>(value_); }

 private:
  explicit State(std::shared_ptr<T> ptr) : value_(std::move(ptr)) {}

  void Invalidate() {
    if (App* app = App::Active()) {
      app->PostEvent(Event::Custom);
    }
  }

  std::shared_ptr<T> value_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_STATE_HPP
