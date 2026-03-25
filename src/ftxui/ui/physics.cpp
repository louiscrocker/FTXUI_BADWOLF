// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/physics.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/animation.hpp"

namespace ftxui::ui {

// ── PhysicsWorld
// ──────────────────────────────────────────────────────────────

PhysicsWorld::PhysicsWorld(Vec2 gravity) : gravity_(gravity) {}

int PhysicsWorld::AddCircle(Vec2 pos,
                            float radius,
                            float mass,
                            ftxui::Color color) {
  RigidBody b;
  b.id = next_id_++;
  b.position = pos;
  b.mass = mass;
  b.color = color;
  b.shape.type = ShapeType::Circle;
  b.shape.radius = radius;
  bodies_.push_back(b);
  return b.id;
}

int PhysicsWorld::AddRect(Vec2 pos,
                          float w,
                          float h,
                          float mass,
                          ftxui::Color color) {
  RigidBody b;
  b.id = next_id_++;
  b.position = pos;
  b.mass = mass;
  b.color = color;
  b.shape.type = ShapeType::Rect;
  b.shape.width = w;
  b.shape.height = h;
  bodies_.push_back(b);
  return b.id;
}

int PhysicsWorld::AddStaticLine(Vec2 p1, Vec2 p2) {
  RigidBody b;
  b.id = next_id_++;
  b.mass = 0.0f;
  b.shape.type = ShapeType::Line;
  b.shape.p1 = p1;
  b.shape.p2 = p2;
  bodies_.push_back(b);
  return b.id;
}

int PhysicsWorld::AddStaticRect(Vec2 pos, float w, float h) {
  return AddRect(pos, w, h, 0.0f, ftxui::Color::White);
}

void PhysicsWorld::RemoveBody(int id) {
  bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(),
                               [id](const RigidBody& b) { return b.id == id; }),
                bodies_.end());
}

RigidBody* PhysicsWorld::GetBody(int id) {
  for (auto& b : bodies_) {
    if (b.id == id) {
      return &b;
    }
  }
  return nullptr;
}

const std::vector<RigidBody>& PhysicsWorld::Bodies() const {
  return bodies_;
}

void PhysicsWorld::SetGravity(Vec2 g) {
  gravity_ = g;
}

void PhysicsWorld::ApplyForce(int id, Vec2 force) {
  RigidBody* b = GetBody(id);
  if (b && !b->is_static()) {
    b->force_accum += force;
  }
}

void PhysicsWorld::ApplyImpulse(int id, Vec2 impulse) {
  RigidBody* b = GetBody(id);
  if (b && !b->is_static()) {
    b->velocity += impulse * (1.0f / b->mass);
  }
}

void PhysicsWorld::SetVelocity(int id, Vec2 vel) {
  RigidBody* b = GetBody(id);
  if (b) {
    b->velocity = vel;
  }
}

void PhysicsWorld::SetPosition(int id, Vec2 pos) {
  RigidBody* b = GetBody(id);
  if (b) {
    b->position = pos;
  }
}

void PhysicsWorld::SetBounds(float width, float height) {
  bound_w_ = width;
  bound_h_ = height;
}

void PhysicsWorld::OnCollision(std::function<void(int, int)> cb) {
  collision_cbs_.push_back(std::move(cb));
}

void PhysicsWorld::Clear() {
  bodies_.clear();
}

void PhysicsWorld::NotifyCollision(int id_a, int id_b) {
  for (auto& cb : collision_cbs_) {
    cb(id_a, id_b);
  }
}

