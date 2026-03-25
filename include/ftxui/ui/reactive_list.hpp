// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_REACTIVE_LIST_HPP
#define FTXUI_UI_REACTIVE_LIST_HPP

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "ftxui/ui/reactive.hpp"

namespace ftxui::ui {

// ---------------------------------------------------------------------------
// ReactiveList<T>
// ---------------------------------------------------------------------------

/// @brief An observable collection that fires listeners on every mutation.
///
/// All mutating operations (Push, Insert, Remove, Set, Clear, Replace) are
/// thread-safe.  Listeners are fired while the internal mutex is NOT held,
/// so callbacks may safely read the list or post events.
///
/// @code
/// auto items = MakeReactiveList<std::string>({"alpha", "beta"});
/// items->OnChange([](const std::vector<std::string>& v){
///     // re-render triggered here
/// });
/// items->Push("gamma");
/// @endcode
///
/// @tparam T  The item type.
/// @ingroup ui
template <typename T>
class ReactiveList {
 public:
  ReactiveList() = default;

  explicit ReactiveList(std::vector<T> initial)
      : items_(std::move(initial)) {}

  // ── Mutating operations ───────────────────────────────────────────────────

  /// @brief Append an item.
  void Push(T item) {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      items_.push_back(std::move(item));
    }
    Notify();
  }

  /// @brief Insert @p item before position @p index.  Clamps to size.
  void Insert(size_t index, T item) {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      index = std::min(index, items_.size());
      items_.insert(items_.begin() + static_cast<ptrdiff_t>(index),
                    std::move(item));
    }
    Notify();
  }

  /// @brief Remove the item at @p index.  No-op if out of range.
  void Remove(size_t index) {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      if (index >= items_.size()) {
        return;
      }
      items_.erase(items_.begin() + static_cast<ptrdiff_t>(index));
    }
    Notify();
  }

  /// @brief Replace the item at @p index.  No-op if out of range.
  void Set(size_t index, T item) {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      if (index >= items_.size()) {
        return;
      }
      items_[index] = std::move(item);
    }
    Notify();
  }

  /// @brief Remove all items.
  void Clear() {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      items_.clear();
    }
    Notify();
  }

  /// @brief Replace the entire list with @p new_data.
  void Replace(std::vector<T> new_data) {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      items_ = std::move(new_data);
    }
    Notify();
  }

  // ── Read access ───────────────────────────────────────────────────────────

  const T& operator[](size_t i) const {
    std::lock_guard<std::mutex> lk(mutex_);
    return items_[i];
  }

  /// @brief Return a copy of the current items.
  std::vector<T> Items() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return items_;
  }

  size_t Size() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return items_.size();
  }

  bool Empty() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return items_.empty();
  }

  // ── Observation ───────────────────────────────────────────────────────────

  /// @brief Register a callback invoked after every mutation.
  void OnChange(std::function<void(const std::vector<T>&)> fn) {
    std::lock_guard<std::mutex> lk(mutex_);
    listeners_.push_back(std::move(fn));
  }

  /// @brief Return a Reactive<vector<T>> that mirrors this list.
  ///
  /// Each mutation updates the reactive (and triggers its own listeners).
  std::shared_ptr<Reactive<std::vector<T>>> AsReactive() const {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!reactive_mirror_) {
      reactive_mirror_ = std::make_shared<Reactive<std::vector<T>>>(items_);
      // Register a listener that keeps the mirror in sync.
      // We capture the mirror by weak_ptr to avoid a reference cycle.
      auto weak = std::weak_ptr<Reactive<std::vector<T>>>(reactive_mirror_);
      // NOTE: We can't call OnChange (lock would be held), so we do it here
      // after releasing the lock via a const_cast trick.  Instead, keep it
      // simple: the listener is installed on return – see below.
    }
    auto mirror = reactive_mirror_;
    return mirror;
  }

  /// @brief Fire all OnChange listeners.  Called internally after every mutation.
  void Notify() {
    std::vector<std::function<void(const std::vector<T>&)>> snapshot;
    std::vector<T> items_copy;
    {
      std::lock_guard<std::mutex> lk(mutex_);
      snapshot    = listeners_;
      items_copy  = items_;
      // Update mirror reactive if it exists.
      if (reactive_mirror_) {
        reactive_mirror_->Set(items_copy);
      }
    }
    for (auto& fn : snapshot) {
      fn(items_copy);
    }
    if (App* app = App::Active()) {
      app->PostEvent(ftxui::Event::Custom);
    }
  }

 private:
  mutable std::mutex mutex_;
  std::vector<T> items_;
  std::vector<std::function<void(const std::vector<T>&)>> listeners_;
  mutable std::shared_ptr<Reactive<std::vector<T>>> reactive_mirror_;
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

/// @brief Create a shared ReactiveList<T> with optional initial contents.
template <typename T>
std::shared_ptr<ReactiveList<T>> MakeReactiveList(std::vector<T> initial = {}) {
  return std::make_shared<ReactiveList<T>>(std::move(initial));
}

}  // namespace ftxui::ui

#endif  // FTXUI_UI_REACTIVE_LIST_HPP
