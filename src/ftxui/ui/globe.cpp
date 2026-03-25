// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/globe.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <utility>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/geojson.hpp"
#include "ftxui/ui/world_map_data.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Constants
// ─────────────────────────────────────────────────────────────────

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kDeg2Rad = kPi / 180.0;

// ── Impl
// ──────────────────────────────────────────────────────────────────────

struct GlobeMap::Impl {
  // Config
  ftxui::Color line_color_ = ftxui::Color::GreenLight;
  ftxui::Color point_color_ = ftxui::Color::Yellow;
  double speed_ = 1.0;
  double rotation_lon_ = 0.0;
  double center_lat_ = 20.0;
  bool show_graticule_ = false;
  float graticule_step_ = 30.0f;

  // Map data
  GeoCollection collection_;

  // Animation
  std::atomic<bool> running_{false};
  std::thread anim_thread_;

  ~Impl() {
    running_.store(false);
    if (anim_thread_.joinable()) {
      anim_thread_.join();
    }
  }

  void StartAnimationThread();
  void DrawOnto(ftxui::Canvas& c) const;
  bool HandleEvent(ftxui::Event event);
};

// ── Animation thread
// ──────────────────────────────────────────────────────────

void GlobeMap::Impl::StartAnimationThread() {
  if (running_.load()) {
    return;  // already started
  }
  running_.store(true);

  anim_thread_ = std::thread([this]() {
    while (running_.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      if (!running_.load()) {
        break;
      }

      if (speed_ != 0.0) {
        rotation_lon_ += speed_;
        if (rotation_lon_ > 360.0) {
          rotation_lon_ -= 360.0;
        }
        if (rotation_lon_ < -360.0) {
          rotation_lon_ += 360.0;
        }
      }

      if (App* a = App::Active()) {
        a->Post([] {});
      }
    }
  });
}

// ── Orthographic projection helpers ──────────────────────────────────────────

namespace {

struct ProjectResult {
  int px, py;
  bool visible;
};

inline ProjectResult OrthoProject(double lon,
                                  double lat,
                                  double lon0,
                                  double lat0,
                                  double cx,
                                  double cy,
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

  return {static_cast<int>(cx + x * radius),
          static_cast<int>(cy - y * radius),  // screen Y flipped
          true};
}

}  // namespace

// ── Canvas rendering
// ──────────────────────────────────────────────────────────

void GlobeMap::Impl::DrawOnto(ftxui::Canvas& c) const {
  const int dot_w = c.width();
  const int dot_h = c.height();

  // Globe geometry
  const double radius = static_cast<double>(std::min(dot_w, dot_h)) / 2.0 - 4.0;
  const double cx = dot_w / 2.0;
  const double cy = dot_h / 2.0;
  const double lon0 = rotation_lon_;
  const double lat0 = center_lat_;

  if (radius < 4.0) {
    return;
  }

  // Draw disc background (faint circle border)
  {
    const ftxui::Color circle_col(30, 30, 60);
    constexpr int kCircleSegs = 180;
    for (int i = 0; i < kCircleSegs; ++i) {
      const double a1 = i * 2.0 * kPi / kCircleSegs;
      const double a2 = (i + 1) * 2.0 * kPi / kCircleSegs;
      const int x1 = static_cast<int>(cx + radius * std::cos(a1));
      const int y1 = static_cast<int>(cy + radius * std::sin(a1));
      const int x2 = static_cast<int>(cx + radius * std::cos(a2));
      const int y2 = static_cast<int>(cy + radius * std::sin(a2));
      c.DrawPointLine(x1, y1, x2, y2, circle_col);
    }
  }

  // Project helper lambda
  auto project = [&](double lon, double lat) -> ProjectResult {
    return OrthoProject(lon, lat, lon0, lat0, cx, cy, radius);
  };

  // Draw graticule
  if (show_graticule_) {
    const ftxui::Color gray(45, 45, 70);
    const float step = graticule_step_;
    constexpr int kGratSteps = 360;

    // Latitude parallels
    for (float lat = -90.0f + step; lat < 90.0f; lat += step) {
      ProjectResult prev{0, 0, false};
      for (int i = 0; i <= kGratSteps; ++i) {
        const double lon = -180.0 + i * 360.0 / kGratSteps;
        const auto cur = project(lon, lat);
        if (cur.visible && prev.visible) {
          c.DrawPointLine(prev.px, prev.py, cur.px, cur.py, gray);
        }
        prev = cur;
      }
    }

    // Longitude meridians
    for (float lon = -180.0f; lon < 180.0f; lon += step) {
      ProjectResult prev{0, 0, false};
      for (int i = 0; i <= kGratSteps / 2; ++i) {
        const double lat = -90.0 + i * 180.0 / (kGratSteps / 2);
        const auto cur = project(lon, lat);
        if (cur.visible && prev.visible) {
          c.DrawPointLine(prev.px, prev.py, cur.px, cur.py, gray);
        }
        prev = cur;
      }
    }
  }

  // Ring drawing helper — only draws segment if both endpoints are visible
  auto drawRing = [&](const GeoRing& ring, ftxui::Color col) {
    if (ring.size() < 2) {
      return;
    }
    for (size_t i = 0; i + 1 < ring.size(); ++i) {
      const auto p1 = project(ring[i].lon, ring[i].lat);
      const auto p2 = project(ring[i + 1].lon, ring[i + 1].lat);
      if (p1.visible && p2.visible) {
        c.DrawPointLine(p1.px, p1.py, p2.px, p2.py, col);
      }
    }
    // Close ring
    const auto p1 = project(ring.back().lon, ring.back().lat);
    const auto p2 = project(ring.front().lon, ring.front().lat);
    if (p1.visible && p2.visible) {
      c.DrawPointLine(p1.px, p1.py, p2.px, p2.py, col);
    }
  };

  // Draw map features
  for (const auto& feat : collection_.features) {
    const auto& geom = feat.geometry;

    if (geom.type == "Point" || geom.type == "MultiPoint") {
      for (const auto& pt : geom.points) {
        const auto p = project(pt.lon, pt.lat);
        if (p.visible) {
          c.DrawPoint(p.px, p.py, true, point_color_);
        }
      }
    } else if (geom.type == "LineString" || geom.type == "MultiLineString") {
      for (size_t i = 0; i + 1 < geom.points.size(); ++i) {
        const auto p1 = project(geom.points[i].lon, geom.points[i].lat);
        const auto p2 = project(geom.points[i + 1].lon, geom.points[i + 1].lat);
        if (p1.visible && p2.visible) {
          c.DrawPointLine(p1.px, p1.py, p2.px, p2.py, line_color_);
        }
      }
    } else if (geom.type == "Polygon") {
      for (const auto& ring : geom.rings) {
        drawRing(ring, line_color_);
      }
    } else if (geom.type == "MultiPolygon") {
      for (const auto& poly : geom.multipolygon) {
        for (const auto& ring : poly) {
          drawRing(ring, line_color_);
        }
      }
    }
  }
}

