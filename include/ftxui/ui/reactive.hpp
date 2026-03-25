// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_REACTIVE_HPP
#define FTXUI_UI_REACTIVE_HPP

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <utility>

#include "ftxui/component/app.hpp"
#include "ftxui/component/event.hpp"

namespace ftxui::ui {

namespace internal {
// Thread-local batch depth: when > 0, Notify() defers UI refresh.
inline thread_local int batch_depth = 0;
}  // namespace internal

// ---------------------------------------------------------------------------
// Reactive<T>
// ---------------------------------------------------------------------------

/// @brief A value wrapper that fires listener callbacks and requests a UI
/// redraw whenever the value changes.
///
/// Thread-safe for concurrent writes.  Reads via Get() / operator* return a
/// const reference to the internal value; callers should not hold the
/// reference across a point where another thread may call Set() or Update().
/// In practice FTXUI renders on the UI thread after the write + PostEvent
/// sequence has completed, so this is safe under the normal usage pattern.
///
/// @code
/// Reactive<int> count(0);
///
/// // Background thread increments:
/// std::thread([&]{ count.Update([](int& v){ v++; }); }).join();
///
/// // Render callback reads:
/// Renderer([&]{ return text(std::to_string(*count)); });
/// @endcode
///
/// @ingroup ui
template <typename T>
class Reactive {
 public:
  explicit Reactive(T initial = T{}) : value_(std::move(initial)) {}

  // Non-copyable, non-movable – listeners may capture a pointer to *this.
  Reactive(const Reactive&) = delete;
  Reactive& operator=(const Reactive&) = delete;
  Reactive(Reactive&&) = delete;
  Reactive& operator=(Reactive&&) = delete;

  // ── Value access ───────────────────────────────────────────────────────────

  /// @brief Read the current value.
  /// Callers should not hold the reference across a potential concurrent
  /// Set() / Update() call on another thread.
  const T& Get() const { return value_; }

  /// @brief Return a mutable reference for in-place modification.
  /// Does NOT trigger listeners – call Notify() explicitly afterwards.
  T& GetMut() { return value_; }

  // ── Mutation (triggers listeners + UI redraw) ──────────────────────────────

  /// @brief Replace the value and notify all listeners.
  void Set(const T& val) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      value_ = val;
    }
    Notify();
  }

  /// @brief Replace the value (move) and notify all listeners.
  void Set(T&& val) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      value_ = std::move(val);
    }
    Notify();
  }

  /// @brief Apply @p fn to the value in-place and notify all listeners.
  /// @code
  /// count.Update([](int& v){ v++; });
  /// @endcode
  void Update(std::function<void(T&)> fn) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      fn(value_);
    }
    Notify();
  }

  // ── Listener management ────────────────────────────────────────────────────

  /// @brief Register a callback invoked on every change.  Returns a handle ID
  /// that can be passed to RemoveListener().
  int OnChange(std::function<void(const T&)> fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_id_++;
    listeners_[id] = std::move(fn);
    return id;
  }

  /// @brief Remove a callback previously registered with OnChange().
  void RemoveListener(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.erase(id);
  }

  /// @brief Fire all listeners without changing the value.
  /// Also posts a UI refresh unless a batch is in progress.
  void Notify() {
    // Copy listeners and current value while holding the lock.
    std::map<int, std::function<void(const T&)>> snapshot;
    T val_snapshot;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot = listeners_;
      val_snapshot = value_;
    }

    for (auto& [id, fn] : snapshot) {
      fn(val_snapshot);
    }

    if (internal::batch_depth > 0) {
      return;
    }
    if (App* app = App::Active()) {
      app->PostEvent(Event::Custom);
    }
  }

  // ── Convenience operators ──────────────────────────────────────────────────

  /// @brief Equivalent to Set(val).
  Reactive& operator=(const T& val) {
    Set(val);
    return *this;
  }

  /// @brief Equivalent to Get().
  const T& operator*() const { return value_; }

  /// @brief Member access on the stored value.
  const T* operator->() const { return &value_; }

 private:
  mutable std::mutex mutex_;
  T value_;
  int next_id_ = 0;
  std::map<int, std::function<void(const T&)>> listeners_;
};

// ---------------------------------------------------------------------------
// Computed<T>  (forward declaration – full definition below MakeComputed)
// ---------------------------------------------------------------------------

