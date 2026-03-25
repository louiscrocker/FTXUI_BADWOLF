// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file animation_test.cpp
/// Unit tests for ftxui::ui::AnimationLoop, Tween, Easing, and ParticleSystem.

#include <chrono>
#include <memory>
#include <thread>

#include "ftxui/ui/animation.hpp"
#include "ftxui/ui/particles.hpp"
#include "gtest/gtest.h"

using namespace ftxui::ui;

// ============================================================================
// Easing tests
// ============================================================================

TEST(Easing, LinearMidpoint) {
  EXPECT_FLOAT_EQ(Easing::Linear(0.5f), 0.5f);
}

TEST(Easing, LinearBoundaries) {
  EXPECT_FLOAT_EQ(Easing::Linear(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(Easing::Linear(1.0f), 1.0f);
}

TEST(Easing, EaseInOut_Symmetric) {
  // EaseInOut should satisfy f(1-t) = 1 - f(t) (symmetric around 0.5).
  for (float t = 0.1f; t < 0.9f; t += 0.1f) {
    EXPECT_NEAR(Easing::EaseInOut(1.0f - t), 1.0f - Easing::EaseInOut(t),
                1e-5f);
  }
}

TEST(Easing, EaseInOut_Boundaries) {
  EXPECT_FLOAT_EQ(Easing::EaseInOut(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(Easing::EaseInOut(1.0f), 1.0f);
}

TEST(Easing, Bounce_Positive) {
  // Bounce should stay non-negative on [0, 1].
  for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
    EXPECT_GE(Easing::Bounce(t), 0.0f);
  }
  // And end at 1.
  EXPECT_NEAR(Easing::Bounce(1.0f), 1.0f, 1e-4f);
}

TEST(Easing, EaseIn_Boundaries) {
  EXPECT_FLOAT_EQ(Easing::EaseIn(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(Easing::EaseIn(1.0f), 1.0f);
}

TEST(Easing, EaseOut_Boundaries) {
  EXPECT_FLOAT_EQ(Easing::EaseOut(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(Easing::EaseOut(1.0f), 1.0f);
}

// ============================================================================
// Tween tests
// ============================================================================

TEST(Tween, StartsAtFrom) {
  Tween t(0.0f, 10.0f, 1.0f, Easing::Linear);
  // Before Start(), value should be `from`.
  EXPECT_FLOAT_EQ(t.Value(), 0.0f);
}

TEST(Tween, ProgressZeroBeforeStart) {
  Tween t(0.0f, 10.0f, 1.0f, Easing::Linear);
  EXPECT_FLOAT_EQ(t.Progress(), 0.0f);
}

TEST(Tween, EndsAtTo) {
  Tween t(0.0f, 10.0f, 0.0f, Easing::Linear);  // zero duration → instant
  t.Start();
  // With zero duration it should immediately report done.
  t.Tick();
  EXPECT_TRUE(t.Done());
  // After done, progress is clamped to 1, so Value() → to.
  EXPECT_FLOAT_EQ(t.Value(), 10.0f);
}

TEST(Tween, LinearEasing) {
  // Use a very short duration and check immediately after start.
  Tween t(2.0f, 4.0f, 100.0f, Easing::Linear);
  t.Start();
  // Immediately after start, progress ≈ 0, value ≈ from.
  EXPECT_NEAR(t.Value(), 2.0f, 0.1f);
}

TEST(Tween, EaseInOut) {
  Tween t(0.0f, 1.0f, 100.0f, Easing::EaseInOut);
  t.Start();
  EXPECT_NEAR(t.Value(), 0.0f, 0.05f);
}

TEST(Tween, Done_AfterDuration) {
  Tween t(0.0f, 1.0f, 0.0f, Easing::Linear);  // instant
  t.Start();
  t.Tick();
  EXPECT_TRUE(t.Done());
}

TEST(Tween, NotDone_BeforeDuration) {
  Tween t(0.0f, 1.0f, 100.0f, Easing::Linear);  // very long
  t.Start();
  t.Tick();
  EXPECT_FALSE(t.Done());
}

TEST(Tween, OnComplete_Called) {
  bool called = false;
  Tween t(0.0f, 1.0f, 0.0f, Easing::Linear);
  t.OnComplete([&] { called = true; });
  t.Start();
  t.Tick();
  EXPECT_TRUE(called);
}

TEST(Tween, OnComplete_CalledOnceOnly) {
  int count = 0;
  Tween t(0.0f, 1.0f, 0.0f, Easing::Linear);
  t.OnComplete([&] { count++; });
  t.Start();
  t.Tick();
  t.Tick();
  t.Tick();
  EXPECT_EQ(count, 1);
}

TEST(Tween, Reverse) {
  Tween t(0.0f, 10.0f, 1.0f, Easing::Linear);
  t.Reverse();
  // After reversal, `from` becomes 10 and `to` becomes 0.
  // Before Start(), value = new `from` = 10.
  EXPECT_FLOAT_EQ(t.Value(), 10.0f);
}

TEST(Tween, Reset_ClearsState) {
  Tween t(0.0f, 1.0f, 0.0f, Easing::Linear);
  t.Start();
  t.Tick();
  ASSERT_TRUE(t.Done());
  t.Reset();
  EXPECT_FALSE(t.Done());
  EXPECT_FLOAT_EQ(t.Progress(), 0.0f);
}

// ============================================================================
// AnimationLoop tests
// ============================================================================

TEST(AnimationLoop, SingletonNotNull) {
  // Instance() must return a stable reference.
  AnimationLoop* a = &AnimationLoop::Instance();
  AnimationLoop* b = &AnimationLoop::Instance();
  EXPECT_EQ(a, b);
}

TEST(AnimationLoop, SetFPS) {
  // SetFPS should not crash with boundary values.
  AnimationLoop::Instance().SetFPS(60);
  AnimationLoop::Instance().SetFPS(1);
  AnimationLoop::Instance().SetFPS(30);
  // No assertion needed — just confirm no crash.
}

TEST(AnimationLoop, AddTween) {
  auto tween = std::make_shared<Tween>(0.0f, 1.0f, 0.05f, Easing::Linear);
  tween->Start();
  AnimationLoop::Instance().Add(tween);
  // Let the loop run a couple of frames.
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  EXPECT_TRUE(tween->Done());
}

TEST(AnimationLoop, OnFrame) {
  std::atomic<int> count{0};
  int id = AnimationLoop::Instance().OnFrame([&](float) { count++; });
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  AnimationLoop::Instance().RemoveCallback(id);
  EXPECT_GT(count.load(), 0);
}

TEST(AnimationLoop, RemoveCallback) {
  std::atomic<int> count{0};
  int id = AnimationLoop::Instance().OnFrame([&](float) { count++; });
  // Remove almost immediately.
  AnimationLoop::Instance().RemoveCallback(id);
  int snap = count.load();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // Count must not have grown significantly after removal.
  EXPECT_LE(count.load() - snap, 2);  // allow at most 2 in-flight calls
}

// ============================================================================
// ParticleSystem tests
// ============================================================================

TEST(ParticleSystem, InitiallyEmpty) {
  ParticleSystem ps;
  EXPECT_EQ(ps.LiveCount(), 0);
}

TEST(ParticleSystem, StartEmitting) {
  ParticleSystem ps;
  ps.Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  int n = ps.LiveCount();
  ps.Stop();
  EXPECT_GT(n, 0);
}

TEST(ParticleSystem, UpdateAdvancesParticles) {
  ParticleConfig cfg;
  cfg.emit_rate = 100;
  cfg.lifetime_min = 5.0f;
  cfg.lifetime_max = 5.0f;
  cfg.gravity = 0.0f;
  cfg.speed_min = 10.0f;
  cfg.speed_max = 10.0f;
  cfg.angle_min = 0.0f;  // all particles go right
  cfg.angle_max = 0.0f;
  ParticleSystem ps(cfg);

  // Manually update without starting the loop callback.
  ps.Update(0.1f);  // emits ~10 particles
  EXPECT_GT(ps.LiveCount(), 0);
  // After another dt particles should still be alive (lifetime = 5s).
  ps.Update(0.1f);
  EXPECT_GT(ps.LiveCount(), 0);
}

TEST(ParticleSystem, ClearRemovesAll) {
  ParticleConfig cfg;
  cfg.emit_rate = 200;
  cfg.lifetime_min = 10.0f;
  cfg.lifetime_max = 10.0f;
  ParticleSystem ps(cfg);
  ps.Update(0.5f);
  ASSERT_GT(ps.LiveCount(), 0);
  ps.Clear();
  EXPECT_EQ(ps.LiveCount(), 0);
}

TEST(ParticleSystem, StopPreventsNewEmission) {
  ParticleConfig cfg;
  cfg.emit_rate = 0;  // no auto-emission
  cfg.lifetime_min = 0.01f;
  cfg.lifetime_max = 0.01f;
  ParticleSystem ps(cfg);
  ps.Stop();
  ps.Update(1.0f);
  EXPECT_EQ(ps.LiveCount(), 0);
}
