// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_ANIMATION_HPP
#define FTXUI_UI_ANIMATION_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ftxui::ui {

/// @brief Easing functions for smooth animations.
/// Each function takes a normalized progress value t in [0, 1] and returns
/// a transformed value, also nominally in [0, 1].
namespace Easing {

float Linear(float t);
float EaseIn(float t);     ///< Quadratic ease in
float EaseOut(float t);    ///< Quadratic ease out
float EaseInOut(float t);  ///< Smooth step (cubic Hermite)
float Bounce(float t);     ///< Bouncy overshoot
float Elastic(float t);    ///< Elastic snap
float Spring(float t);     ///< Spring physics approximation

}  // namespace Easing

// ---------------------------------------------------------------------------
// Tween
// ---------------------------------------------------------------------------

/// @brief A single animated value that interpolates from `from` to `to` over
/// `duration_seconds` using the supplied easing function.
///
/// @code
/// auto fade = std::make_shared<Tween>(0.0f, 1.0f, 0.5f, Easing::EaseOut);
/// fade->Start();
/// // in render: float v = fade->Value();
/// @endcode
///
/// @ingroup ui
class Tween {
 public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  Tween(float from,
        float to,
        float duration_seconds,
        std::function<float(float)> easing = Easing::EaseInOut);

  /// @brief Begin interpolation from the current time.
  void Start();

  /// @brief Reset to the initial state (not started).
  void Reset();

  /// @brief Return true once the full duration has elapsed.
  bool Done() const;

  /// @brief Current interpolated value in [from, to].
  float Value() const;

  /// @brief Normalized progress in [0, 1].
  float Progress() const;

  /// @brief Swap from/to so the next Start() goes in the opposite direction.
  void Reverse();

  /// @brief Register a callback invoked once when the tween finishes.
  Tween& OnComplete(std::function<void()> fn);

  // Internal: called by AnimationLoop to advance time.
  void Tick();

 private:
  float from_;
  float to_;
  float duration_;
  std::function<float(float)> easing_;
  std::function<void()> on_complete_;

  bool started_{false};
  bool done_{false};
  bool complete_fired_{false};
  TimePoint start_time_;
};

// ---------------------------------------------------------------------------
// AnimationLoop
// ---------------------------------------------------------------------------

/// @brief Drives Tweens and per-frame callbacks at a configurable frame rate.
///
/// Runs a background thread that wakes at `target_fps` Hz, advances all
/// registered Tweens, calls per-frame callbacks with the delta time, then
/// posts a UI refresh via `App::Active()->Post([] {})`.
///
/// @ingroup ui
class AnimationLoop {
 public:
  /// @brief Meyer's singleton — thread-safe in C++11+.
  static AnimationLoop& Instance();

  /// @brief Set target frame rate.  Clamped to [1, 60].  Default: 30.
  void SetFPS(int fps);

  /// @brief Register a Tween to be driven each frame.
  std::shared_ptr<Tween> Add(std::shared_ptr<Tween> tween);

  /// @brief Register a per-frame callback (delta in seconds).
  /// @returns A handle ID that can be passed to RemoveCallback().
  int OnFrame(std::function<void(float delta_seconds)> callback);

  /// @brief Remove a per-frame callback by its handle ID.
  void RemoveCallback(int id);

  /// @brief Start the background thread (called automatically by Instance()).
  void Start();

  /// @brief Stop the background thread.
  void Stop();

 private:
  AnimationLoop();
  ~AnimationLoop();

  void Run();

  std::atomic<bool> running_{false};
  std::thread thread_;
  mutable std::mutex mutex_;
  int target_fps_{30};
  std::vector<std::shared_ptr<Tween>> tweens_;
  std::map<int, std::function<void(float)>> callbacks_;
  int next_callback_id_{0};
};

// ---------------------------------------------------------------------------
// Free-function helpers
// ---------------------------------------------------------------------------

/// @brief Create a Tween, register it with the AnimationLoop, and Start() it.
std::shared_ptr<Tween> Animate(
    float from,
    float to,
    float duration,
    std::function<float(float)> easing = Easing::EaseInOut);

/// @brief Register a per-frame callback with the global AnimationLoop.
/// @returns Callback ID for use with RemoveFrameCallback().
int OnFrame(std::function<void(float dt)> callback);

/// @brief Remove a per-frame callback registered with OnFrame().
void RemoveFrameCallback(int id);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_ANIMATION_HPP