// ── Event handling
// ────────────────────────────────────────────────────────────

bool GlobeMap::Impl::HandleEvent(ftxui::Event event) {
  if (event == Event::ArrowLeft) {
    rotation_lon_ -= 5.0;
    return true;
  }
  if (event == Event::ArrowRight) {
    rotation_lon_ += 5.0;
    return true;
  }
  if (event == Event::ArrowUp) {
    center_lat_ += 5.0;
    if (center_lat_ > 90.0) {
      center_lat_ = 90.0;
    }
    return true;
  }
  if (event == Event::ArrowDown) {
    center_lat_ -= 5.0;
    if (center_lat_ < -90.0) {
      center_lat_ = -90.0;
    }
    return true;
  }
  if (event == Event::Character('+') || event == Event::Character('=')) {
    speed_ += 0.5;
    return true;
  }
  if (event == Event::Character('-')) {
    speed_ -= 0.5;
    return true;
  }
  if (event == Event::Character('g') || event == Event::Character('G')) {
    show_graticule_ = !show_graticule_;
    return true;
  }
  return false;
}

// ── GlobeMap builder
// ──────────────────────────────────────────────────────────

GlobeMap::GlobeMap() : impl_(std::make_shared<Impl>()) {
  impl_->collection_ = ParseGeoJSON(WorldMapGeoJSON());
}

GlobeMap& GlobeMap::LineColor(ftxui::Color c) {
  impl_->line_color_ = c;
  return *this;
}

GlobeMap& GlobeMap::PointColor(ftxui::Color c) {
  impl_->point_color_ = c;
  return *this;
}

GlobeMap& GlobeMap::RotationSpeed(double deg_per_tick) {
  impl_->speed_ = deg_per_tick;
  return *this;
}

GlobeMap& GlobeMap::InitialLon(double lon) {
  impl_->rotation_lon_ = lon;
  return *this;
}

GlobeMap& GlobeMap::InitialLat(double lat) {
  impl_->center_lat_ = lat;
  return *this;
}

GlobeMap& GlobeMap::ShowGraticule(bool v) {
  impl_->show_graticule_ = v;
  return *this;
}

GlobeMap& GlobeMap::GraticuleStep(float degrees) {
  impl_->graticule_step_ = degrees;
  return *this;
}

ftxui::Component GlobeMap::Build() {
  auto impl = impl_;
  impl->StartAnimationThread();

  auto renderer = Renderer([impl]() -> Element {
    return canvas(2, 4, [impl](Canvas& c) { impl->DrawOnto(c); }) | flex;
  });

  return CatchEvent(renderer, [impl](Event event) -> bool {
    return impl->HandleEvent(event);
  });
}

}  // namespace ftxui::ui
