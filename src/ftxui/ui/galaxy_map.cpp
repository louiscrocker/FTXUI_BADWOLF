// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/galaxy_map.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/reactive_list.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kDeg2Rad = kPi / 180.0;

// ── Orthographic projection ───────────────────────────────────────────────────

namespace {

struct ProjResult {
  int px, py;
  bool visible;
};

inline ProjResult OrthoProject(double lon, double lat, double lon0,
                                double lat0, double cx, double cy,
                                double radius) {
  const double lam = lon * kDeg2Rad;
  const double phi = lat * kDeg2Rad;
  const double lam0 = lon0 * kDeg2Rad;
  const double phi0 = lat0 * kDeg2Rad;

  const double cos_c = std::sin(phi0) * std::sin(phi) +
                       std::cos(phi0) * std::cos(phi) * std::cos(lam - lam0);
  if (cos_c <= 0.0) {
    return {0, 0, false};
  }

  const double x = std::cos(phi) * std::sin(lam - lam0);
  const double y = std::cos(phi0) * std::sin(phi) -
                   std::sin(phi0) * std::cos(phi) * std::cos(lam - lam0);

  return {static_cast<int>(std::round(cx + x * radius)),
          static_cast<int>(std::round(cy - y * radius)),
          true};
}

}  // namespace

// ── Impl ──────────────────────────────────────────────────────────────────────

struct GalaxyMap::Impl {
  // Data
  std::vector<StarObject> stars_;
  std::vector<FleetMarker> static_fleets_;
  std::vector<SpaceArc> routes_;
  std::shared_ptr<ReactiveList<FleetMarker>> bound_fleets_;
  int fleet_listener_id_ = -1;

  // View state
  double center_ra_ = 0.0;
  double center_dec_ = 0.0;
  double zoom_ = 1.0;
  bool show_grid_ = false;
  float rotate_speed_ = 0.0f;

  // Selection
  std::function<void(std::string)> on_select_;
  int selected_index_ = -1;

  // Auto-rotation animation
  std::atomic<bool> running_{false};
  std::thread anim_thread_;

  ~Impl() {
    running_.store(false);
    if (anim_thread_.joinable()) {
      anim_thread_.join();
    }
  }

  void StartAnimationThread() {
    if (running_.load()) return;
    running_.store(true);
    anim_thread_ = std::thread([this]() {
      while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (!running_.load()) break;
        if (rotate_speed_ != 0.0f) {
          center_ra_ += static_cast<double>(rotate_speed_) * 0.05;
          if (center_ra_ > 360.0) center_ra_ -= 360.0;
          if (center_ra_ < 0.0) center_ra_ += 360.0;
          if (App* a = App::Active()) {
            a->Post([] {});
          }
        }
      }
    });
  }

  void DrawOnto(ftxui::Canvas& c) const;
  bool HandleEvent(ftxui::Event event);

  // Get all fleet markers (static + bound)
  std::vector<FleetMarker> AllFleets() const {
    auto result = static_fleets_;
    if (bound_fleets_) {
      auto items = bound_fleets_->Items();
      result.insert(result.end(), items.begin(), items.end());
    }
    return result;
  }
};

// ── Canvas rendering ──────────────────────────────────────────────────────────