template <typename T>
class Computed;

// ---------------------------------------------------------------------------
// MakeComputed
// ---------------------------------------------------------------------------

/// @brief Create a derived reactive value that recomputes whenever any of the
/// source Reactive<Dep> values change.
///
/// @code
/// Reactive<float> latency(0.f);
/// auto label = MakeComputed([](float v){ return std::to_string(v) + "ms"; },
///                            latency);
/// // *label == "0ms"
/// latency.Set(12.5f);
/// // *label == "12.500000ms"
/// @endcode
///
/// @note The returned Computed<T> holds shared ownership of its internal state.
///       If it is destroyed before its source Reactive objects, future change
///       callbacks become no-ops (via weak_ptr).  Source Reactive objects must
///       outlive the Computed.
///
/// @ingroup ui
template <typename Fn, typename... Deps>
auto MakeComputed(Fn&& fn, Reactive<Deps>&... deps);

// ---------------------------------------------------------------------------
// Computed<T>  (full definition)
// ---------------------------------------------------------------------------

/// @brief A read-only reactive value derived from one or more Reactive sources.
///
/// Use MakeComputed() to construct — it handles type deduction.
///
/// @ingroup ui
template <typename T>
class Computed {
 public:
  /// @brief Direct construction: pass compute function followed by dep refs.
  /// Prefer MakeComputed() for cleaner syntax.
  template <typename Fn, typename... Deps>
  explicit Computed(Fn&& fn, Reactive<Deps>&... deps) {
    auto inner = std::make_shared<Reactive<T>>(std::invoke(fn, deps.Get()...));
    inner_ = inner;

    auto weak = std::weak_ptr<Reactive<T>>(inner);
    auto fn_ptr = std::make_shared<std::decay_t<Fn>>(std::forward<Fn>(fn));
    // Store raw pointers to each dep; deps must outlive this Computed.
    auto dep_ptrs = std::make_shared<std::tuple<Reactive<Deps>*...>>(&deps...);

    auto update = [weak, fn_ptr, dep_ptrs]() {
      if (auto v = weak.lock()) {
        T new_val = std::apply(
            [&](auto*... dep) { return std::invoke(*fn_ptr, dep->Get()...); },
            *dep_ptrs);
        v->Set(std::move(new_val));
      }
    };

    // Register on each dep using a fold expression.
    (void)std::initializer_list<int>{
        (deps.OnChange([update](const Deps& /*unused*/) { update(); }), 0)...};
  }

  /// @brief Read the computed value.
  const T& Get() const { return inner_->Get(); }

  /// @brief Equivalent to Get().
  const T& operator*() const { return Get(); }

  /// @brief Member access on the computed value.
  const T* operator->() const { return &inner_->Get(); }

 private:
  friend struct ComputedFactory;
  explicit Computed(std::shared_ptr<Reactive<T>> inner)
      : inner_(std::move(inner)) {}

  std::shared_ptr<Reactive<T>> inner_;
};

// ---------------------------------------------------------------------------
// MakeComputed  (definition – placed after Computed is fully defined)
// ---------------------------------------------------------------------------

template <typename Fn, typename... Deps>
auto MakeComputed(Fn&& fn, Reactive<Deps>&... deps) {
  using ResultT = std::invoke_result_t<Fn, const Deps&...>;
  return Computed<ResultT>(std::forward<Fn>(fn), deps...);
}

// ---------------------------------------------------------------------------
// ReactiveGroup
// ---------------------------------------------------------------------------

/// @brief Defers listener notifications across a batch of mutations.
///
/// @code
/// ReactiveGroup group;
/// group.BeginBatch();
/// x.Set(1);
/// y.Set(2);
/// group.EndBatch();  // fires one UI refresh instead of two
/// @endcode
///
/// @note BatchDepth is thread-local, so batches on different threads are
///       independent.
///
/// @ingroup ui
class ReactiveGroup {
 public:
  /// @brief Suppress notifications until EndBatch() is called.
  void BeginBatch() { ++internal::batch_depth; }

  /// @brief Re-enable notifications and post a single UI refresh.
  void EndBatch() {
    if (--internal::batch_depth <= 0) {
      internal::batch_depth = 0;
      if (App* app = App::Active()) {
        app->PostEvent(Event::Custom);
      }
    }
  }
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_REACTIVE_HPP
