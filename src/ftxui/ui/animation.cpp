// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/animation.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "ftxui/component/app.hpp"

namespace ftxui::ui {

// ---------------------------------------------------------------------------
// Easing
// ---------------------------------------------------------------------------

namespace Easing {

float Linear(float t) {
  return t;
}

float EaseIn(float t) {
  return t * t;
}

float EaseOut(float t) {
  return t * (2.0f - t);
}

float EaseInOut(float t) {
  return t * t * (3.0f - 2.0f * t);
}

float Bounce(float t) {
  // Inspired by Robert Penner's bounce easing.
  if (t < 1.0f / 2.75f) {
    return 7.5625f * t * t;
  } else if (t < 2.0f / 2.75f) {
    t -= 1.5f / 2.75f;
    return 7.5625f * t * t + 0.75f;
  } else if (t < 2.5f / 2.75f) {
    t -= 2.25f / 2.75f;
    return 7.5625f * t * t + 0.9375f;
  } else {
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
  }
}

float Elastic(float t) {
  if (t <= 0.0f) {
    return 0.0f;
  }
  if (t >= 1.0f) {
    return 1.0f;
  }
  constexpr float kPi = 3.14159265358979323846f;
  constexpr float p = 0.3f;
  return std::pow(2.0f, -10.0f * t) *
             std::sin((t - p / 4.0f) * (2.0f * kPi) / p) +
         1.0f;
}

float Spring(float t) {
  constexpr float kPi = 3.14159265358979323846f;
  // Damped harmonic oscillator approximation.
  return 1.0f - std::exp(-6.0f * t) * std::cos(2.0f * kPi * t);
}

}  // namespace Easing

// ---------------------------------------------------------------------------
// Tween
// ---------------------------------------------------------------------------

Tween::Tween(float from,
             float to,
             float duration_seconds,
             std::function<float(float)> easing)
    : from_(from),
      to_(to),
      duration_(duration_seconds),
      easing_(std::move(easing)) {}

void Tween::Start() {
  start_time_ = Clock::now();
  started_ = true;
  done_ = false;
  complete_fired_ = false;
}

void Tween::Reset() {
  started_ = false;
  done_ = false;
  complete_fired_ = false;
}

bool Tween::Done() const {
  return done_;
}

float Tween::Progress() const {
  if (!started_) {
    return 0.0f;
  }
  if (done_) {
    return 1.0f;
  }
  if (duration_ <= 0.0f) {
    return 1.0f;
  }
  auto elapsed =
      std::chrono::duration<float>(Clock::now() - start_time_).count();
  float t = elapsed / duration_;
  return std::max(0.0f, std::min(1.0f, t));
}

float Tween::Value() const {
  float p = Progress();
  float eased = easing_ ? easing_(p) : p;
  return from_ + (to_ - from_) * eased;
}

void Tween::Reverse() {
  std::swap(from_, to_);
  // Reset state so a subsequent Start() runs forward from new `from_`.
  started_ = false;
  done_ = false;
  complete_fired_ = false;
}

Tween& Tween::OnComplete(std::function<void()> fn) {
  on_complete_ = std::move(fn);
  return *this;
}

void Tween::Tick() {
  if (!started_ || done_) {
    return;
  }
  if (duration_ <= 0.0f) {
    done_ = true;
  } else {
    auto elapsed =
        std::chrono::duration<float>(Clock::now() - start_time_).count();
    if (elapsed >= duration_) {
      done_ = true;
    }
  }
  if (done_ && !complete_fired_) {
    complete_fired_ = true;
    if (on_complete_) {
      on_complete_();
    }
  }
}

// ---------------------------------------------------------------------------
// AnimationLoop
// ---------------------------------------------------------------------------

AnimationLoop& AnimationLoop::Instance() {
  static AnimationLoop instance;
  return instance;
}

AnimationLoop::AnimationLoop() {
  Start();
}

AnimationLoop::~AnimationLoop() {
  Stop();
}

void AnimationLoop::SetFPS(int fps) {
  std::lock_guard<std::mutex> lock(mutex_);
  target_fps_ = std::max(1, std::min(60, fps));
}

std::shared_ptr<Tween> AnimationLoop::Add(std::shared_ptr<Tween> tween) {
  std::lock_guard<std::mutex> lock(mutex_);
  tweens_.push_back(tween);
  return tween;
}

int AnimationLoop::OnFrame(std::function<void(float)> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  int id = next_callback_id_++;
  callbacks_[id] = std::move(callback);
  return id;
}

void AnimationLoop::RemoveCallback(int id) {
  std::lock_guard<std::mutex> lock(mutex_);
  callbacks_.erase(id);
}

void AnimationLoop::Start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return;  // already running
  }
  thread_ = std::thread([this] { Run(); });
}

void AnimationLoop::Stop() {
  running_.store(false);
  if (thread_.joinable()) {
    thread_.join();
  }
}

void AnimationLoop::Run() {
  using Clock = std::chrono::steady_clock;
  auto next_tick = Clock::now();

  while (running_.load()) {
    // Determine frame interval from current target_fps.
    int fps;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      fps = target_fps_;
    }
    auto interval = std::chrono::microseconds(1'000'000 / fps);
    next_tick += interval;

    std::this_thread::sleep_until(next_tick);

    if (!running_.load()) {
      break;
    }

    float dt = std::chrono::duration<float>(interval).count();

    // Snapshot tweens and callbacks while holding the lock.
    std::vector<std::shared_ptr<Tween>> tweens_snapshot;
    std::map<int, std::function<void(float)>> callbacks_snapshot;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tweens_snapshot = tweens_;
      callbacks_snapshot = callbacks_;
    }

    // Advance tweens.
    for (auto& tw : tweens_snapshot) {
      tw->Tick();
    }

    // Remove finished tweens from the main list.
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tweens_.erase(std::remove_if(tweens_.begin(), tweens_.end(),
                                   [](const std::shared_ptr<Tween>& t) {
                                     return t->Done();
                                   }),
                    tweens_.end());
    }

    // Call per-frame callbacks.
    for (auto& [id, fn] : callbacks_snapshot) {
      fn(dt);
    }

    // Post UI refresh.
    if (App* a = App::Active()) {
      a->Post([] {});
    }
  }
}

// ---------------------------------------------------------------------------
// Free-function helpers
// ---------------------------------------------------------------------------

std::shared_ptr<Tween> Animate(float from,
                               float to,
                               float duration,
                               std::function<float(float)> easing) {
  auto tween = std::make_shared<Tween>(from, to, duration, std::move(easing));
  AnimationLoop::Instance().Add(tween);
  tween->Start();
  return tween;
}

int OnFrame(std::function<void(float dt)> callback) {
  return AnimationLoop::Instance().OnFrame(std::move(callback));
}

void RemoveFrameCallback(int id) {
  AnimationLoop::Instance().RemoveCallback(id);
}

}  // namespace ftxui::ui