void GalaxyMap::Impl::DrawOnto(ftxui::Canvas& c) const {
  const int dot_w = c.width();
  const int dot_h = c.height();

  const double radius_raw =
      static_cast<double>(std::min(dot_w, dot_h)) / 2.0 - 2.0;
  const double radius = radius_raw * zoom_;
  const double cx = dot_w / 2.0;
  const double cy = dot_h / 2.0;

  if (radius < 4.0) return;

  const double lon0 = center_ra_;
  const double lat0 = center_dec_;

  // Disc border (space background)
  {
    const ftxui::Color border_col(15, 15, 40);
    constexpr int kSegs = 180;
    for (int i = 0; i < kSegs; ++i) {
      const double a1 = i * 2.0 * kPi / kSegs;
      const double a2 = (i + 1) * 2.0 * kPi / kSegs;
      const int x1 = static_cast<int>(cx + radius * std::cos(a1));
      const int y1 = static_cast<int>(cy + radius * std::sin(a1));
      const int x2 = static_cast<int>(cx + radius * std::cos(a2));
      const int y2 = static_cast<int>(cy + radius * std::sin(a2));
      c.DrawPointLine(x1, y1, x2, y2, border_col);
    }
  }

  // Project helper
  auto project = [&](double lon, double lat) -> ProjResult {
    return OrthoProject(lon, lat, lon0, lat0, cx, cy, radius);
  };

  // Grid
  if (show_grid_) {
    const ftxui::Color grid_col(30, 30, 60);
    constexpr int kGratSteps = 360;
    constexpr float kGridStep = 30.0f;

    // Dec parallels
    for (float dec = -90.0f + kGridStep; dec < 90.0f; dec += kGridStep) {
      ProjResult prev{0, 0, false};
      for (int i = 0; i <= kGratSteps; ++i) {
        const double lon = -180.0 + i * 360.0 / kGratSteps;
        const auto cur = project(lon, static_cast<double>(dec));
        if (cur.visible && prev.visible) {
          c.DrawPointLine(prev.px, prev.py, cur.px, cur.py, grid_col);
        }
        prev = cur;
      }
    }

    // RA meridians
    for (float ra = 0.0f; ra < 360.0f; ra += kGridStep) {
      ProjResult prev{0, 0, false};
      for (int i = 0; i <= kGratSteps / 2; ++i) {
        const double lat = -90.0 + i * 180.0 / (kGratSteps / 2);
        const auto cur = project(static_cast<double>(ra), lat);
        if (cur.visible && prev.visible) {
          c.DrawPointLine(prev.px, prev.py, cur.px, cur.py, grid_col);
        }
        prev = cur;
      }
    }
  }

  // Hyperspace routes
  for (const auto& arc : routes_) {
    constexpr int kArcSteps = 64;
    ProjResult prev{0, 0, false};
    for (int i = 0; i <= kArcSteps; ++i) {
      const double t = static_cast<double>(i) / kArcSteps;
      // Great circle interpolation (simple linear in lon/lat for display)
      const double lon = arc.ra1 + t * (arc.ra2 - arc.ra1);
      const double lat = arc.dec1 + t * (arc.dec2 - arc.dec1);
      const auto cur = project(lon, lat);
      if (cur.visible && prev.visible) {
        c.DrawPointLine(prev.px, prev.py, cur.px, cur.py, arc.color);
      }
      prev = cur;
    }
  }

  // Stars
  for (size_t i = 0; i < stars_.size(); ++i) {
    const auto& star = stars_[i];
    const auto p = project(static_cast<double>(star.ra),
                            static_cast<double>(star.dec));
    if (!p.visible) continue;

    // Distance shading: closer stars are brighter
    ftxui::Color col = star.color;
    if (star.distance > 0.0f && star.distance < 1000.0f) {
      float brightness = 1.0f - std::min(star.distance / 500.0f, 0.8f);
      (void)brightness;  // color is already set per-star
    }

    c.DrawPoint(p.px, p.py, true, col);

    // Named stars get their symbol drawn around them
    if (!star.name.empty() && star.symbol != '*') {
      // Draw the symbol offset
      c.DrawText(p.px + 1, p.py, std::string(1, star.symbol), col);
    }
  }

  // Fleet markers
  const auto fleets = AllFleets();
  for (const auto& fleet : fleets) {
    const auto p = project(static_cast<double>(fleet.ra),
                            static_cast<double>(fleet.dec));
    if (!p.visible) continue;
    // Draw a small cross for fleets
    c.DrawPoint(p.px, p.py, true, fleet.color);
    c.DrawPoint(p.px + 1, p.py, true, fleet.color);
    c.DrawPoint(p.px - 1, p.py, true, fleet.color);
    c.DrawPoint(p.px, p.py + 1, true, fleet.color);
    c.DrawPoint(p.px, p.py - 1, true, fleet.color);
  }
}

// ── Event handling ────────────────────────────────────────────────────────────