void PhysicsWorld::Step(float dt) {
  for (auto& b : bodies_) {
    if (!b.active || b.is_static()) {
      continue;
    }

    // Apply accumulated forces + gravity
    Vec2 accel = gravity_ + b.force_accum * (1.0f / b.mass);
    b.velocity += accel * dt;

    // Integrate position
    b.position += b.velocity * dt;

    // Damping
    b.velocity = b.velocity * (1.0f - b.friction * dt);

    // Reset per-frame forces
    b.force_accum = Vec2::Zero();

    // Bound collisions
    if (bound_w_ > 0 && bound_h_ > 0) {
      float r = (b.shape.type == ShapeType::Circle) ? b.shape.radius : 0;
      float hw = (b.shape.type == ShapeType::Rect) ? b.shape.width * 0.5f : r;
      float hh = (b.shape.type == ShapeType::Rect) ? b.shape.height * 0.5f : r;

      if (b.position.x - hw < 0) {
        b.position.x = hw;
        b.velocity.x = std::abs(b.velocity.x) * b.restitution;
      }
      if (b.position.x + hw > bound_w_) {
        b.position.x = bound_w_ - hw;
        b.velocity.x = -std::abs(b.velocity.x) * b.restitution;
      }
      if (b.position.y - hh < 0) {
        b.position.y = hh;
        b.velocity.y = std::abs(b.velocity.y) * b.restitution;
      }
      if (b.position.y + hh > bound_h_) {
        b.position.y = bound_h_ - hh;
        b.velocity.y = -std::abs(b.velocity.y) * b.restitution;
      }
    }
  }

  ResolveCollisions();
}

// ── Collision detection
// ────────────────────────────────────────────────────────

bool PhysicsWorld::CircleCircle(RigidBody& a, RigidBody& b) {
  Vec2 diff = b.position - a.position;
  float dist = diff.length();
  float min_dist = a.shape.radius + b.shape.radius;
  if (dist >= min_dist || dist < 1e-6f) {
    return false;
  }

  Vec2 normal = diff.normalized();
  float overlap = min_dist - dist;

  // Positional correction
  if (!a.is_static()) {
    a.position = a.position - normal * (overlap * 0.5f);
  }
  if (!b.is_static()) {
    b.position = b.position + normal * (overlap * 0.5f);
  }

  // Velocity reflection
  Vec2 rel_vel = b.velocity - a.velocity;
  float vel_along = rel_vel.dot(normal);
  if (vel_along > 0) {
    return true;  // separating
  }

  float e = std::min(a.restitution, b.restitution);
  float j = -(1.0f + e) * vel_along;
  float inv_mass_sum =
      (a.is_static() ? 0 : 1.0f / a.mass) + (b.is_static() ? 0 : 1.0f / b.mass);
  if (inv_mass_sum < 1e-6f) {
    return true;
  }
  j /= inv_mass_sum;

  Vec2 impulse = normal * j;
  if (!a.is_static()) {
    a.velocity = a.velocity - impulse * (1.0f / a.mass);
  }
  if (!b.is_static()) {
    b.velocity = b.velocity + impulse * (1.0f / b.mass);
  }

  NotifyCollision(a.id, b.id);
  return true;
}

bool PhysicsWorld::CircleRect(RigidBody& circle, RigidBody& rect) {
  // Clamp circle center to rect AABB
  float rx = rect.position.x - rect.shape.width * 0.5f;
  float ry = rect.position.y - rect.shape.height * 0.5f;
  float rw = rect.shape.width;
  float rh = rect.shape.height;

  float cx = std::max(rx, std::min(circle.position.x, rx + rw));
  float cy = std::max(ry, std::min(circle.position.y, ry + rh));

  float dx = circle.position.x - cx;
  float dy = circle.position.y - cy;
  float dist2 = dx * dx + dy * dy;

  if (dist2 >= circle.shape.radius * circle.shape.radius) {
    return false;
  }

  float dist = std::sqrt(dist2);
  Vec2 normal;
  if (dist < 1e-6f) {
    normal = {0, -1};
  } else {
    normal = {dx / dist, dy / dist};
  }

  float overlap = circle.shape.radius - dist;
  if (!circle.is_static()) {
    circle.position += normal * overlap;
  }
  if (!rect.is_static()) {
    rect.position = rect.position - normal * overlap;
  }

  Vec2 rel_vel = circle.velocity - rect.velocity;
  float vel_along = rel_vel.dot(normal);
  if (vel_along < 0) {
    return true;  // separating
  }

  float e = std::min(circle.restitution, rect.restitution);
  float j = (1.0f + e) * vel_along;
  float inv_mass_sum = (circle.is_static() ? 0 : 1.0f / circle.mass) +
                       (rect.is_static() ? 0 : 1.0f / rect.mass);
  if (inv_mass_sum < 1e-6f) {
    return true;
  }
  j /= inv_mass_sum;

  Vec2 impulse = normal * j;
  if (!circle.is_static()) {
    circle.velocity = circle.velocity - impulse * (1.0f / circle.mass);
  }
  if (!rect.is_static()) {
    rect.velocity = rect.velocity + impulse * (1.0f / rect.mass);
  }

  NotifyCollision(circle.id, rect.id);
  return true;
}

