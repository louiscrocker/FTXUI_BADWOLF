// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/geomap.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/box.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/geojson.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kDeg2Rad = kPi / 180.0;
static constexpr double kRad2Deg = 180.0 / kPi;

// ── Arc definition ────────────────────────────────────────────────────────────

struct ArcDef {
  double lon1, lat1, lon2, lat2;
  ftxui::Color color;
  int steps;
};

// ── Overlay definition ────────────────────────────────────────────────────────

struct MapOverlay {
  GeoCollection collection;
  ftxui::Color color;
};

// ── Impl ──────────────────────────────────────────────────────────────────────

struct GeoMap::Impl {
  GeoCollection collection_;
  Projection projection_ = Projection::Equirectangular;
  ftxui::Color point_color_ = ftxui::Color::Yellow;
  ftxui::Color line_color_ = ftxui::Color::Green;
  ftxui::Color fill_color_ = ftxui::Color::GreenLight;
  bool show_graticule_ = false;
  float graticule_step_ = 30.0f;
  std::function<void(const GeoFeature&)> on_select_;

  std::vector<MapOverlay> overlays_;
  std::vector<ArcDef> arcs_;

  // Pan / zoom state
  double pan_lon_ = 0.0;
  double pan_lat_ = 0.0;
  double zoom_ = 1.0;

  // Mouse drag state
  bool dragging_ = false;
  int drag_start_x_ = 0;
  int drag_start_y_ = 0;
  double drag_start_pan_lon_ = 0.0;
  double drag_start_pan_lat_ = 0.0;

  // Cached view bounds (updated each render for mouse coordinate conversion)
  double cached_lon_min_ = -180.0;
  double cached_lon_max_ = 180.0;
  double cached_lat_min_ = -90.0;
  double cached_lat_max_ = 90.0;
  int cached_dot_w_ = 2;
  int cached_dot_h_ = 4;
  ftxui::Box canvas_box_;

  // Double-click detection
  using Clock = std::chrono::steady_clock;
  Clock::time_point last_click_time_{};
  int last_click_x_ = -1;
  int last_click_y_ = -1;

  ftxui::Canvas RenderCanvas();
  bool HandleEvent(ftxui::Event event);

  // Project (lon, lat) → canvas (x, y) given explicit view bounds and dims.
  std::pair<int, int> Project(double lon, double lat, double lon_min,
                               double lon_max, double lat_min, double lat_max,
                               int dot_w, int dot_h) const;

  // Draw a closed ring onto the canvas using a project lambda.
  void DrawRing(ftxui::Canvas& c, const GeoRing& ring,
                const std::function<std::pair<int,int>(double,double)>& project,
                ftxui::Color col) const;

  // Draw a great-circle arc between two lon/lat positions.
  void DrawArc(ftxui::Canvas& c, const ArcDef& arc,
               const std::function<std::pair<int,int>(double,double)>& project) const;

  // Draw all features onto an existing Canvas (uses c.width()/c.height()).
  void DrawOnto(ftxui::Canvas& c);

  static double MercY(double lat_deg) {
    double rad = lat_deg * kDeg2Rad;
    if (rad > 1.4835) rad = 1.4835;   // clamp ~85°
    if (rad < -1.4835) rad = -1.4835;
    return std::log(std::tan(kPi / 4.0 + rad / 2.0));
  }
};

// ── Projection ────────────────────────────────────────────────────────────────

std::pair<int, int> GeoMap::Impl::Project(double lon, double lat,
                                            double lon_min, double lon_max,
                                            double lat_min, double lat_max,
                                            int dot_w, int dot_h) const {
  double px = 0.0;
  double py = 0.0;

  double dlon = lon_max - lon_min;
  if (dlon < 1e-10) dlon = 1e-10;

  if (projection_ == Projection::Equirectangular) {
    double dlat = lat_max - lat_min;
    if (dlat < 1e-10) dlat = 1e-10;
    px = (lon - lon_min) / dlon * (dot_w - 1);
    py = (lat_max - lat) / dlat * (dot_h - 1);
  } else {
    double y = MercY(lat);
    double y_min = MercY(lat_min);
    double y_max = MercY(lat_max);
    double dy = y_max - y_min;
    if (std::abs(dy) < 1e-10) dy = 1e-10;
    px = (lon - lon_min) / dlon * (dot_w - 1);
    py = (y_max - y) / dy * (dot_h - 1);
  }

  return {static_cast<int>(std::round(px)),
          static_cast<int>(std::round(py))};
}