bool GalaxyMap::Impl::HandleEvent(ftxui::Event event) {
  if (event == Event::ArrowLeft) {
    center_ra_ -= 5.0;
    if (center_ra_ < 0.0) center_ra_ += 360.0;
    return true;
  }
  if (event == Event::ArrowRight) {
    center_ra_ += 5.0;
    if (center_ra_ >= 360.0) center_ra_ -= 360.0;
    return true;
  }
  if (event == Event::ArrowUp) {
    center_dec_ += 5.0;
    if (center_dec_ > 90.0) center_dec_ = 90.0;
    return true;
  }
  if (event == Event::ArrowDown) {
    center_dec_ -= 5.0;
    if (center_dec_ < -90.0) center_dec_ = -90.0;
    return true;
  }
  if (event == Event::Character('+') || event == Event::Character('=')) {
    zoom_ = std::min(zoom_ * 1.2, 10.0);
    return true;
  }
  if (event == Event::Character('-')) {
    zoom_ = std::max(zoom_ / 1.2, 0.1);
    return true;
  }
  if (event.is_mouse()) {
    const auto& mouse = event.mouse();
    if (mouse.button == Mouse::WheelUp) {
      zoom_ = std::min(zoom_ * 1.1, 10.0);
      return true;
    }
    if (mouse.button == Mouse::WheelDown) {
      zoom_ = std::max(zoom_ / 1.1, 0.1);
      return true;
    }
  }
  if (event == Event::Return && on_select_ && selected_index_ >= 0 &&
      selected_index_ < static_cast<int>(stars_.size())) {
    on_select_(stars_[static_cast<size_t>(selected_index_)].name);
    return true;
  }
  return false;
}

// ── Builder ───────────────────────────────────────────────────────────────────

GalaxyMap::GalaxyMap() : impl_(std::make_shared<Impl>()) {}

GalaxyMap& GalaxyMap::Stars(int count, int seed) {
  auto stars = StarFields::Procedural(count, seed);
  impl_->stars_.insert(impl_->stars_.end(), stars.begin(), stars.end());
  return *this;
}

GalaxyMap& GalaxyMap::AddStar(StarObject star) {
  impl_->stars_.push_back(std::move(star));
  return *this;
}

GalaxyMap& GalaxyMap::StarField(std::vector<StarObject> stars) {
  impl_->stars_ = std::move(stars);
  return *this;
}

GalaxyMap& GalaxyMap::AddFleet(FleetMarker fleet) {
  impl_->static_fleets_.push_back(std::move(fleet));
  return *this;
}

GalaxyMap& GalaxyMap::AddRoute(SpaceArc arc) {
  impl_->routes_.push_back(std::move(arc));
  return *this;
}

GalaxyMap& GalaxyMap::BindFleets(
    std::shared_ptr<ReactiveList<FleetMarker>> fleets) {
  impl_->bound_fleets_ = std::move(fleets);
  return *this;
}

GalaxyMap& GalaxyMap::Center(float ra, float dec) {
  impl_->center_ra_ = static_cast<double>(ra);
  impl_->center_dec_ = static_cast<double>(dec);
  return *this;
}

GalaxyMap& GalaxyMap::Zoom(float zoom) {
  impl_->zoom_ = static_cast<double>(zoom);
  return *this;
}

GalaxyMap& GalaxyMap::Grid(bool show) {
  impl_->show_grid_ = show;
  return *this;
}

GalaxyMap& GalaxyMap::Rotate(float deg_per_second) {
  impl_->rotate_speed_ = deg_per_second;
  return *this;
}

GalaxyMap& GalaxyMap::OnSelect(std::function<void(std::string name)> fn) {
  impl_->on_select_ = std::move(fn);
  return *this;
}

ftxui::Component GalaxyMap::Build() {
  auto impl = impl_;
  if (impl->rotate_speed_ != 0.0f) {
    impl->StartAnimationThread();
  }

  // Subscribe to bound fleets if set
  if (impl->bound_fleets_) {
    impl->bound_fleets_->OnChange([](const std::vector<FleetMarker>&) {
      if (App* a = App::Active()) {
        a->Post([] {});
      }
    });
  }

  auto renderer = Renderer([impl]() -> Element {
    return canvas(2, 4, [impl](Canvas& c) { impl->DrawOnto(c); }) | flex;
  });

  return CatchEvent(renderer, [impl](Event event) -> bool {
    return impl->HandleEvent(event);
  });
}

