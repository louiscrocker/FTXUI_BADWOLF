// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_SPATIAL_HPP
#define FTXUI_UI_SPATIAL_HPP

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

// ── Vec3
// ──────────────────────────────────────────────────────────────────────
struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 cross(Vec3 o) const;
  float dot(Vec3 o) const { return x * o.x + y * o.y + z * o.z; }
  float length() const;
  Vec3 normalized() const;
  static Vec3 Zero() { return {}; }
  static Vec3 Up() { return {0, 1, 0}; }
  static Vec3 Forward() { return {0, 0, -1}; }
};

// ── Mat4
// ────────────────────────────────────────────────────────────────────── 4x4
// transformation matrix (column-major, OpenGL convention)
struct Mat4 {
  float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};  // identity
  static Mat4 Identity();
  static Mat4 Translation(Vec3 t);
  static Mat4 Scale(Vec3 s);
  static Mat4 RotationX(float radians);
  static Mat4 RotationY(float radians);
  static Mat4 RotationZ(float radians);
  static Mat4 Perspective(float fov_y,
                          float aspect,
                          float near_plane,
                          float far_plane);
  static Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up);
  Mat4 operator*(const Mat4& o) const;
  Vec3 Transform(
      Vec3 v) const;  // applies 4x4 to homogeneous vec4, divides by w
};

// ── Camera3D
// ──────────────────────────────────────────────────────────────────
struct Camera3D {
  Vec3 position = {0, 0, 5};
  Vec3 target = {0, 0, 0};
  Vec3 up = {0, 1, 0};
  float fov = 60.0f;  // degrees
  float near_plane = 0.1f;
  float far_plane = 1000.0f;

  Mat4 ViewMatrix() const;
  Mat4 ProjectionMatrix(float aspect) const;
  Mat4 ViewProjection(float aspect) const;

  void OrbitAround(Vec3 center, float delta_yaw, float delta_pitch);
  void Zoom(float delta);
  void Pan(Vec3 delta);
};

// ── SpatialWidget
// ─────────────────────────────────────────────────────────────
struct SpatialWidget {
  int id = 0;
  Vec3 position;
  Vec3 rotation = Vec3::Zero();  // Euler angles in radians
  Vec3 scale = {1, 1, 1};
  float opacity = 1.0f;

  ftxui::Component content;
  int content_width = 20;
  int content_height = 10;

  bool focusable = true;
  bool visible = true;
  std::string label;

  // Computed by SpatialScene::Render
  mutable float screen_x = 0;
  mutable float screen_y = 0;
  mutable float depth = 0;
  mutable float screen_scale = 1.0f;
};

// ── SpatialScene
// ──────────────────────────────────────────────────────────────
class SpatialScene {
 public:
  explicit SpatialScene(Camera3D camera = {});
  ~SpatialScene();

  int AddWidget(SpatialWidget w);
  void RemoveWidget(int id);
  SpatialWidget* GetWidget(int id);
  void UpdateWidget(int id, std::function<void(SpatialWidget&)> fn);

  void SetCamera(Camera3D cam);
  Camera3D& camera();
  const Camera3D& camera() const;

  int WidgetCount() const;
  std::vector<int> WidgetIds() const;

  ftxui::Element Render(int canvas_cols, int canvas_rows) const;

  struct Projected {
    float x, y, depth;
    bool visible;
  };
  Projected Project(Vec3 world_pos, int canvas_cols, int canvas_rows) const;

  ftxui::Component BuildComponent();

  void StartOrbit(float deg_per_second = 30.0f);
  void StopOrbit();

  // Current orbit angle (radians); updated by background thread
  float OrbitAngle() const;

 private:
  Camera3D camera_;
  std::vector<SpatialWidget> widgets_;
  int next_id_{0};
  std::atomic<bool> orbiting_{false};
  float orbit_speed_{0.0f};
  float orbit_angle_{0.0f};
  std::thread orbit_thread_;

  void SortByDepth(std::vector<int>& indices) const;
};

// ── SpatialLayouts
// ────────────────────────────────────────────────────────────
namespace SpatialLayouts {
void Circle(SpatialScene& scene, float radius = 3.0f, float y = 0);
void Grid(SpatialScene& scene, int cols = 3, float spacing = 2.5f);
void Sphere(SpatialScene& scene, float radius = 4.0f);
void Stack(SpatialScene& scene, float y_step = 1.5f);
void Fan(SpatialScene& scene,
         float arc_degrees = 120.0f,
         float distance = 3.0f);
}  // namespace SpatialLayouts

// ── HUD
// ───────────────────────────────────────────────────────────────────────
class HUD {
 public:
  HUD& AddCorner(ftxui::Element el, int corner);  // 0=TL, 1=TR, 2=BL, 3=BR
  HUD& AddCrosshair(bool show = true);
  HUD& AddCompass(bool show = true);

  ftxui::Element Render(ftxui::Element scene_element) const;

 private:
  ftxui::Element corners_[4];
  bool show_crosshair_ = false;
  bool show_compass_ = false;
};

// ── Free functions
// ────────────────────────────────────────────────────────────
ftxui::Component SpatialDock(std::vector<ftxui::Component> widgets,
                             Camera3D camera = {});

ftxui::Component FloatingWindow(ftxui::Component content,
                                Vec3 position = {0, 0, 0},
                                const std::string& title = "");

}  // namespace ftxui::ui

#endif  // FTXUI_UI_SPATIAL_HPP