bool PhysicsWorld::RectRect(RigidBody& a, RigidBody& b) {
  float ax1 = a.position.x - a.shape.width * 0.5f;
  float ax2 = a.position.x + a.shape.width * 0.5f;
  float ay1 = a.position.y - a.shape.height * 0.5f;
  float ay2 = a.position.y + a.shape.height * 0.5f;

  float bx1 = b.position.x - b.shape.width * 0.5f;
  float bx2 = b.position.x + b.shape.width * 0.5f;
  float by1 = b.position.y - b.shape.height * 0.5f;
  float by2 = b.position.y + b.shape.height * 0.5f;

  if (ax2 <= bx1 || bx2 <= ax1 || ay2 <= by1 || by2 <= ay1) {
    return false;
  }

  // Find smallest overlap axis
  float overlap_x = std::min(ax2 - bx1, bx2 - ax1);
  float overlap_y = std::min(ay2 - by1, by2 - ay1);

  Vec2 normal;
  float overlap;
  if (overlap_x < overlap_y) {
    overlap = overlap_x;
    normal = {(a.position.x < b.position.x) ? 1.0f : -1.0f, 0};
  } else {
    overlap = overlap_y;
    normal = {0, (a.position.y < b.position.y) ? 1.0f : -1.0f};
  }

  if (!a.is_static()) {
    a.position = a.position - normal * (overlap * 0.5f);
  }
  if (!b.is_static()) {
    b.position = b.position + normal * (overlap * 0.5f);
  }

  Vec2 rel_vel = b.velocity - a.velocity;
  float vel_along = rel_vel.dot(normal);
  if (vel_along > 0) {
    return true;
  }

  float e = std::min(a.restitution, b.restitution);
  float j = -(1.0f + e) * vel_along;
  float inv_mass_sum =
      (a.is_static() ? 0 : 1.0f / a.mass) + (b.is_static() ? 0 : 1.0f / b.mass);
  if (inv_mass_sum < 1e-6f) {
    return true;
  }
  j /= inv_mass_sum;

  Vec2 impulse = normal * j;
  if (!a.is_static()) {
    a.velocity = a.velocity - impulse * (1.0f / a.mass);
  }
  if (!b.is_static()) {
    b.velocity = b.velocity + impulse * (1.0f / b.mass);
  }

  NotifyCollision(a.id, b.id);
  return true;
}

void PhysicsWorld::ResolveCollisions() {
  for (size_t i = 0; i < bodies_.size(); ++i) {
    auto& a = bodies_[i];
    if (!a.active || a.shape.type == ShapeType::Line) {
      continue;
    }

    for (size_t j = i + 1; j < bodies_.size(); ++j) {
      auto& b = bodies_[j];
      if (!b.active || b.shape.type == ShapeType::Line) {
        continue;
      }
      if (a.is_static() && b.is_static()) {
        continue;
      }

      if (a.shape.type == ShapeType::Circle &&
          b.shape.type == ShapeType::Circle) {
        CircleCircle(a, b);
      } else if (a.shape.type == ShapeType::Circle &&
                 b.shape.type == ShapeType::Rect) {
        CircleRect(a, b);
      } else if (a.shape.type == ShapeType::Rect &&
                 b.shape.type == ShapeType::Circle) {
        CircleRect(b, a);
      } else if (a.shape.type == ShapeType::Rect &&
                 b.shape.type == ShapeType::Rect) {
        RectRect(a, b);
      }
    }
  }
}

// ── GameLoop
// ──────────────────────────────────────────────────────────────────

GameLoop::GameLoop(int target_fps) : target_fps_(target_fps) {}

GameLoop::~GameLoop() {
  Stop();
}

void GameLoop::OnUpdate(std::function<void(const State&)> fn) {
  update_fn_ = std::move(fn);
}

void GameLoop::OnRender(std::function<ftxui::Element()> fn) {
  render_fn_ = std::move(fn);
}

void GameLoop::OnInput(std::function<bool(ftxui::Event)> fn) {
  input_fn_ = std::move(fn);
}

