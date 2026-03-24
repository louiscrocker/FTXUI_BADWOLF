// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_BACKGROUND_TASK_HPP
#define FTXUI_UI_BACKGROUND_TASK_HPP

#include <functional>
#include <memory>
#include <thread>
#include <atomic>

#include "ftxui/component/app.hpp"
#include "ftxui/component/task.hpp"

namespace ftxui::ui {

/// @brief Run @p work on a background thread; call @p on_complete on the UI
///        thread when done.
///
/// The active `App*` is captured before the thread is spawned, so the
/// completion callback is always posted to the correct event loop via
/// `App::Post(Closure)`.  If no App is active at spawn time, the completion
/// is called directly on the background thread (use with care).
///
/// @code
/// bool loading = false;
///
/// // Start async work:
/// loading = true;
/// ui::RunAsync<std::string>(
///     []() -> std::string {
///         return fetch_from_network();     // runs on background thread
///     },
///     [&](std::string result) {            // runs on UI thread
///         loading = false;
///         data = std::move(result);
///     });
/// @endcode
///
/// @tparam T  Return type of the work function.
/// @ingroup ui
template <typename T>
void RunAsync(std::function<T()> work, std::function<void(T)> on_complete) {
  App* app = App::Active();  // capture before thread spawn

  std::thread([work = std::move(work), on_complete = std::move(on_complete),
               app]() mutable {
    T result = work();

    if (app) {
      // Post back to the UI thread via the App's task queue.
      Closure closure = [result = std::move(result),
                         on_complete = std::move(on_complete)]() mutable {
        on_complete(std::move(result));
      };
      app->Post(std::move(closure));
    } else {
      on_complete(std::move(result));
    }
  }).detach();
}

/// @brief Overload for work that returns void.
inline void RunAsync(std::function<void()> work,
                     std::function<void()> on_complete = {}) {
  App* app = App::Active();

  std::thread([work = std::move(work), on_complete = std::move(on_complete),
               app]() mutable {
    work();

    if (app) {
      Closure closure = on_complete ? std::move(on_complete) : [] {};
      app->Post(std::move(closure));
    } else if (on_complete) {
      on_complete();
    }
  }).detach();
}

/// @brief A cancellable background task handle.
///
/// @code
/// auto handle = ui::MakeAsync<int>(
///     []{ return compute(); },
///     [&](int result){ display(result); });
///
/// // Later, if no longer needed:
/// handle->Cancel();
/// @endcode
template <typename T>
class AsyncHandle {
 public:
  AsyncHandle() : cancelled_(std::make_shared<std::atomic<bool>>(false)) {}

  void Cancel() { cancelled_->store(true); }
  bool IsCancelled() const { return cancelled_->load(); }

  /// @brief Internal: start the background thread (used by MakeAsync).
  void Start(std::function<T()> work, std::function<void(T)> on_complete) {
    App* app = App::Active();
    auto cancelled = cancelled_;

    std::thread([work = std::move(work), on_complete = std::move(on_complete),
                 app, cancelled]() mutable {
      T result = work();

      if (cancelled->load()) return;

      if (app) {
        Closure closure = [result = std::move(result),
                           on_complete = std::move(on_complete),
                           cancelled]() mutable {
          if (!cancelled->load()) on_complete(std::move(result));
        };
        app->Post(std::move(closure));
      } else if (!cancelled->load()) {
        on_complete(std::move(result));
      }
    }).detach();
  }

 private:
  std::shared_ptr<std::atomic<bool>> cancelled_;
};

/// @brief Start a cancellable background task and return a handle.
template <typename T>
std::shared_ptr<AsyncHandle<T>> MakeAsync(std::function<T()> work,
                                           std::function<void(T)> on_complete) {
  auto handle = std::make_shared<AsyncHandle<T>>();
  handle->Start(std::move(work), std::move(on_complete));
  return handle;
}

}  // namespace ftxui::ui

#endif  // FTXUI_UI_BACKGROUND_TASK_HPP