// ── StarFields namespace ──────────────────────────────────────────────────────

namespace StarFields {

std::vector<StarObject> StarWars() {
  return {
      {"Coruscant", 75.f, 15.f, 0.f, ftxui::Color::Yellow, 'o'},
      {"Tatooine", 220.f, -25.f, 43.f, ftxui::Color::YellowLight, 'o'},
      {"Hoth", 310.f, 60.f, 80.f, ftxui::Color::White, 'o'},
      {"Ryloth", 155.f, 30.f, 25.f, ftxui::Color::RedLight, 'o'},
      {"Naboo", 95.f, 5.f, 15.f, ftxui::Color::GreenLight, 'o'},
      {"Dagobah", 250.f, -70.f, 90.f, ftxui::Color::Green, 'o'},
      {"Endor", 185.f, -15.f, 60.f, ftxui::Color::GreenLight, 'o'},
      {"Mustafar", 40.f, -45.f, 55.f, ftxui::Color::Red, 'o'},
      {"Death Star", 180.f, 0.f, 30.f, ftxui::Color::GrayLight, '+'},
      {"Alderaan", 65.f, 25.f, 10.f, ftxui::Color::CyanLight, 'o'},
  };
}

std::vector<StarObject> StarTrek() {
  return {
      {"Earth/Sol", 0.f, 0.f, 0.f, ftxui::Color::CyanLight, 'o'},
      {"Vulcan", 55.f, 40.f, 16.f, ftxui::Color::Yellow, 'o'},
      {"Qo'noS", 120.f, 25.f, 90.f, ftxui::Color::RedLight, 'o'},
      {"Romulus", 200.f, -20.f, 160.f, ftxui::Color::GreenLight, 'o'},
      {"Cardassia", 270.f, 10.f, 130.f, ftxui::Color::GrayLight, 'o'},
      {"DS9/Bajor", 310.f, 45.f, 120.f, ftxui::Color::YellowLight, '+'},
      {"Ferenginar", 160.f, -30.f, 75.f, ftxui::Color::Yellow, 'o'},
      {"Betazed", 85.f, 50.f, 40.f, ftxui::Color::MagentaLight, 'o'},
      {"Risa", 145.f, 20.f, 90.f, ftxui::Color::CyanLight, 'o'},
      {"Borg Cube", 340.f, 70.f, 7000.f, ftxui::Color::GreenLight, '+'},
  };
}

std::vector<StarObject> Procedural(int count, int seed) {
  std::vector<StarObject> stars;
  stars.reserve(static_cast<size_t>(count));

  // Simple LCG random number generator (no external deps)
  uint64_t rng = static_cast<uint64_t>(seed) ^ 0xDEADBEEFCAFEBABEULL;
  auto next_rand = [&]() -> double {
    rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<double>(rng >> 33) / static_cast<double>(1ULL << 31);
  };

  const ftxui::Color star_colors[] = {
      ftxui::Color::White,      ftxui::Color::GrayLight,
      ftxui::Color::CyanLight,  ftxui::Color::YellowLight,
      ftxui::Color::BlueLight,  ftxui::Color::RedLight,
  };
  constexpr size_t kNumColors =
      sizeof(star_colors) / sizeof(star_colors[0]);

  for (int i = 0; i < count; ++i) {
    StarObject s;
    s.ra = static_cast<float>(next_rand() * 360.0);
    // Uniform distribution over sphere surface: dec = asin(2*u - 1)
    s.dec = static_cast<float>(
        std::asin(2.0 * next_rand() - 1.0) * 180.0 / kPi);
    s.distance = static_cast<float>(next_rand() * 1000.0);
    s.color = star_colors[static_cast<size_t>(next_rand() * kNumColors) %
                           kNumColors];
    s.symbol = '*';
    stars.push_back(std::move(s));
  }
  return stars;
}

}  // namespace StarFields

}  // namespace ftxui::ui
