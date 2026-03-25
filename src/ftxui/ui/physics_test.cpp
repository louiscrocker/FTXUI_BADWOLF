// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file physics_test.cpp
/// Unit tests for ftxui::ui physics engine: Vec2, PhysicsWorld, PhysicsRenderer
/// and GameLoop.

#include "ftxui/ui/physics.hpp"

#include <cmath>
#include <memory>

#include "gtest/gtest.h"

using namespace ftxui::ui;

// ============================================================================
// Vec2 tests
// ============================================================================

TEST(Vec2, Addition) {
  Vec2 a{1.0f, 2.0f};
  Vec2 b{3.0f, 4.0f};
  Vec2 c = a + b;
  EXPECT_FLOAT_EQ(c.x, 4.0f);
  EXPECT_FLOAT_EQ(c.y, 6.0f);
}

TEST(Vec2, Subtraction) {
  Vec2 a{5.0f, 7.0f};
  Vec2 b{2.0f, 3.0f};
  Vec2 c = a - b;
  EXPECT_FLOAT_EQ(c.x, 3.0f);
  EXPECT_FLOAT_EQ(c.y, 4.0f);
}

TEST(Vec2, ScalarMultiply) {
  Vec2 a{2.0f, 3.0f};
  Vec2 b = a * 2.0f;
  EXPECT_FLOAT_EQ(b.x, 4.0f);
  EXPECT_FLOAT_EQ(b.y, 6.0f);
}

TEST(Vec2, Length) {
  Vec2 a{3.0f, 4.0f};
  EXPECT_FLOAT_EQ(a.length(), 5.0f);
}

TEST(Vec2, LengthZero) {
  Vec2 a{0.0f, 0.0f};
  EXPECT_FLOAT_EQ(a.length(), 0.0f);
}

TEST(Vec2, Normalized) {
  Vec2 a{3.0f, 4.0f};
  Vec2 n = a.normalized();
  EXPECT_NEAR(n.length(), 1.0f, 1e-5f);
  EXPECT_NEAR(n.x, 0.6f, 1e-5f);
  EXPECT_NEAR(n.y, 0.8f, 1e-5f);
}

TEST(Vec2, NormalizedZeroVector) {
  Vec2 a{0.0f, 0.0f};
  Vec2 n = a.normalized();
  EXPECT_FLOAT_EQ(n.x, 0.0f);
  EXPECT_FLOAT_EQ(n.y, 0.0f);
}

TEST(Vec2, DotProduct) {
  Vec2 a{1.0f, 0.0f};
  Vec2 b{0.0f, 1.0f};
  EXPECT_FLOAT_EQ(a.dot(b), 0.0f);

  Vec2 c{1.0f, 2.0f};
  Vec2 d{3.0f, 4.0f};
  EXPECT_FLOAT_EQ(c.dot(d), 11.0f);
}

// ============================================================================
// PhysicsWorld construction
// ============================================================================

TEST(PhysicsWorld, ConstructsWithDefaultGravity) {
  PhysicsWorld world;
  EXPECT_EQ(world.Bodies().size(), 0u);
}

TEST(PhysicsWorld, ConstructsWithCustomGravity) {
  PhysicsWorld world({0, 20.0f});
  EXPECT_EQ(world.Bodies().size(), 0u);
}

// ============================================================================
// Body management
// ============================================================================

TEST(PhysicsWorld, AddCircleReturnsValidId) {
  PhysicsWorld world;
  int id = world.AddCircle({0, 0}, 1.0f);
  EXPECT_GE(id, 0);
  EXPECT_EQ(world.Bodies().size(), 1u);
}

TEST(PhysicsWorld, AddRectReturnsValidId) {
  PhysicsWorld world;
  int id = world.AddRect({0, 0}, 2.0f, 1.0f);
  EXPECT_GE(id, 0);
  EXPECT_EQ(world.Bodies().size(), 1u);
}

TEST(PhysicsWorld, GetBodyReturnsCorrectBody) {
  PhysicsWorld world;
  int id = world.AddCircle({5.0f, 3.0f}, 1.0f);
  auto* body = world.GetBody(id);
  ASSERT_NE(body, nullptr);
  EXPECT_FLOAT_EQ(body->position.x, 5.0f);
  EXPECT_FLOAT_EQ(body->position.y, 3.0f);
  EXPECT_EQ(body->id, id);
}

TEST(PhysicsWorld, GetBodyInvalidIdReturnsNull) {
  PhysicsWorld world;
  EXPECT_EQ(world.GetBody(9999), nullptr);
}

// ============================================================================
// Step and simulation
// ============================================================================