void GameLoop::Start() {
  if (running_) {
    return;
  }
  running_ = true;
  AnimationLoop::Instance().SetFPS(target_fps_);
  frame_callback_id_ = AnimationLoop::Instance().OnFrame([this](float dt) {
    if (!running_) {
      return;
    }
    elapsed_ += dt;
    frame_++;
    if (update_fn_) {
      State s;
      s.dt = dt;
      s.frame = frame_;
      s.elapsed = elapsed_;
      update_fn_(s);
    }
  });
}

void GameLoop::Stop() {
  if (!running_) {
    return;
  }
  running_ = false;
  if (frame_callback_id_ != -1) {
    AnimationLoop::Instance().RemoveCallback(frame_callback_id_);
    frame_callback_id_ = -1;
  }
}

bool GameLoop::IsRunning() const {
  return running_;
}

ftxui::Component GameLoop::Build() {
  Start();

  struct GameComponent : public ftxui::ComponentBase {
    GameLoop* loop;
    GameComponent(GameLoop* l) : loop(l) {}

    ftxui::Element OnRender() override {
      if (loop->render_fn_) {
        return loop->render_fn_();
      }
      return ftxui::text("");
    }

    bool OnEvent(ftxui::Event event) override {
      if (loop->input_fn_) {
        return loop->input_fn_(event);
      }
      return false;
    }
  };

  return std::make_shared<GameComponent>(this);
}

// ── PhysicsRenderer
// ───────────────────────────────────────────────────────────

PhysicsRenderer::PhysicsRenderer(std::shared_ptr<PhysicsWorld> world,
                                 float world_width,
                                 float world_height)
    : world_(std::move(world)), world_w_(world_width), world_h_(world_height) {}

Vec2 PhysicsRenderer::WorldToCanvas(Vec2 world,
                                    int canvas_w,
                                    int canvas_h) const {
  float cx = (world.x / world_w_) * static_cast<float>(canvas_w);
  float cy = (world.y / world_h_) * static_cast<float>(canvas_h);
  return {cx, cy};
}

ftxui::Element PhysicsRenderer::Render() const {
  auto world_ptr = world_;
  float ww = world_w_;
  float wh = world_h_;

  return ftxui::canvas([world_ptr, ww, wh](ftxui::Canvas& c) {
           int cw = c.width();
           int ch = c.height();

           auto toCanvas = [&](Vec2 world) -> Vec2 {
             float cx = (world.x / ww) * static_cast<float>(cw);
             float cy = (world.y / wh) * static_cast<float>(ch);
             return {cx, cy};
           };

           for (const auto& body : world_ptr->Bodies()) {
             if (!body.active) {
               continue;
             }

             ftxui::Color col = body.color;

             if (body.shape.type == ShapeType::Circle) {
               Vec2 cp = toCanvas(body.position);
               float r = (body.shape.radius / ww) * static_cast<float>(cw);
               int ix = static_cast<int>(cp.x);
               int iy = static_cast<int>(cp.y);
               int ir = static_cast<int>(r);
               if (ir < 1) {
                 ir = 1;
               }

               // Draw circle as braille dots
               for (int dy = -ir; dy <= ir; ++dy) {
                 for (int dx = -ir; dx <= ir; ++dx) {
                   if (dx * dx + dy * dy <= ir * ir) {
                     int px = ix + dx;
                     int py = iy + dy;
                     if (px >= 0 && px < cw && py >= 0 && py < ch) {
                       c.DrawPoint(px, py, true, col);
                     }
                   }
                 }
               }
             } else if (body.shape.type == ShapeType::Rect) {
               Vec2 cp = toCanvas(body.position);
               float rw = (body.shape.width / ww) * static_cast<float>(cw);
               float rh = (body.shape.height / wh) * static_cast<float>(ch);
               int x1 = static_cast<int>(cp.x - rw * 0.5f);
               int y1 = static_cast<int>(cp.y - rh * 0.5f);
               int x2 = static_cast<int>(cp.x + rw * 0.5f);
               int y2 = static_cast<int>(cp.y + rh * 0.5f);

               // Draw rect outline
               for (int px = x1; px <= x2; ++px) {
                 if (px >= 0 && px < cw) {
                   if (y1 >= 0 && y1 < ch) {
                     c.DrawPoint(px, y1, true, col);
                   }
                   if (y2 >= 0 && y2 < ch) {
                     c.DrawPoint(px, y2, true, col);
                   }
                 }
               }
               for (int py = y1; py <= y2; ++py) {
                 if (py >= 0 && py < ch) {
                   if (x1 >= 0 && x1 < cw) {
                     c.DrawPoint(x1, py, true, col);
                   }
                   if (x2 >= 0 && x2 < cw) {
                     c.DrawPoint(x2, py, true, col);
                   }
                 }
               }
               // Fill static rects
               if (body.is_static()) {
                 for (int px = x1 + 1; px < x2; ++px) {
                   for (int py = y1 + 1; py < y2; ++py) {
                     if (px >= 0 && px < cw && py >= 0 && py < ch) {
                       c.DrawPoint(px, py, true, col);
                     }
                   }
                 }
               }
             } else if (body.shape.type == ShapeType::Line) {
               Vec2 cp1 = toCanvas(body.shape.p1);
               Vec2 cp2 = toCanvas(body.shape.p2);
               c.DrawPointLine(static_cast<int>(cp1.x), static_cast<int>(cp1.y),
                               static_cast<int>(cp2.x), static_cast<int>(cp2.y),
                               col);
             }
           }
         }) |
         ftxui::flex;
}

