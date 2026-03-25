// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/particles.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>

#include "ftxui/ui/animation.hpp"

namespace ftxui::ui {

namespace {

// Pseudo-random float in [lo, hi].
float Rand(float lo, float hi) {
  return lo + (hi - lo) * (static_cast<float>(std::rand()) /
                           static_cast<float>(RAND_MAX));
}

}  // namespace

// ---------------------------------------------------------------------------
// ParticleSystem
// ---------------------------------------------------------------------------

ParticleSystem::ParticleSystem(ParticleConfig config)
    : config_(std::move(config)) {}

ParticleSystem::~ParticleSystem() {
  Stop();
}

void ParticleSystem::Start() {
  std::lock_guard<std::mutex> lock(mutex_);
  running_ = true;
  if (frame_callback_id_ == -1) {
    frame_callback_id_ =
        AnimationLoop::Instance().OnFrame([this](float dt) { Update(dt); });
  }
}

void ParticleSystem::Stop() {
  std::lock_guard<std::mutex> lock(mutex_);
  running_ = false;
  if (frame_callback_id_ != -1) {
    AnimationLoop::Instance().RemoveCallback(frame_callback_id_);
    frame_callback_id_ = -1;
  }
}

void ParticleSystem::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  particles_.clear();
  emit_accumulator_ = 0.0f;
}

void ParticleSystem::SetPosition(float x, float y) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.emit_x = x;
  config_.emit_y = y;
}

void ParticleSystem::SetConfig(ParticleConfig cfg) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_ = std::move(cfg);
}

void ParticleSystem::Update(float dt) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Advance existing particles.
  for (auto& p : particles_) {
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    p.vy += config_.gravity * dt;
    p.lifetime -= dt;
  }

  // Remove dead particles.
  particles_.erase(
      std::remove_if(particles_.begin(), particles_.end(),
                     [](const Particle& p) { return p.lifetime <= 0.0f; }),
      particles_.end());

  // Emit new particles.
  if (running_) {
    emit_accumulator_ += static_cast<float>(config_.emit_rate) * dt;
    int n = static_cast<int>(emit_accumulator_);
    if (n > 0) {
      emit_accumulator_ -= static_cast<float>(n);
      Emit(n);
    }
  }
}

void ParticleSystem::Emit(int count) {
  // Assumes mutex_ is already held by the caller (Update).
  for (int i = 0; i < count; ++i) {
    Particle p;
    p.x = config_.emit_x + Rand(-config_.emit_radius, config_.emit_radius);
    p.y = config_.emit_y + Rand(-config_.emit_radius, config_.emit_radius);

    float angle = Rand(config_.angle_min, config_.angle_max);
    float speed = Rand(config_.speed_min, config_.speed_max);
    p.vx = std::cos(angle) * speed;
    p.vy = std::sin(angle) * speed;

    p.max_lifetime = Rand(config_.lifetime_min, config_.lifetime_max);
    p.lifetime = p.max_lifetime;
    p.size = 1.0f;

    particles_.push_back(p);
  }
}

void ParticleSystem::Render(ftxui::Canvas& c) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const int w = c.width();
  const int h = c.height();
  for (const auto& p : particles_) {
    float alpha = p.lifetime / p.max_lifetime;
    if (alpha < 0.1f) {
      continue;
    }
    int px = static_cast<int>(p.x);
    int py = static_cast<int>(p.y);
    if (px < 0 || px >= w || py < 0 || py >= h) {
      continue;
    }
    c.DrawPoint(px, py, true);
  }
}

int ParticleSystem::LiveCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<int>(particles_.size());
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------

std::shared_ptr<ParticleSystem> Explosion(float x, float y) {
  ParticleConfig cfg;
  cfg.emit_x = x;
  cfg.emit_y = y;
  cfg.emit_radius = 1.0f;
  cfg.speed_min = 30.0f;
  cfg.speed_max = 80.0f;
  cfg.angle_min = 0.0f;
  cfg.angle_max = 6.2831853f;
  cfg.lifetime_min = 0.3f;
  cfg.lifetime_max = 1.0f;
  cfg.gravity = 20.0f;
  cfg.emit_rate = 80;
  return std::make_shared<ParticleSystem>(cfg);
}

std::shared_ptr<ParticleSystem> Thruster(float x, float y, float angle_rad) {
  constexpr float kPi = 3.14159265358979323846f;
  ParticleConfig cfg;
  cfg.emit_x = x;
  cfg.emit_y = y;
  cfg.emit_radius = 1.0f;
  cfg.speed_min = 25.0f;
  cfg.speed_max = 55.0f;
  // Thrust is emitted in the direction opposite to `angle_rad`.
  cfg.angle_min = angle_rad + kPi - 0.4f;
  cfg.angle_max = angle_rad + kPi + 0.4f;
  cfg.lifetime_min = 0.2f;
  cfg.lifetime_max = 0.6f;
  cfg.gravity = 0.0f;
  cfg.emit_rate = 40;
  return std::make_shared<ParticleSystem>(cfg);
}

std::shared_ptr<ParticleSystem> WarpStreaks() {
  ParticleConfig cfg;
  cfg.emit_x = 0.0f;
  cfg.emit_y = 20.0f;
  cfg.emit_radius = 50.0f;  // Wide vertical spread
  cfg.speed_min = 80.0f;
  cfg.speed_max = 200.0f;
  cfg.angle_min = -0.2f;  // Mostly rightward
  cfg.angle_max = 0.2f;
  cfg.lifetime_min = 0.3f;
  cfg.lifetime_max = 0.8f;
  cfg.gravity = 0.0f;
  cfg.emit_rate = 30;
  return std::make_shared<ParticleSystem>(cfg);
}

std::shared_ptr<ParticleSystem> Rain() {
  constexpr float kPi = 3.14159265358979323846f;
  ParticleConfig cfg;
  cfg.emit_x = 40.0f;
  cfg.emit_y = 0.0f;
  cfg.emit_radius = 80.0f;  // Wide horizontal spread
  cfg.speed_min = 30.0f;
  cfg.speed_max = 60.0f;
  cfg.angle_min = kPi / 2.0f - 0.2f;  // Mostly downward
  cfg.angle_max = kPi / 2.0f + 0.2f;
  cfg.lifetime_min = 0.8f;
  cfg.lifetime_max = 1.8f;
  cfg.gravity = 0.0f;  // velocity already points down; gravity = free bonus
  cfg.emit_rate = 25;
  return std::make_shared<ParticleSystem>(cfg);
}

std::shared_ptr<ParticleSystem> Sparkle(float x, float y) {
  ParticleConfig cfg;
  cfg.emit_x = x;
  cfg.emit_y = y;
  cfg.emit_radius = 5.0f;
  cfg.speed_min = 5.0f;
  cfg.speed_max = 15.0f;
  cfg.angle_min = 0.0f;
  cfg.angle_max = 6.2831853f;
  cfg.lifetime_min = 0.2f;
  cfg.lifetime_max = 0.6f;
  cfg.gravity = -5.0f;  // slight upward drift
  cfg.emit_rate = 15;
  return std::make_shared<ParticleSystem>(cfg);
}

}  // namespace ftxui::ui