TEST(PhysicsWorld, StepMovesBody) {
  PhysicsWorld world({0, 0});  // no gravity
  int id = world.AddCircle({0, 0}, 0.5f);
  world.SetVelocity(id, {1.0f, 0.0f});
  world.Step(1.0f);
  auto* b = world.GetBody(id);
  ASSERT_NE(b, nullptr);
  EXPECT_GT(b->position.x, 0.0f);
}

TEST(PhysicsWorld, GravityAcceleratesBodyDownward) {
  PhysicsWorld world({0, 10.0f});
  int id = world.AddCircle({0, 0}, 0.5f);
  world.Step(1.0f);
  auto* b = world.GetBody(id);
  ASSERT_NE(b, nullptr);
  EXPECT_GT(b->velocity.y, 0.0f);
}

TEST(PhysicsWorld, BoundCollisionReversesVelocity) {
  PhysicsWorld world({0, 0});
  world.SetBounds(10.0f, 10.0f);
  int id = world.AddCircle({9.6f, 5.0f}, 0.5f);
  world.SetVelocity(id, {10.0f, 0.0f});
  // Step enough to hit right wall
  world.Step(0.1f);
  auto* b = world.GetBody(id);
  ASSERT_NE(b, nullptr);
  // Should be bounced back
  EXPECT_LE(b->position.x, 10.0f);
}

TEST(PhysicsWorld, SetGravity) {
  PhysicsWorld world({0, 0});
  world.SetGravity({0, 5.0f});
  int id = world.AddCircle({5, 5}, 0.5f);
  world.Step(1.0f);
  auto* b = world.GetBody(id);
  ASSERT_NE(b, nullptr);
  EXPECT_GT(b->velocity.y, 0.0f);
}

TEST(PhysicsWorld, ApplyImpulseChangesVelocity) {
  PhysicsWorld world({0, 0});
  int id = world.AddCircle({0, 0}, 0.5f, 1.0f);
  world.ApplyImpulse(id, {10.0f, 0.0f});
  auto* b = world.GetBody(id);
  ASSERT_NE(b, nullptr);
  EXPECT_GT(b->velocity.x, 0.0f);
}

TEST(PhysicsWorld, RemoveBodyRemovesIt) {
  PhysicsWorld world;
  int id = world.AddCircle({0, 0}, 1.0f);
  EXPECT_EQ(world.Bodies().size(), 1u);
  world.RemoveBody(id);
  EXPECT_EQ(world.Bodies().size(), 0u);
  EXPECT_EQ(world.GetBody(id), nullptr);
}

TEST(PhysicsWorld, StaticBodyDoesNotMove) {
  PhysicsWorld world({0, 9.8f});
  int id = world.AddStaticRect({5, 5}, 4.0f, 2.0f);
  auto* b = world.GetBody(id);
  ASSERT_NE(b, nullptr);
  float x0 = b->position.x;
  float y0 = b->position.y;
  world.Step(1.0f);
  EXPECT_FLOAT_EQ(b->position.x, x0);
  EXPECT_FLOAT_EQ(b->position.y, y0);
}

TEST(PhysicsWorld, ClearRemovesAllBodies) {
  PhysicsWorld world;
  world.AddCircle({0, 0}, 1.0f);
  world.AddCircle({5, 5}, 1.0f);
  world.AddRect({2, 2}, 1.0f, 1.0f);
  EXPECT_EQ(world.Bodies().size(), 3u);
  world.Clear();
  EXPECT_EQ(world.Bodies().size(), 0u);
}

// ============================================================================
// PhysicsRenderer
// ============================================================================

TEST(PhysicsRenderer, ConstructsWithoutCrash) {
  auto world = std::make_shared<PhysicsWorld>();
  EXPECT_NO_THROW({ PhysicsRenderer renderer(world, 40.0f, 20.0f); });
}

TEST(PhysicsRenderer, RenderReturnsNonNullElement) {
  auto world = std::make_shared<PhysicsWorld>();
  world->AddCircle({5, 5}, 1.0f);
  PhysicsRenderer renderer(world, 40.0f, 20.0f);
  auto elem = renderer.Render();
  EXPECT_NE(elem, nullptr);
}

// ============================================================================
// GameLoop
// ============================================================================

TEST(GameLoop, ConstructsWithoutCrash) {
  EXPECT_NO_THROW({ GameLoop loop(60); });
}

TEST(GameLoop, BuildReturnsNonNullComponent) {
  GameLoop loop(30);
  loop.OnRender([]() -> ftxui::Element { return ftxui::text("test"); });
  auto comp = loop.Build();
  EXPECT_NE(comp, nullptr);
  loop.Stop();
}