// ── Ring drawing ──────────────────────────────────────────────────────────────

void GeoMap::Impl::DrawRing(
    ftxui::Canvas& c, const GeoRing& ring,
    const std::function<std::pair<int,int>(double,double)>& project,
    ftxui::Color col) const {
  if (ring.size() < 2) return;
  for (size_t i = 0; i + 1 < ring.size(); ++i) {
    auto [x1, y1] = project(ring[i].lon, ring[i].lat);
    auto [x2, y2] = project(ring[i + 1].lon, ring[i + 1].lat);
    c.DrawPointLine(x1, y1, x2, y2, col);
  }
  // Close the ring
  auto [x1, y1] = project(ring.back().lon, ring.back().lat);
  auto [x2, y2] = project(ring.front().lon, ring.front().lat);
  c.DrawPointLine(x1, y1, x2, y2, col);
}

// ── Great-circle arc drawing ──────────────────────────────────────────────────

void GeoMap::Impl::DrawArc(
    ftxui::Canvas& c, const ArcDef& arc,
    const std::function<std::pair<int,int>(double,double)>& project) const {
  // Convert lon/lat → unit 3D vector on unit sphere
  auto toVec = [](double lon_deg, double lat_deg,
                  double& x, double& y, double& z) {
    double lon = lon_deg * kDeg2Rad;
    double lat = lat_deg * kDeg2Rad;
    x = std::cos(lat) * std::cos(lon);
    y = std::cos(lat) * std::sin(lon);
    z = std::sin(lat);
  };

  double x1, y1, z1, x2, y2, z2;
  toVec(arc.lon1, arc.lat1, x1, y1, z1);
  toVec(arc.lon2, arc.lat2, x2, y2, z2);

  // Great-circle slerp: interpolate on sphere surface
  double dot = x1*x2 + y1*y2 + z1*z2;
  if (dot > 1.0) dot = 1.0;
  if (dot < -1.0) dot = -1.0;
  double omega = std::acos(dot);

  int prev_px = -1, prev_py = -1;
  const int steps = std::max(arc.steps, 2);

  for (int i = 0; i <= steps; ++i) {
    double t = static_cast<double>(i) / steps;

    double ix, iy, iz;
    if (std::abs(omega) < 1e-10) {
      ix = x1; iy = y1; iz = z1;
    } else {
      double s = std::sin(omega);
      ix = (std::sin((1-t)*omega)/s)*x1 + (std::sin(t*omega)/s)*x2;
      iy = (std::sin((1-t)*omega)/s)*y1 + (std::sin(t*omega)/s)*y2;
      iz = (std::sin((1-t)*omega)/s)*z1 + (std::sin(t*omega)/s)*z2;
    }

    double lon = std::atan2(iy, ix) * kRad2Deg;
    double lat = std::asin(std::max(-1.0, std::min(1.0, iz))) * kRad2Deg;

    auto [px, py] = project(lon, lat);

    if (prev_px >= 0) {
      // Skip wrap-around segments (> 180° longitude jump)
      if (std::abs(px - prev_px) < c.width() / 2) {
        c.DrawPointLine(prev_px, prev_py, px, py, arc.color);
      }
    }
    prev_px = px;
    prev_py = py;
  }
}

// ── Canvas rendering ──────────────────────────────────────────────────────────

