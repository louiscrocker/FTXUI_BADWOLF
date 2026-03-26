// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/spatial.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
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
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/screen/screen.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Vec3
// ──────────────────────────────────────────────────────────────────────

Vec3 Vec3::cross(Vec3 o) const {
  return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
}

float Vec3::length() const {
  return std::sqrt(x * x + y * y + z * z);
}

Vec3 Vec3::normalized() const {
  float len = length();
  if (len < 1e-7f) {
    return {0, 0, 0};
  }
  return {x / len, y / len, z / len};
}

// ── Mat4
// ──────────────────────────────────────────────────────────────────────

Mat4 Mat4::Identity() {
  return Mat4{};
}

Mat4 Mat4::Translation(Vec3 t) {
  Mat4 r;
  r.m[12] = t.x;
  r.m[13] = t.y;
  r.m[14] = t.z;
  return r;
}

Mat4 Mat4::Scale(Vec3 s) {
  Mat4 r;
  r.m[0] = s.x;
  r.m[5] = s.y;
  r.m[10] = s.z;
  r.m[15] = 1;
  return r;
}

Mat4 Mat4::RotationX(float radians) {
  Mat4 r;
  float c = std::cos(radians);
  float s = std::sin(radians);
  // Column-major: index = col*4 + row
  r.m[0] = 1;
  r.m[5] = c;
  r.m[6] = s;
  r.m[9] = -s;
  r.m[10] = c;
  r.m[15] = 1;
  // Zero out non-identity non-set
  r.m[1] = 0;
  r.m[2] = 0;
  r.m[3] = 0;
  r.m[4] = 0;
  r.m[7] = 0;
  r.m[8] = 0;
  r.m[11] = 0;
  r.m[12] = 0;
  r.m[13] = 0;
  r.m[14] = 0;
  return r;
}

Mat4 Mat4::RotationY(float radians) {
  Mat4 r;
  float c = std::cos(radians);
  float s = std::sin(radians);
  r.m[0] = c;
  r.m[2] = -s;
  r.m[5] = 1;
  r.m[8] = s;
  r.m[10] = c;
  r.m[15] = 1;
  r.m[1] = 0;
  r.m[3] = 0;
  r.m[4] = 0;
  r.m[6] = 0;
  r.m[7] = 0;
  r.m[9] = 0;
  r.m[11] = 0;
  r.m[12] = 0;
  r.m[13] = 0;
  r.m[14] = 0;
  return r;
}

Mat4 Mat4::RotationZ(float radians) {
  Mat4 r;
  float c = std::cos(radians);
  float s = std::sin(radians);
  r.m[0] = c;
  r.m[1] = s;
  r.m[4] = -s;
  r.m[5] = c;
  r.m[10] = 1;
  r.m[15] = 1;
  r.m[2] = 0;
  r.m[3] = 0;
  r.m[6] = 0;
  r.m[7] = 0;
  r.m[8] = 0;
  r.m[9] = 0;
  r.m[11] = 0;
  r.m[12] = 0;
  r.m[13] = 0;
  r.m[14] = 0;
  return r;
}