// ── Games
// ─────────────────────────────────────────────────────────────────────

namespace Games {

// ── Pong
// ──────────────────────────────────────────────────────────────────────

ftxui::Component Pong() {
  struct PongState {
    std::shared_ptr<PhysicsWorld> world =
        std::make_shared<PhysicsWorld>(Vec2{0, 0});
    PhysicsRenderer renderer{world, 40.0f, 20.0f};
    int ball_id;
    int paddle_l_id;
    int paddle_r_id;
    int score_l = 0;
    int score_r = 0;
    bool up_l = false, down_l = false;
    bool up_r = false, down_r = false;
    int frame_cb = -1;

    void Reset() {
      world->Clear();
      world->SetBounds(40.0f, 20.0f);
      ball_id = world->AddCircle({20, 10}, 0.6f, 1.0f, ftxui::Color::Yellow);
      paddle_l_id =
          world->AddRect({2, 10}, 1.0f, 4.0f, 0.0f, ftxui::Color::Cyan);
      paddle_r_id =
          world->AddRect({38, 10}, 1.0f, 4.0f, 0.0f, ftxui::Color::Green);
      world->SetVelocity(ball_id, {15.0f, 8.0f});
      // Walls top/bottom
      world->AddStaticLine({0, 0}, {40, 0});
      world->AddStaticLine({0, 20}, {40, 20});
    }

    void Update(float dt) {
      // Move paddles
      const float paddle_speed = 15.0f;
      auto* pl = world->GetBody(paddle_l_id);
      auto* pr = world->GetBody(paddle_r_id);
      if (pl) {
        float vy = 0;
        if (up_l) {
          vy = -paddle_speed;
        }
        if (down_l) {
          vy = paddle_speed;
        }
        pl->velocity = {0, vy};
        pl->position.y += vy * dt;
        pl->position.y = std::max(2.0f, std::min(18.0f, pl->position.y));
      }
      if (pr) {
        float vy = 0;
        if (up_r) {
          vy = -paddle_speed;
        }
        if (down_r) {
          vy = paddle_speed;
        }
        pr->velocity = {0, vy};
        pr->position.y += vy * dt;
        pr->position.y = std::max(2.0f, std::min(18.0f, pr->position.y));
      }

      world->Step(dt);

      // Check scoring
      auto* ball = world->GetBody(ball_id);
      if (ball) {
        if (ball->position.x < 0) {
          score_r++;
          world->SetPosition(ball_id, {20, 10});
          world->SetVelocity(ball_id, {15.0f, 8.0f});
        } else if (ball->position.x > 40) {
          score_l++;
          world->SetPosition(ball_id, {20, 10});
          world->SetVelocity(ball_id, {-15.0f, 8.0f});
        }
      }
    }
  };

  auto state = std::make_shared<PongState>();
  state->Reset();

  struct PongComp : public ftxui::ComponentBase {
    std::shared_ptr<PongState> s;
    int cb_id = -1;

    PongComp(std::shared_ptr<PongState> st) : s(std::move(st)) {
      cb_id = AnimationLoop::Instance().OnFrame(
          [this](float dt) { s->Update(dt); });
    }

    ~PongComp() override {
      if (cb_id != -1) {
        AnimationLoop::Instance().RemoveCallback(cb_id);
      }
    }

    ftxui::Element OnRender() override {
      auto game_view = s->renderer.Render();
      auto score = ftxui::hbox({
          ftxui::text(" Player 1: " + std::to_string(s->score_l)) |
              ftxui::color(ftxui::Color::Cyan),
          ftxui::filler(),
          ftxui::text("Player 2: " + std::to_string(s->score_r) + " ") |
              ftxui::color(ftxui::Color::Green),
      });
      auto controls = ftxui::text(" L:W/S  R:↑/↓  [Q]uit") |
                      ftxui::color(ftxui::Color::GrayDark);
      return ftxui::vbox({score, game_view | ftxui::flex, controls}) |
             ftxui::border;
    }

    bool OnEvent(ftxui::Event e) override {
      if (e == ftxui::Event::Character('w')) {
        s->up_l = true;
        return true;
      }
      if (e == ftxui::Event::Character('s')) {
        s->down_l = true;
        return true;
      }
      if (e == ftxui::Event::ArrowUp) {
        s->up_r = true;
        return true;
      }
      if (e == ftxui::Event::ArrowDown) {
        s->down_r = true;
        return true;
      }
      // Key releases handled via custom events not available directly;
      // use a timer approach: clear on each frame poll
      if (e.is_character()) {
        s->up_l = s->down_l = false;
      }
      return false;
    }
  };

  return std::make_shared<PongComp>(state);
}

// ── Breakout
// ──────────────────────────────────────────────────────────────────

ftxui::Component Breakout() {
  struct BreakoutState {
    std::shared_ptr<PhysicsWorld> world =
        std::make_shared<PhysicsWorld>(Vec2{0, 0});
    PhysicsRenderer renderer{world, 30.0f, 20.0f};
    int ball_id;
    int paddle_id;
    std::vector<int> brick_ids;
    int lives = 3;
    int score = 0;
    bool left = false, right = false;

    void Reset() {
      world->Clear();
      brick_ids.clear();
      world->SetBounds(30.0f, 20.0f);

      // Bricks in a grid
      for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 8; ++col) {
          float bx = 2.0f + col * 3.3f;
          float by = 2.0f + row * 1.5f;
          ftxui::Color c = (row == 0)   ? ftxui::Color::Red
                           : (row == 1) ? ftxui::Color::Yellow
                                        : ftxui::Color::Green;
          int id = world->AddRect({bx, by}, 2.6f, 1.0f, 0.0f, c);
          brick_ids.push_back(id);
        }
      }

      ball_id = world->AddCircle({15, 15}, 0.5f, 1.0f, ftxui::Color::White);
      paddle_id =
          world->AddRect({15, 18.5f}, 4.0f, 0.8f, 0.0f, ftxui::Color::Cyan);
      world->SetVelocity(ball_id, {8.0f, -12.0f});

      world->OnCollision([this](int a, int b) {
        for (auto it = brick_ids.begin(); it != brick_ids.end();) {
          if (*it == a || *it == b) {
            auto* body = world->GetBody(*it);
            if (body) {
              body->active = false;
              score += 10;
            }
            it = brick_ids.erase(it);
          } else {
            ++it;
          }
        }
      });
    }