void GeoMap::Impl::DrawOnto(ftxui::Canvas& c) {
  const int dot_w = c.width();
  const int dot_h = c.height();

  if (collection_.features.empty()) {
    c.DrawText(0, 0, "No GeoJSON data loaded");
    return;
  }

  // Compute visible lon/lat bounds including zoom and pan
  double base_lon_range =
      (collection_.max_lon - collection_.min_lon) * 1.06;
  double base_lat_range =
      (collection_.max_lat - collection_.min_lat) * 1.06;
  if (base_lon_range < 1.0) base_lon_range = 360.0;
  if (base_lat_range < 1.0) base_lat_range = 180.0;

  double lon_range = base_lon_range / zoom_;
  double lat_range = base_lat_range / zoom_;

  double center_lon =
      (collection_.min_lon + collection_.max_lon) / 2.0 + pan_lon_;
  double center_lat =
      (collection_.min_lat + collection_.max_lat) / 2.0 + pan_lat_;

  double lon_min = center_lon - lon_range / 2.0;
  double lon_max = center_lon + lon_range / 2.0;
  double lat_min = center_lat - lat_range / 2.0;
  double lat_max = center_lat + lat_range / 2.0;

  // Clamp to valid geographic range
  if (lon_min < -180.0) lon_min = -180.0;
  if (lon_max > 180.0)  lon_max = 180.0;
  if (lat_min < -90.0)  lat_min = -90.0;
  if (lat_max > 90.0)   lat_max = 90.0;

  // Project helper using current canvas dimensions
  auto project = [&](double lon, double lat) -> std::pair<int,int> {
    return Project(lon, lat, lon_min, lon_max, lat_min, lat_max, dot_w, dot_h);
  };

  // Cache view bounds for mouse coordinate mapping
  cached_lon_min_ = lon_min;
  cached_lon_max_ = lon_max;
  cached_lat_min_ = lat_min;
  cached_lat_max_ = lat_max;
  cached_dot_w_ = dot_w;
  cached_dot_h_ = dot_h;

  // Draw graticule
  if (show_graticule_) {
    const ftxui::Color gray(50, 50, 60);
    float step = graticule_step_;

    for (float lon = std::ceil(lon_min / step) * step; lon <= lon_max; lon += step) {
      auto [x1, y1] = project(lon, lat_min);
      auto [x2, y2] = project(lon, lat_max);
      c.DrawPointLine(x1, y1, x2, y2, gray);
    }
    for (float lat = std::ceil(lat_min / step) * step; lat <= lat_max; lat += step) {
      auto [x1, y1] = project(lon_min, lat);
      auto [x2, y2] = project(lon_max, lat);
      c.DrawPointLine(x1, y1, x2, y2, gray);
    }
  }

  // Draw features
  for (const auto& feat : collection_.features) {
    const auto& geom = feat.geometry;

    if (geom.type == "Point" || geom.type == "MultiPoint") {
      for (const auto& pt : geom.points) {
        auto [x, y] = project(pt.lon, pt.lat);
        c.DrawPoint(x, y, true, point_color_);
      }
    } else if (geom.type == "LineString" || geom.type == "MultiLineString") {
      for (size_t i = 0; i + 1 < geom.points.size(); ++i) {
        auto [x1, y1] = project(geom.points[i].lon, geom.points[i].lat);
        auto [x2, y2] = project(geom.points[i+1].lon, geom.points[i+1].lat);
        c.DrawPointLine(x1, y1, x2, y2, line_color_);
      }
    } else if (geom.type == "Polygon") {
      for (const auto& ring : geom.rings)
        DrawRing(c, ring, project, line_color_);
    } else if (geom.type == "MultiPolygon") {
      for (const auto& poly : geom.multipolygon)
        for (const auto& ring : poly)
          DrawRing(c, ring, project, line_color_);
    }
  }

  // Draw overlays (city markers, custom feature layers)
  for (const auto& ov : overlays_) {
    for (const auto& feat : ov.collection.features) {
      const auto& geom = feat.geometry;
      if (geom.type == "Point" || geom.type == "MultiPoint") {
        for (const auto& pt : geom.points) {
          auto [x, y] = project(pt.lon, pt.lat);
          // Draw a 3-dot cross marker for each point
          c.DrawPoint(x,     y,     true, ov.color);
          c.DrawPoint(x + 1, y,     true, ov.color);
          c.DrawPoint(x - 1, y,     true, ov.color);
          c.DrawPoint(x,     y + 1, true, ov.color);
          c.DrawPoint(x,     y - 1, true, ov.color);
        }
      } else if (geom.type == "LineString" || geom.type == "MultiLineString") {
        for (size_t i = 0; i + 1 < geom.points.size(); ++i) {
          auto [x1, y1] = project(geom.points[i].lon, geom.points[i].lat);
          auto [x2, y2] = project(geom.points[i+1].lon, geom.points[i+1].lat);
          c.DrawPointLine(x1, y1, x2, y2, ov.color);
        }
      } else if (geom.type == "Polygon") {
        for (const auto& ring : geom.rings)
          DrawRing(c, ring, project, ov.color);
      }
    }
  }

  // Draw great-circle arcs (missile trajectories, flight routes)
  for (const auto& arc : arcs_) {
    DrawArc(c, arc, project);
  }

  // Crosshair when zoomed in
  if (zoom_ > 1.5) {
    int cx = dot_w / 2;
    int cy = dot_h / 2;
    c.DrawPointLine(cx - 6, cy, cx + 6, cy, ftxui::Color::White);
    c.DrawPointLine(cx, cy - 6, cx, cy + 6, ftxui::Color::White);
  }
}

