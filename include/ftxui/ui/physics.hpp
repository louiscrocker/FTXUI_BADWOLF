// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_PHYSICS_HPP
#define FTXUI_UI_PHYSICS_HPP

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

// ── Vec2
// ──────────────────────────────────────────────────────────────────────
struct Vec2 {
  float x = 0, y = 0;
  Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  Vec2& operator+=(Vec2 o) {
    x += o.x;
    y += o.y;
    return *this;
  }
  float length() const { return std::sqrt(x * x + y * y); }
  Vec2 normalized() const {
    float l = length();
    if (l < 1e-6f) {
      return {0, 0};
    }
    return {x / l, y / l};
  }
  float dot(Vec2 o) const { return x * o.x + y * o.y; }
  static Vec2 Zero() { return {0, 0}; }
};

// ── Shape types
// ───────────────────────────────────────────────────────────────
enum class ShapeType { Circle, Rect, Line };

struct Shape {
  ShapeType type = ShapeType::Circle;
  float radius = 0;             // Circle
  float width = 0, height = 0;  // Rect
  Vec2 p1, p2;                  // Line
};

// ── RigidBody
// ─────────────────────────────────────────────────────────────────
struct RigidBody {
  int id = 0;
  Vec2 position;
  Vec2 velocity;
  Vec2 force_accum;
  float mass = 1.0f;         // 0 = static (immovable)
  float restitution = 0.7f;  // bounciness [0,1]
  float friction = 0.1f;
  float rotation = 0;  // radians
  float angular_velocity = 0;
  Shape shape;
  ftxui::Color color = ftxui::Color::White;
  std::string tag;  // for identification
  bool active = true;

  bool is_static() const { return mass == 0.0f; }
};

// ── PhysicsWorld
// ──────────────────────────────────────────────────────────────
class PhysicsWorld {
 public:
  explicit PhysicsWorld(Vec2 gravity = {0, 9.8f});

  // Body management
  int AddCircle(Vec2 pos,
                float radius,
                float mass = 1.0f,
                ftxui::Color color = ftxui::Color::White);
  int AddRect(Vec2 pos,
              float w,
              float h,
              float mass = 1.0f,
              ftxui::Color color = ftxui::Color::White);
  int AddStaticLine(Vec2 p1, Vec2 p2);
  int AddStaticRect(Vec2 pos, float w, float h);

  void RemoveBody(int id);
  RigidBody* GetBody(int id);
  const std::vector<RigidBody>& Bodies() const;

  // Physics
  void Step(float dt_seconds);
  void SetGravity(Vec2 g);
  void ApplyForce(int id, Vec2 force);
  void ApplyImpulse(int id, Vec2 impulse);
  void SetVelocity(int id, Vec2 vel);
  void SetPosition(int id, Vec2 pos);

  // Bounds (bodies bounce off edges)
  void SetBounds(float width, float height);

  // Collision callbacks
  void OnCollision(std::function<void(int id_a, int id_b)> cb);

  // Clear all bodies
  void Clear();

 private:
  Vec2 gravity_;
  std::vector<RigidBody> bodies_;
  int next_id_{0};
  float bound_w_{0}, bound_h_{0};
  std::vector<std::function<void(int, int)>> collision_cbs_;

  void ResolveCollisions();
  bool CircleCircle(RigidBody& a, RigidBody& b);
  bool CircleRect(RigidBody& circle, RigidBody& rect);
  bool RectRect(RigidBody& a, RigidBody& b);
  void NotifyCollision(int id_a, int id_b);
};

// ── GameLoop
// ──────────────────────────────────────────────────────────────────
/// @brief Fixed-timestep game loop at configurable FPS, integrates with
/// AnimationLoop.
///
/// @ingroup ui
class GameLoop {
 public:
  struct State {
    float dt;        // delta time in seconds
    uint64_t frame;  // frame counter
    double elapsed;  // total elapsed seconds
  };

  explicit GameLoop(int target_fps = 60);
  ~GameLoop();

  void OnUpdate(std::function<void(const State&)> fn);
  void OnRender(std::function<ftxui::Element()> fn);
  void OnInput(std::function<bool(ftxui::Event)> fn);

  void Start();
  void Stop();
  bool IsRunning() const;

  // Build a Component that runs the game loop
  ftxui::Component Build();

 private:
  int target_fps_;
  bool running_{false};
  int frame_callback_id_{-1};
  uint64_t frame_{0};
  double elapsed_{0.0};

  std::function<void(const State&)> update_fn_;
  std::function<ftxui::Element()> render_fn_;
  std::function<bool(ftxui::Event)> input_fn_;
};

// ── PhysicsRenderer
// ───────────────────────────────────────────────────────────
/// @brief Renders a PhysicsWorld onto a BrailleCanvas.
///
/// @ingroup ui
class PhysicsRenderer {
 public:
  PhysicsRenderer(std::shared_ptr<PhysicsWorld> world,
                  float world_width,
                  float world_height);

  // Returns a flex Element that fills its container
  ftxui::Element Render() const;

  // Map world coords → canvas coords
  Vec2 WorldToCanvas(Vec2 world, int canvas_w, int canvas_h) const;

 private:
  std::shared_ptr<PhysicsWorld> world_;
  float world_w_;
  float world_h_;
};

// ── Built-in game presets
// ─────────────────────────────────────────────────────
namespace Games {

ftxui::Component Pong();       // Two-player pong
ftxui::Component Breakout();   // Breakout/Arkanoid
ftxui::Component Gravity();    // Gravity sandbox (add bodies with mouse)
ftxui::Component Billiards();  // Pool/billiards demo

}  // namespace Games

}  // namespace ftxui::ui

#endif  // FTXUI_UI_PHYSICS_HPP
