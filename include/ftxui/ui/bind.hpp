// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_BIND_HPP
#define FTXUI_UI_BIND_HPP

#include <functional>
#include <memory>

#include "ftxui/ui/reactive.hpp"

namespace ftxui::ui {

// ---------------------------------------------------------------------------
// Bind<T>
// ---------------------------------------------------------------------------

/// @brief A two-way binding between a Reactive<T> and a UI component.
///
/// Reading a Bind<T> returns the current reactive value.  Writing through
/// Bind<T> updates the underlying Reactive<T> and notifies all subscribers.
///
/// @code
/// auto name = std::make_shared<Reactive<std::string>>("Alice");
/// auto b    = MakeBind(name);
///
/// b.Set("Bob");           // updates the Reactive
/// std::string v = b;      // implicit conversion → "Bob"
///
/// b.Subscribe([](const std::string& s){ /* ... */ });
/// @endcode
///
/// @ingroup ui
template <typename T>
class Bind {
 public:
  /// @brief Construct an owning Bind – creates an internal Reactive<T>.
  explicit Bind(T initial = T{})
      : reactive_(std::make_shared<ftxui::ui::Reactive<T>>(std::move(initial))) {
  }

  /// @brief Construct a non-owning Bind wrapping an existing Reactive<T>.
  explicit Bind(std::shared_ptr<ftxui::ui::Reactive<T>> reactive)
      : reactive_(std::move(reactive)) {}

  // ── Value access ─────────────────────────────────────────────────────────

  /// @brief Read the current value.
  T Get() const { return reactive_->Get(); }

  /// @brief Update the value and notify all subscribers.
  void Set(T value) { reactive_->Set(std::move(value)); }

  /// @brief Register a callback invoked on every change.
  void Subscribe(std::function<void(const T&)> fn) {
    reactive_->OnChange(std::move(fn));
  }

  /// @brief Access the underlying Reactive<T> shared_ptr.
  std::shared_ptr<ftxui::ui::Reactive<T>> AsReactive() const {
    return reactive_;
  }

  /// @brief Implicit conversion to T for ergonomic use.
  operator T() const { return Get(); }  // NOLINT

 private:
  std::shared_ptr<ftxui::ui::Reactive<T>> reactive_;
};

// ---------------------------------------------------------------------------
// Factory helpers
// ---------------------------------------------------------------------------

/// @brief Create a Bind<T> owning a freshly-constructed Reactive<T>.
template <typename T>
Bind<T> MakeBind(T initial = T{}) {
  return Bind<T>(std::move(initial));
}

/// @brief Create a Bind<T> wrapping an existing Reactive<T> shared_ptr.
template <typename T>
Bind<T> MakeBind(std::shared_ptr<ftxui::ui::Reactive<T>> r) {
  return Bind<T>(std::move(r));
}

// ---------------------------------------------------------------------------
// MakeLens – read/write a single field of a model through a Bind<T>
// ---------------------------------------------------------------------------

/// @brief Return a Bind<F> that reads and writes field @p field of the model
/// @p model.
///
/// This is the "lens" pattern: given a @c Bind<T> and a member pointer, you
/// get a derived @c Bind<F> that stays in sync bidirectionally.
///
/// @code
/// struct Point { int x, y; };
/// auto model = MakeBind<Point>({3, 7});
/// auto x_bind = MakeLens(model, &Point::x);
///
/// x_bind.Set(42);            // model.Get().x == 42
/// model.Set({0, 0});         // x_bind.Get() == 0
/// @endcode
///
/// @tparam T  The model (struct) type.
/// @tparam F  The field type (deduced from the member pointer).
///
/// @ingroup ui
template <typename T, typename F>
Bind<F> MakeLens(Bind<T>& model, F T::*field) {
  // Create a Reactive<F> initialised from the model's current field value.
  auto r = std::make_shared<ftxui::ui::Reactive<F>>(model.Get().*field);

  // Shared flag prevents the two listeners from chasing each other.
  auto suppressing = std::make_shared<bool>(false);

  auto model_reactive = model.AsReactive();
  auto weak_r         = std::weak_ptr<ftxui::ui::Reactive<F>>(r);

  // Model → field: when the whole model changes, update the field reactive.
  model.Subscribe([weak_r, field, suppressing](const T& val) {
    if (*suppressing) {
      return;
    }
    if (auto rr = weak_r.lock()) {
      *suppressing = true;
      rr->Set(val.*field);
      *suppressing = false;
    }
  });

  // Field → model: when the field reactive changes, write it back.
  r->OnChange([model_reactive, field, suppressing](const F& val) {
    if (*suppressing) {
      return;
    }
    *suppressing = true;
    T current = model_reactive->Get();
    current.*field = val;
    model_reactive->Set(std::move(current));
    *suppressing = false;
  });

  return Bind<F>(r);
}

}  // namespace ftxui::ui

#endif  // FTXUI_UI_BIND_HPP