    void Update(float dt) {
      const float pspeed = 18.0f;
      auto* p = world->GetBody(paddle_id);
      if (p) {
        float vx = 0;
        if (left) {
          vx = -pspeed;
        }
        if (right) {
          vx = pspeed;
        }
        p->velocity = {vx, 0};
        p->position.x += vx * dt;
        p->position.x = std::max(2.0f, std::min(28.0f, p->position.x));
      }

      world->Step(dt);

      auto* ball = world->GetBody(ball_id);
      if (ball && ball->position.y > 20.0f) {
        lives--;
        if (lives > 0) {
          world->SetPosition(ball_id, {15, 15});
          world->SetVelocity(ball_id, {8.0f, -12.0f});
        }
      }

      // Speed cap
      if (ball) {
        float sp = ball->velocity.length();
        if (sp > 20.0f) {
          ball->velocity = ball->velocity.normalized() * 20.0f;
        }
        if (sp < 8.0f && sp > 0.1f) {
          ball->velocity = ball->velocity.normalized() * 8.0f;
        }
      }
    }
  };

  auto state = std::make_shared<BreakoutState>();
  state->Reset();

  struct BreakoutComp : public ftxui::ComponentBase {
    std::shared_ptr<BreakoutState> s;
    int cb_id = -1;

    BreakoutComp(std::shared_ptr<BreakoutState> st) : s(std::move(st)) {
      cb_id = AnimationLoop::Instance().OnFrame(
          [this](float dt) { s->Update(dt); });
    }

    ~BreakoutComp() override {
      if (cb_id != -1) {
        AnimationLoop::Instance().RemoveCallback(cb_id);
      }
    }

    ftxui::Element OnRender() override {
      std::string status;
      if (s->lives <= 0) {
        status = " GAME OVER — Score: " + std::to_string(s->score);
      } else if (s->brick_ids.empty()) {
        status = " YOU WIN! — Score: " + std::to_string(s->score);
      } else {
        status = " Lives: " + std::to_string(s->lives) +
                 "  Score: " + std::to_string(s->score);
      }
      return ftxui::vbox({
                 ftxui::text(status) | ftxui::color(ftxui::Color::Yellow),
                 s->renderer.Render() | ftxui::flex,
                 ftxui::text(" ←/→ Move  [R]eset") |
                     ftxui::color(ftxui::Color::GrayDark),
             }) |
             ftxui::border;
    }

    bool OnEvent(ftxui::Event e) override {
      if (e == ftxui::Event::ArrowLeft) {
        s->left = true;
        return true;
      }
      if (e == ftxui::Event::ArrowRight) {
        s->right = true;
        return true;
      }
      if (e == ftxui::Event::Character('r') ||
          e == ftxui::Event::Character('R')) {
        s->Reset();
        return true;
      }
      return false;
    }
  };

  return std::make_shared<BreakoutComp>(state);
}