// ── Event handling ────────────────────────────────────────────────────────────

bool GeoMap::Impl::HandleEvent(ftxui::Event event) {
  const double pan_step = 10.0 / zoom_;

  if (event == Event::ArrowLeft) {
    pan_lon_ -= pan_step;
    return true;
  }
  if (event == Event::ArrowRight) {
    pan_lon_ += pan_step;
    return true;
  }
  if (event == Event::ArrowUp) {
    pan_lat_ += pan_step;
    return true;
  }
  if (event == Event::ArrowDown) {
    pan_lat_ -= pan_step;
    return true;
  }
  if (event == Event::Character('+') || event == Event::Character('=')) {
    zoom_ *= 1.5;
    return true;
  }
  if (event == Event::Character('-')) {
    zoom_ /= 1.5;
    if (zoom_ < 0.1) zoom_ = 0.1;
    return true;
  }
  if (event == Event::Character('r') || event == Event::Character('R')) {
    zoom_ = 1.0;
    pan_lon_ = 0.0;
    pan_lat_ = 0.0;
    return true;
  }
  if (event == Event::Return && on_select_) {
    double center_lon =
        (collection_.min_lon + collection_.max_lon) / 2.0 + pan_lon_;
    double center_lat =
        (collection_.min_lat + collection_.max_lat) / 2.0 + pan_lat_;

    double best_dist = std::numeric_limits<double>::max();
    const GeoFeature* best = nullptr;

    for (const auto& f : collection_.features) {
      double clon = 0.0;
      double clat = 0.0;
      int cnt = 0;

      auto acc = [&](const GeoPoint& p) {
        clon += p.lon;
        clat += p.lat;
        ++cnt;
      };
      for (const auto& pt : f.geometry.points) acc(pt);
      for (const auto& ring : f.geometry.rings) {
        for (const auto& pt : ring) acc(pt);
      }
      for (const auto& poly : f.geometry.multipolygon) {
        for (const auto& ring : poly) {
          for (const auto& pt : ring) acc(pt);
        }
      }

      if (cnt == 0) continue;
      clon /= cnt;
      clat /= cnt;
      double d = (clon - center_lon) * (clon - center_lon) +
                 (clat - center_lat) * (clat - center_lat);
      if (d < best_dist) {
        best_dist = d;
        best = &f;
      }
    }

    if (best) on_select_(*best);
    return true;
  }

  // ── Mouse events ──────────────────────────────────────────────────────────
  if (event.is_mouse()) {
    auto& mouse = event.mouse();

    // Only handle events within the canvas box.
    if (!canvas_box_.Contain(mouse.x, mouse.y)) {
      return false;
    }

    if (mouse.button == Mouse::WheelUp || mouse.button == Mouse::WheelDown) {
      // Zoom centred on the cursor position.
      const double dw = cached_dot_w_ > 1 ? cached_dot_w_ - 1 : 1;
      const double dh = cached_dot_h_ > 1 ? cached_dot_h_ - 1 : 1;
      const double lon_range = cached_lon_max_ - cached_lon_min_;
      const double lat_range = cached_lat_max_ - cached_lat_min_;

      // Canvas dot coordinates of the mouse cursor.
      const double dot_x =
          static_cast<double>(mouse.x - canvas_box_.x_min) * 2.0;
      const double dot_y =
          static_cast<double>(mouse.y - canvas_box_.y_min) * 4.0;

      // Geographic position under the cursor in the current view.
      const double mouse_lon =
          cached_lon_min_ + (dot_x / dw) * lon_range;
      const double mouse_lat =
          cached_lat_max_ - (dot_y / dh) * lat_range;

      const double factor = (mouse.button == Mouse::WheelUp) ? 1.25 : (1.0 / 1.25);
      zoom_ *= factor;
      if (zoom_ < 0.1) zoom_ = 0.1;

      // Recompute the base range (without zoom) to find new range.
      double base_lon_range =
          (collection_.max_lon - collection_.min_lon) * 1.06;
      double base_lat_range =
          (collection_.max_lat - collection_.min_lat) * 1.06;
      if (base_lon_range < 1.0) base_lon_range = 360.0;
      if (base_lat_range < 1.0) base_lat_range = 180.0;

      const double new_lon_range = base_lon_range / zoom_;
      const double new_lat_range = base_lat_range / zoom_;

      // Adjust pan so the point under the cursor stays fixed.
      const double frac_x = dw > 0 ? dot_x / dw : 0.5;
      const double frac_y = dh > 0 ? dot_y / dh : 0.5;

      const double new_center_lon =
          mouse_lon - (frac_x - 0.5) * new_lon_range;
      const double new_center_lat =
          mouse_lat + (frac_y - 0.5) * new_lat_range;

      const double data_center_lon =
          (collection_.min_lon + collection_.max_lon) / 2.0;
      const double data_center_lat =
          (collection_.min_lat + collection_.max_lat) / 2.0;

      pan_lon_ = new_center_lon - data_center_lon;
      pan_lat_ = new_center_lat - data_center_lat;
      return true;
    }

    if (mouse.button == Mouse::Left && mouse.motion == Mouse::Pressed) {
      // Detect double-click (within 400ms and ≤2 cell radius).
      const auto now = Clock::now();
      const auto dt =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              now - last_click_time_)
              .count();
      const bool same_pos = std::abs(mouse.x - last_click_x_) <= 2 &&
                             std::abs(mouse.y - last_click_y_) <= 2;
      if (dt < 400 && same_pos && on_select_) {
        // Double-click: select nearest feature at click position.
        const double dw = cached_dot_w_ > 1 ? cached_dot_w_ - 1 : 1;
        const double dh = cached_dot_h_ > 1 ? cached_dot_h_ - 1 : 1;
        const double lon_range = cached_lon_max_ - cached_lon_min_;
        const double lat_range = cached_lat_max_ - cached_lat_min_;
        const double dot_x =
            static_cast<double>(mouse.x - canvas_box_.x_min) * 2.0;
        const double dot_y =
            static_cast<double>(mouse.y - canvas_box_.y_min) * 4.0;
        const double click_lon =
            cached_lon_min_ + (dot_x / dw) * lon_range;
        const double click_lat =
            cached_lat_max_ - (dot_y / dh) * lat_range;

        double best_dist = std::numeric_limits<double>::max();
        const GeoFeature* best = nullptr;
        for (const auto& f : collection_.features) {
          double clon = 0.0, clat = 0.0;
          int cnt = 0;
          auto acc = [&](const GeoPoint& p) {
            clon += p.lon; clat += p.lat; ++cnt;
          };
          for (const auto& pt : f.geometry.points) acc(pt);
          for (const auto& ring : f.geometry.rings)
            for (const auto& pt : ring) acc(pt);
          for (const auto& poly : f.geometry.multipolygon)
            for (const auto& ring : poly)
              for (const auto& pt : ring) acc(pt);
          if (cnt == 0) continue;
          clon /= cnt; clat /= cnt;
          double d = (clon - click_lon) * (clon - click_lon) +
                     (clat - click_lat) * (clat - click_lat);
          if (d < best_dist) { best_dist = d; best = &f; }
        }
        if (best) on_select_(*best);
        last_click_time_ = Clock::time_point{};  // reset
        return true;
      }

      // Begin drag.
      last_click_time_ = now;
      last_click_x_ = mouse.x;
      last_click_y_ = mouse.y;
      dragging_ = true;
      drag_start_x_ = mouse.x;
      drag_start_y_ = mouse.y;
      drag_start_pan_lon_ = pan_lon_;
      drag_start_pan_lat_ = pan_lat_;
      return true;
    }

    if (mouse.button == Mouse::Left && mouse.motion == Mouse::Released) {
      dragging_ = false;
      return true;
    }

    if (mouse.motion == Mouse::Moved && dragging_) {
      const double dw = cached_dot_w_ > 1 ? cached_dot_w_ - 1 : 1;
      const double dh = cached_dot_h_ > 1 ? cached_dot_h_ - 1 : 1;
      const double lon_range = cached_lon_max_ - cached_lon_min_;
      const double lat_range = cached_lat_max_ - cached_lat_min_;

      // Delta in braille dots.
      const double ddx =
          static_cast<double>(mouse.x - drag_start_x_) * 2.0;
      const double ddy =
          static_cast<double>(mouse.y - drag_start_y_) * 4.0;

      pan_lon_ = drag_start_pan_lon_ - (ddx / dw) * lon_range;
      pan_lat_ = drag_start_pan_lat_ + (ddy / dh) * lat_range;
      return true;
    }
  }

  return false;
}

