// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_PARTICLES_HPP
#define FTXUI_UI_PARTICLES_HPP

#include <memory>
#include <mutex>
#include <vector>

#include "ftxui/dom/canvas.hpp"

namespace ftxui::ui {

// ---------------------------------------------------------------------------
// Particle
// ---------------------------------------------------------------------------

/// @brief State of a single particle.
struct Particle {
  float x, y;           ///< Position in canvas dot coordinates.
  float vx, vy;         ///< Velocity in dots per second.
  float lifetime;       ///< Remaining lifetime in seconds.
  float max_lifetime;   ///< Initial lifetime (for alpha fade).
  float size;           ///< Radius in dots (1.0 = single dot).
};

// ---------------------------------------------------------------------------
// ParticleConfig
// ---------------------------------------------------------------------------

/// @brief Configuration for a ParticleSystem emitter.
struct ParticleConfig {
  float emit_x = 0.0f;          ///< Emission point X (dot coords).
  float emit_y = 0.0f;          ///< Emission point Y (dot coords).
  float emit_radius = 2.0f;     ///< Random spread radius around emit point.
  float speed_min = 20.0f;      ///< Min initial speed (dots/sec).
  float speed_max = 60.0f;      ///< Max initial speed (dots/sec).
  float angle_min = 0.0f;       ///< Emission angle range start (radians).
  float angle_max = 6.2831853f; ///< Emission angle range end (full circle).
  float lifetime_min = 0.5f;    ///< Min particle lifetime (seconds).
  float lifetime_max = 1.5f;    ///< Max particle lifetime (seconds).
  float gravity = 30.0f;        ///< Downward acceleration (dots/sec²).
  int   emit_rate = 20;         ///< Particles emitted per second.
};

// ---------------------------------------------------------------------------
// ParticleSystem
// ---------------------------------------------------------------------------

/// @brief A braille-resolution particle system rendered into an ftxui::Canvas.
///
/// Usage:
/// @code
/// auto ps = std::make_shared<ParticleSystem>(config);
/// ps->Start();
///
/// auto element = canvas(2, 4, [ps](Canvas& c) {
///     ps->Render(c);
/// }) | flex;
/// @endcode
///
/// @ingroup ui
class ParticleSystem {
 public:
  explicit ParticleSystem(ParticleConfig config = {});
  ~ParticleSystem();

  /// @brief Begin emitting particles; registers an AnimationLoop frame callback.
  void Start();

  /// @brief Stop emitting new particles (existing particles finish naturally).
  void Stop();

  /// @brief Immediately remove all live particles.
  void Clear();

  /// @brief Move the emitter to a new position.
  void SetPosition(float x, float y);

  /// @brief Replace the emitter configuration.
  void SetConfig(ParticleConfig cfg);

  /// @brief Advance particle physics by `dt` seconds. Called by the frame
  /// callback registered in Start().
  void Update(float dt);

  /// @brief Render all live particles into the canvas.
  void Render(ftxui::Canvas& c) const;

  /// @brief Return the number of currently live particles.
  int LiveCount() const;

 private:
  ParticleConfig config_;
  std::vector<Particle> particles_;
  float emit_accumulator_{0.0f};
  bool running_{true};  // emitting by default; Stop() disables
  mutable std::mutex mutex_;
  int frame_callback_id_{-1};

  void Emit(int count);
};

// ---------------------------------------------------------------------------
// Preset factories
// ---------------------------------------------------------------------------

/// @brief Explosion: burst outward in all directions with gravity and fade.
std::shared_ptr<ParticleSystem> Explosion(float x, float y);

/// @brief Thruster: directional stream (e.g. for a rocket/ship exhaust).
/// @param angle_rad Direction the thrust comes FROM (particles go opposite way).
std::shared_ptr<ParticleSystem> Thruster(float x, float y, float angle_rad);

/// @brief Warp: horizontal streaks flying past at warp speed.
std::shared_ptr<ParticleSystem> WarpStreaks();

/// @brief Rain: particles falling from the top of the canvas.
std::shared_ptr<ParticleSystem> Rain();

/// @brief Sparkle: random sparkling dots around a point.
std::shared_ptr<ParticleSystem> Sparkle(float x, float y);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_PARTICLES_HPP