// ── Gravity Sandbox
// ────────────────────────────────────────────────────────────

ftxui::Component Gravity() {
  struct GravityState {
    std::shared_ptr<PhysicsWorld> world =
        std::make_shared<PhysicsWorld>(Vec2{0, 9.8f});
    PhysicsRenderer renderer{world, 40.0f, 20.0f};
    int body_count = 0;

    GravityState() { world->SetBounds(40.0f, 20.0f); }

    void AddBall() {
      static const ftxui::Color colors[] = {
          ftxui::Color::Red,    ftxui::Color::Green, ftxui::Color::Blue,
          ftxui::Color::Yellow, ftxui::Color::Cyan,  ftxui::Color::Magenta,
          ftxui::Color::White,
      };
      float x = 5.0f + static_cast<float>(body_count % 7) * 4.5f;
      float r = 0.5f + static_cast<float>(body_count % 3) * 0.3f;
      ftxui::Color c = colors[body_count % 7];
      world->AddCircle({x, 2.0f}, r, 1.0f, c);
      body_count++;
    }
  };

  auto state = std::make_shared<GravityState>();

  struct GravityComp : public ftxui::ComponentBase {
    std::shared_ptr<GravityState> s;
    int cb_id = -1;

    GravityComp(std::shared_ptr<GravityState> st) : s(std::move(st)) {
      cb_id = AnimationLoop::Instance().OnFrame(
          [this](float dt) { s->world->Step(dt); });
    }

    ~GravityComp() override {
      if (cb_id != -1) {
        AnimationLoop::Instance().RemoveCallback(cb_id);
      }
    }

    ftxui::Element OnRender() override {
      int n = static_cast<int>(s->world->Bodies().size());
      return ftxui::vbox({
                 ftxui::text(" Gravity Sandbox — " + std::to_string(n) +
                             " bodies  [Space]=add  [C]=clear") |
                     ftxui::color(ftxui::Color::Yellow),
                 s->renderer.Render() | ftxui::flex,
             }) |
             ftxui::border;
    }

    bool OnEvent(ftxui::Event e) override {
      if (e == ftxui::Event::Character(' ')) {
        s->AddBall();
        return true;
      }
      if (e == ftxui::Event::Character('c') ||
          e == ftxui::Event::Character('C')) {
        s->world->Clear();
        s->body_count = 0;
        return true;
      }
      return false;
    }
  };

  return std::make_shared<GravityComp>(state);
}