// ── GeoMap builder ────────────────────────────────────────────────────────────

GeoMap::GeoMap() : impl_(std::make_shared<Impl>()) {}

GeoMap& GeoMap::Data(const GeoCollection& col) {
  impl_->collection_ = col;
  return *this;
}

GeoMap& GeoMap::Data(std::string_view geojson_str) {
  impl_->collection_ = ParseGeoJSON(geojson_str);
  return *this;
}

GeoMap& GeoMap::LoadFile(std::string_view path) {
  impl_->collection_ = LoadGeoJSON(path);
  return *this;
}

GeoMap& GeoMap::SetProjection(Projection p) {
  impl_->projection_ = p;
  return *this;
}

GeoMap& GeoMap::PointColor(ftxui::Color c) {
  impl_->point_color_ = c;
  return *this;
}

GeoMap& GeoMap::LineColor(ftxui::Color c) {
  impl_->line_color_ = c;
  return *this;
}

GeoMap& GeoMap::FillColor(ftxui::Color c) {
  impl_->fill_color_ = c;
  return *this;
}

GeoMap& GeoMap::ShowGraticule(bool v) {
  impl_->show_graticule_ = v;
  return *this;
}

GeoMap& GeoMap::GraticuleStep(float degrees) {
  impl_->graticule_step_ = degrees;
  return *this;
}