Mat4 Mat4::Perspective(float fov_y,
                       float aspect,
                       float near_plane,
                       float far_plane) {
  Mat4 r;
  // Zero out
  for (float& v : r.m) {
    v = 0;
  }
  float f = 1.0f / std::tan(fov_y * 0.5f);
  r.m[0] = f / aspect;
  r.m[5] = f;
  r.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
  r.m[11] = -1.0f;
  r.m[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
  r.m[15] = 0.0f;
  return r;
}

Mat4 Mat4::LookAt(Vec3 eye, Vec3 target, Vec3 up) {
  Vec3 n = (eye - target).normalized();  // z-axis (backward)
  Vec3 u = up.cross(n).normalized();     // x-axis (right)
  Vec3 v = n.cross(u);                   // y-axis (new up)

  Mat4 r;
  // Column-major: m[col*4 + row]
  // col 0
  r.m[0] = u.x;
  r.m[1] = v.x;
  r.m[2] = n.x;
  r.m[3] = 0;
  // col 1
  r.m[4] = u.y;
  r.m[5] = v.y;
  r.m[6] = n.y;
  r.m[7] = 0;
  // col 2
  r.m[8] = u.z;
  r.m[9] = v.z;
  r.m[10] = n.z;
  r.m[11] = 0;
  // col 3
  r.m[12] = -u.dot(eye);
  r.m[13] = -v.dot(eye);
  r.m[14] = -n.dot(eye);
  r.m[15] = 1;
  return r;
}

Mat4 Mat4::operator*(const Mat4& o) const {
  Mat4 res;
  for (float& v : res.m) {
    v = 0;
  }
  // result[c*4+r] = sum_k A[k*4+r] * B[c*4+k]
  for (int c = 0; c < 4; c++) {
    for (int row = 0; row < 4; row++) {
      float sum = 0;
      for (int k = 0; k < 4; k++) {
        sum += m[k * 4 + row] * o.m[c * 4 + k];
      }
      res.m[c * 4 + row] = sum;
    }
  }
  return res;
}

Vec3 Mat4::Transform(Vec3 v) const {
  float vx = v.x, vy = v.y, vz = v.z, vw = 1.0f;
  float rx = m[0] * vx + m[4] * vy + m[8] * vz + m[12] * vw;
  float ry = m[1] * vx + m[5] * vy + m[9] * vz + m[13] * vw;
  float rz = m[2] * vx + m[6] * vy + m[10] * vz + m[14] * vw;
  float rw = m[3] * vx + m[7] * vy + m[11] * vz + m[15] * vw;
  if (std::abs(rw) < 1e-7f) {
    return {rx, ry, rz};
  }
  return {rx / rw, ry / rw, rz / rw};
}

// ── Camera3D
// ──────────────────────────────────────────────────────────────────

Mat4 Camera3D::ViewMatrix() const {
  return Mat4::LookAt(position, target, up);
}

Mat4 Camera3D::ProjectionMatrix(float aspect) const {
  float fov_rad = fov * float(M_PI) / 180.0f;
  return Mat4::Perspective(fov_rad, aspect, near_plane, far_plane);
}

Mat4 Camera3D::ViewProjection(float aspect) const {
  return ProjectionMatrix(aspect) * ViewMatrix();
}

void Camera3D::OrbitAround(Vec3 center, float delta_yaw, float delta_pitch) {
  Vec3 offset = position - center;
  float dist = offset.length();
  if (dist < 1e-5f) {
    return;
  }

  // Current angles
  float yaw = std::atan2(offset.x, offset.z);
  float pitch = std::asin(offset.y / dist);

  yaw += delta_yaw;
  pitch += delta_pitch;

  // Clamp pitch to avoid gimbal lock
  static constexpr float kMaxPitch = float(M_PI) * 0.48f;
  if (pitch > kMaxPitch) {
    pitch = kMaxPitch;
  }
  if (pitch < -kMaxPitch) {
    pitch = -kMaxPitch;
  }

  position = center + Vec3{dist * std::sin(yaw) * std::cos(pitch),
                           dist * std::sin(pitch),
                           dist * std::cos(yaw) * std::cos(pitch)};
  target = center;
}

void Camera3D::Zoom(float delta) {
  Vec3 dir = (target - position).normalized();
  position = position + dir * delta;
}

void Camera3D::Pan(Vec3 delta) {
  position = position + delta;
  target = target + delta;
}

// ── SpatialScene
// ──────────────────────────────────────────────────────────────

SpatialScene::SpatialScene(Camera3D camera) : camera_(camera) {}

SpatialScene::~SpatialScene() {
  StopOrbit();
}

int SpatialScene::AddWidget(SpatialWidget w) {
  w.id = next_id_++;
  widgets_.push_back(std::move(w));
  return widgets_.back().id;
}

void SpatialScene::RemoveWidget(int id) {
  widgets_.erase(
      std::remove_if(widgets_.begin(), widgets_.end(),
                     [id](const SpatialWidget& w) { return w.id == id; }),
      widgets_.end());
}

SpatialWidget* SpatialScene::GetWidget(int id) {
  for (auto& w : widgets_) {
    if (w.id == id) {
      return &w;
    }
  }
  return nullptr;
}

void SpatialScene::UpdateWidget(int id,
                                std::function<void(SpatialWidget&)> fn) {
  for (auto& w : widgets_) {
    if (w.id == id) {
      fn(w);
      return;
    }
  }
}

void SpatialScene::SetCamera(Camera3D cam) {
  camera_ = cam;
}

Camera3D& SpatialScene::camera() {
  return camera_;
}

const Camera3D& SpatialScene::camera() const {
  return camera_;
}

int SpatialScene::WidgetCount() const {
  return int(widgets_.size());
}

std::vector<int> SpatialScene::WidgetIds() const {
  std::vector<int> ids;
  ids.reserve(widgets_.size());
  for (const auto& w : widgets_) {
    ids.push_back(w.id);
  }
  return ids;
}

void SpatialScene::SortByDepth(std::vector<int>& indices) const {
  std::sort(indices.begin(), indices.end(), [this](int a, int b) {
    // Find widgets by index
    float da = 0, db = 0;
    for (const auto& w : widgets_) {
      if (w.id == a) {
        da = w.depth;
      }
      if (w.id == b) {
        db = w.depth;
      }
    }
    return da > db;  // farther first (painter's algorithm)
  });
}

SpatialScene::Projected SpatialScene::Project(Vec3 world_pos,
                                              int canvas_cols,
                                              int canvas_rows) const {
  float aspect = float(canvas_cols) / float(canvas_rows != 0 ? canvas_rows : 1);
  Mat4 view = camera_.ViewMatrix();
  Vec3 view_pos = view.Transform(world_pos);
  float depth_val = -view_pos.z;

  Mat4 mvp = camera_.ViewProjection(aspect);
  Vec3 ndc = mvp.Transform(world_pos);

  float sx = (ndc.x + 1.0f) * 0.5f * float(canvas_cols);
  float sy = (1.0f - ndc.y) * 0.5f * float(canvas_rows);
  bool vis = depth_val > camera_.near_plane;
  return {sx, sy, depth_val, vis};
}

ftxui::Element SpatialScene::Render(int canvas_cols, int canvas_rows) const {
  // Capture a snapshot of widget data for the lambda
  struct WidgetSnapshot {
    Vec3 position;
    std::string label;
    ftxui::Component content;
    int content_width;
    int content_height;
    bool visible;
  };

  std::vector<WidgetSnapshot> snapshots;
  snapshots.reserve(widgets_.size());
  for (const auto& w : widgets_) {
    snapshots.push_back({w.position, w.label, w.content, w.content_width,
                         w.content_height, w.visible});
  }

  Camera3D cam = camera_;
  float orbit = orbit_angle_;

  return ftxui::canvas(
      canvas_cols * 2, canvas_rows * 4,
      [snapshots, cam, orbit, canvas_cols, canvas_rows](ftxui::Canvas& c) {
        float aspect =
            float(canvas_cols) / float(canvas_rows != 0 ? canvas_rows : 1);

        // Build sorted indices by depth
        struct Entry {
          int idx;
          float depth;
        };
        std::vector<Entry> entries;
        entries.reserve(snapshots.size());

        for (int i = 0; i < int(snapshots.size()); i++) {
          const auto& snap = snapshots[i];
          if (!snap.visible) {
            continue;
          }

          // Apply orbit rotation around Y axis
          Vec3 pos = snap.position;
          float ox = pos.x * std::cos(orbit) - pos.z * std::sin(orbit);
          float oz = pos.x * std::sin(orbit) + pos.z * std::cos(orbit);
          pos.x = ox;
          pos.z = oz;

          Mat4 view = cam.ViewMatrix();
          Vec3 view_pos = view.Transform(pos);
          float depth_val = -view_pos.z;
          entries.push_back({i, depth_val});
        }

        // Sort farther first (painter's algorithm)
        std::sort(
            entries.begin(), entries.end(),
            [](const Entry& a, const Entry& b) { return a.depth > b.depth; });

        for (const auto& entry : entries) {
          const auto& snap = snapshots[entry.idx];
          float depth_val = entry.depth;

          // Apply orbit rotation again for projection
          Vec3 pos = snap.position;
          float ox = pos.x * std::cos(orbit) - pos.z * std::sin(orbit);
          float oz = pos.x * std::sin(orbit) + pos.z * std::cos(orbit);
          pos.x = ox;
          pos.z = oz;

          Mat4 mvp = cam.ViewProjection(aspect);
          Vec3 ndc = mvp.Transform(pos);

          if (depth_val <= cam.near_plane) {
            continue;
          }

          float sx = (ndc.x + 1.0f) * 0.5f * float(canvas_cols);
          float sy = (1.0f - ndc.y) * 0.5f * float(canvas_rows);

          // Clamp scale
          float scl = std::max(0.3f, std::min(3.0f, 2.0f / depth_val * 4.0f));

          int disp_w = std::max(4, std::min(30, int(snap.content_width * scl)));
          int disp_h =
              std::max(2, std::min(20, int(snap.content_height * scl)));
          int x0 = int(sx) - disp_w / 2;
          int y0 = int(sy) - disp_h / 2;

          int dx0 = x0 * 2;
          int dy0 = y0 * 4;

          if (snap.content) {
            auto screen =
                ftxui::Screen::Create(ftxui::Dimension::Fixed(disp_w),
                                      ftxui::Dimension::Fixed(disp_h));
            ftxui::Render(screen, snap.content->Render());
            for (int row = 0; row < disp_h; row++) {
              for (int col = 0; col < disp_w; col++) {
                int cx = dx0 + col * 2;
                int cy = dy0 + row * 4;
                if (cx >= 0 && cy >= 0 && cx < canvas_cols * 2 &&
                    cy < canvas_rows * 4) {
                  const std::string& ch = screen.at(col, row);
                  c.DrawText(cx, cy, ch);
                }
              }
            }
          } else {
            int lx = std::max(0, dx0);
            int ly = std::max(0, dy0);
            if (!snap.label.empty() && lx < canvas_cols * 2 &&
                ly < canvas_rows * 4) {
              c.DrawText(lx, ly, snap.label, ftxui::Color::Cyan);
            }
          }

          // Draw bounding box
          c.DrawPointLine(dx0, dy0, dx0 + disp_w * 2, dy0, ftxui::Color::Blue);
          c.DrawPointLine(dx0, dy0 + disp_h * 4, dx0 + disp_w * 2,
                          dy0 + disp_h * 4, ftxui::Color::Blue);
          c.DrawPointLine(dx0, dy0, dx0, dy0 + disp_h * 4, ftxui::Color::Blue);
          c.DrawPointLine(dx0 + disp_w * 2, dy0, dx0 + disp_w * 2,
                          dy0 + disp_h * 4, ftxui::Color::Blue);
        }
      });
}

ftxui::Component SpatialScene::BuildComponent() {
  struct MouseState {
    bool dragging = false;
    int last_x = 0;
    int last_y = 0;
  };
  auto mouse_state = std::make_shared<MouseState>();

  auto renderer = ftxui::Renderer([this] { return Render(80, 24); });

  return ftxui::CatchEvent(renderer, [this, mouse_state](ftxui::Event e) {
    if (e.is_mouse()) {
      auto& m = e.mouse();
      if (m.button == ftxui::Mouse::Left) {
        if (m.motion == ftxui::Mouse::Pressed) {
          mouse_state->dragging = true;
          mouse_state->last_x = m.x;
          mouse_state->last_y = m.y;
        } else if (m.motion == ftxui::Mouse::Released) {
          mouse_state->dragging = false;
        } else if (m.motion == ftxui::Mouse::Moved && mouse_state->dragging) {
          float dyaw = (m.x - mouse_state->last_x) * 0.05f;
          float dpitch = (m.y - mouse_state->last_y) * 0.05f;
          camera_.OrbitAround(camera_.target, dyaw, dpitch);
          mouse_state->last_x = m.x;
          mouse_state->last_y = m.y;
          return true;
        }
      }
      if (m.button == ftxui::Mouse::WheelUp) {
        camera_.Zoom(-0.5f);
        return true;
      }
      if (m.button == ftxui::Mouse::WheelDown) {
        camera_.Zoom(0.5f);
        return true;
      }
    }
    return false;
  });
}

void SpatialScene::StartOrbit(float deg_per_second) {
  orbit_speed_ = deg_per_second * float(M_PI) / 180.0f;
  if (orbiting_.load()) {
    return;
  }
  orbiting_.store(true);
  orbit_thread_ = std::thread([this]() {
    while (orbiting_.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      if (!orbiting_.load()) {
        break;
      }
      orbit_angle_ += orbit_speed_ * 0.05f;
      if (orbit_angle_ > float(2 * M_PI)) {
        orbit_angle_ -= float(2 * M_PI);
      }
      if (App* a = App::Active()) {
        a->Post([] {});
      }
    }
  });
}

void SpatialScene::StopOrbit() {
  orbiting_.store(false);
  if (orbit_thread_.joinable()) {
    orbit_thread_.join();
  }
}

float SpatialScene::OrbitAngle() const {
  return orbit_angle_;
}

// ── SpatialLayouts
// ────────────────────────────────────────────────────────────

namespace SpatialLayouts {

void Circle(SpatialScene& scene, float radius, float y) {
  auto ids = scene.WidgetIds();
  int n = int(ids.size());
  for (int i = 0; i < n; i++) {
    float angle = float(i) * float(2 * M_PI) / float(n);
    scene.UpdateWidget(ids[i], [&](SpatialWidget& w) {
      w.position = {radius * std::cos(angle), y, radius * std::sin(angle)};
      w.rotation.y = -angle + float(M_PI);
    });
  }
}

void Grid(SpatialScene& scene, int cols, float spacing) {
  auto ids = scene.WidgetIds();
  int n = int(ids.size());
  for (int i = 0; i < n; i++) {
    int col = i % cols;
    int row = i / cols;
    scene.UpdateWidget(ids[i], [&](SpatialWidget& w) {
      w.position = {(col - cols / 2.0f) * spacing, 0, float(row) * spacing};
    });
  }
}

void Sphere(SpatialScene& scene, float radius) {
  auto ids = scene.WidgetIds();
  int n = int(ids.size());
  for (int i = 0; i < n; i++) {
    float phi = std::acos(1.0f - 2.0f * (float(i) + 0.5f) / float(n));
    float theta = float(M_PI) * (1.0f + std::sqrt(5.0f)) * float(i);
    scene.UpdateWidget(ids[i], [&](SpatialWidget& w) {
      w.position = {radius * std::sin(phi) * std::cos(theta),
                    radius * std::cos(phi),
                    radius * std::sin(phi) * std::sin(theta)};
    });
  }
}

void Stack(SpatialScene& scene, float y_step) {
  auto ids = scene.WidgetIds();
  for (int i = 0; i < int(ids.size()); i++) {
    scene.UpdateWidget(
        ids[i], [&](SpatialWidget& w) { w.position.y = float(i) * y_step; });
  }
}

void Fan(SpatialScene& scene, float arc_degrees, float distance) {
  auto ids = scene.WidgetIds();
  int n = int(ids.size());
  float arc_rad = arc_degrees * float(M_PI) / 180.0f;
  for (int i = 0; i < n; i++) {
    float t = n > 1 ? float(i) / float(n - 1) : 0.5f;
    float angle = -arc_rad / 2.0f + t * arc_rad;
    scene.UpdateWidget(ids[i], [&](SpatialWidget& w) {
      w.position = {distance * std::sin(angle), 0, -distance * std::cos(angle)};
      w.rotation.y = -angle;
    });
  }
}

}  // namespace SpatialLayouts

// ── HUD
// ───────────────────────────────────────────────────────────────────────

HUD& HUD::AddCorner(ftxui::Element el, int corner) {
  if (corner >= 0 && corner < 4) {
    corners_[corner] = std::move(el);
  }
  return *this;
}

HUD& HUD::AddCrosshair(bool show) {
  show_crosshair_ = show;
  return *this;
}

HUD& HUD::AddCompass(bool show) {
  show_compass_ = show;
  return *this;
}

ftxui::Element HUD::Render(ftxui::Element scene_element) const {
  using namespace ftxui;

  auto tl = corners_[0] ? corners_[0] : text("");
  auto tr = corners_[1] ? corners_[1] : text("");
  auto bl = corners_[2] ? corners_[2] : text("");
  auto br = corners_[3] ? corners_[3] : text("");

  Elements bottom_row = {bl, filler(), br};
  if (show_compass_) {
    bottom_row = {
        bl, filler(),
        text("N\xe2\x86\x91 E\xe2\x86\x92 S\xe2\x86\x93 W\xe2\x86\x90") |
            color(Color::Yellow),
        filler(), br};
  }

  auto overlay = vbox({
      hbox({tl, filler(), tr}),
      filler(),
      hbox(std::move(bottom_row)),
  });

  if (show_crosshair_) {
    overlay = dbox({
        overlay,
        vbox({filler(), hbox({filler(), text("+"), filler()}), filler()}),
    });
  }

  return dbox({std::move(scene_element), overlay});
}

// ── Free functions
// ────────────────────────────────────────────────────────────

ftxui::Component SpatialDock(std::vector<ftxui::Component> widgets,
                             Camera3D camera) {
  auto scene = std::make_shared<SpatialScene>(camera);
  for (auto& w : widgets) {
    SpatialWidget sw;
    sw.content = w;
    sw.label = "Widget";
    scene->AddWidget(std::move(sw));
  }
  SpatialLayouts::Circle(*scene);
  scene->StartOrbit();
  return scene->BuildComponent();
}

ftxui::Component FloatingWindow(ftxui::Component content,
                                Vec3 position,
                                const std::string& title) {
  auto scene = std::make_shared<SpatialScene>();
  SpatialWidget w;
  w.content = content;
  w.position = position;
  w.label = title;
  scene->AddWidget(std::move(w));
  return scene->BuildComponent();
}

}  // namespace ftxui::ui