// ── Billiards
// ─────────────────────────────────────────────────────────────────

ftxui::Component Billiards() {
  struct BillardsState {
    std::shared_ptr<PhysicsWorld> world =
        std::make_shared<PhysicsWorld>(Vec2{0, 0});
    PhysicsRenderer renderer{world, 40.0f, 20.0f};
    int cue_id;
    float aim_angle = 0.0f;
    float shoot_power = 12.0f;

    BillardsState() {
      world->SetBounds(40.0f, 20.0f);

      // 15-ball triangle formation
      static const ftxui::Color ball_colors[] = {
          ftxui::Color::Yellow,     ftxui::Color::Blue,
          ftxui::Color::Red,        ftxui::Color::MediumPurple,
          ftxui::Color::DarkOrange, ftxui::Color::Green,
          ftxui::Color::Cyan,       ftxui::Color::White,
          ftxui::Color::Yellow,     ftxui::Color::Blue,
          ftxui::Color::Red,        ftxui::Color::MediumPurple,
          ftxui::Color::DarkOrange, ftxui::Color::Green,
          ftxui::Color::Cyan,
      };
      int idx = 0;
      float start_x = 28.0f;
      for (int row = 0; row < 5; ++row) {
        for (int col = 0; col <= row; ++col) {
          float bx = start_x + row * 1.1f;
          float by = 10.0f - row * 0.55f + col * 1.1f;
          world->AddCircle({bx, by}, 0.5f, 1.0f, ball_colors[idx % 15]);
          // High restitution for billiards
          if (auto* b = world->GetBody(world->Bodies().back().id)) {
            b->restitution = 0.95f;
          }
          idx++;
        }
      }

      cue_id = world->AddCircle({12, 10}, 0.5f, 1.0f, ftxui::Color::White);
      if (auto* b = world->GetBody(cue_id)) {
        b->restitution = 0.95f;
      }
    }

    void Shoot() {
      Vec2 dir = {std::cos(aim_angle), std::sin(aim_angle)};
      world->SetVelocity(cue_id, dir * shoot_power);
    }
  };

  auto state = std::make_shared<BillardsState>();

  struct BillardsComp : public ftxui::ComponentBase {
    std::shared_ptr<BillardsState> s;
    int cb_id = -1;

    BillardsComp(std::shared_ptr<BillardsState> st) : s(std::move(st)) {
      cb_id = AnimationLoop::Instance().OnFrame(
          [this](float dt) { s->world->Step(dt); });
    }

    ~BillardsComp() override {
      if (cb_id != -1) {
        AnimationLoop::Instance().RemoveCallback(cb_id);
      }
    }

    ftxui::Element OnRender() override {
      std::string angle_str =
          std::to_string(static_cast<int>(s->aim_angle * 57.3f)) + "°";
      return ftxui::vbox({
                 ftxui::hbox({
                     ftxui::text(" Billiards  aim:" + angle_str) |
                         ftxui::color(ftxui::Color::Yellow),
                     ftxui::filler(),
                     ftxui::text("←/→ aim  [Space] shoot  [R] reset ") |
                         ftxui::color(ftxui::Color::GrayDark),
                 }),
                 s->renderer.Render() | ftxui::flex,
             }) |
             ftxui::border;
    }

    bool OnEvent(ftxui::Event e) override {
      if (e == ftxui::Event::ArrowLeft) {
        s->aim_angle -= 0.1f;
        return true;
      }
      if (e == ftxui::Event::ArrowRight) {
        s->aim_angle += 0.1f;
        return true;
      }
      if (e == ftxui::Event::Character(' ')) {
        s->Shoot();
        return true;
      }
      if (e == ftxui::Event::Character('r') ||
          e == ftxui::Event::Character('R')) {
        s->world->Clear();
        *s = BillardsState();
        return true;
      }
      return false;
    }
  };

  return std::make_shared<BillardsComp>(state);
}

}  // namespace Games

}  // namespace ftxui::ui