GeoMap& GeoMap::OnSelect(std::function<void(const GeoFeature&)> fn) {
  impl_->on_select_ = std::move(fn);
  return *this;
}

GeoMap& GeoMap::Overlay(const GeoCollection& col, ftxui::Color color) {
  impl_->overlays_.push_back(MapOverlay{col, color});
  return *this;
}

GeoMap& GeoMap::AddArc(double lon1, double lat1, double lon2, double lat2,
                        ftxui::Color color, int steps) {
  impl_->arcs_.push_back({lon1, lat1, lon2, lat2, color, steps});
  return *this;
}

ftxui::Component GeoMap::Build() {
  auto impl = impl_;

  // canvas(2, 4, fn) sets a 1-char minimum size.
  // FTXUI's CanvasFunctionNode::Render() creates the Canvas at the ACTUAL
  // allocated box size (box_w*2 dots × box_h*4 dots), so the map always
  // fills every available braille dot at maximum resolution.
  auto renderer = Renderer([impl]() -> Element {
    return canvas(2, 4, [impl](Canvas& c) { impl->DrawOnto(c); })
           | flex
           | reflect(impl->canvas_box_);
  });

  return CatchEvent(renderer, [impl](Event event) -> bool {
    return impl->HandleEvent(event);
  });
}

}  // namespace ftxui::ui
