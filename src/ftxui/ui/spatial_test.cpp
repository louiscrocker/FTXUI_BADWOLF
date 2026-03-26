// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "ftxui/ui/spatial.hpp"

#include <cmath>
#include <memory>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "gtest/gtest.h"

using namespace ftxui;
using namespace ftxui::ui;

namespace {

// ── Vec3 tests ───────────────────────────────────────────────────────────────

TEST(Vec3Test, Addition) {
  Vec3 a{1, 2, 3};
  Vec3 b{4, 5, 6};
  Vec3 c = a + b;
  EXPECT_FLOAT_EQ(c.x, 5);
  EXPECT_FLOAT_EQ(c.y, 7);
  EXPECT_FLOAT_EQ(c.z, 9);
}

TEST(Vec3Test, CrossProduct) {
  Vec3 a{1, 0, 0};
  Vec3 b{0, 1, 0};
  Vec3 c = a.cross(b);
  EXPECT_NEAR(c.x, 0, 1e-5f);
  EXPECT_NEAR(c.y, 0, 1e-5f);
  EXPECT_NEAR(c.z, 1, 1e-5f);
}

TEST(Vec3Test, CrossProductAnticommutative) {
  Vec3 a{1, 2, 3};
  Vec3 b{4, 5, 6};
  Vec3 axb = a.cross(b);
  Vec3 bxa = b.cross(a);
  EXPECT_NEAR(axb.x, -bxa.x, 1e-5f);
  EXPECT_NEAR(axb.y, -bxa.y, 1e-5f);
  EXPECT_NEAR(axb.z, -bxa.z, 1e-5f);
}

TEST(Vec3Test, Normalized) {
  Vec3 a{3, 4, 0};
  Vec3 n = a.normalized();
  EXPECT_NEAR(n.length(), 1.0f, 1e-5f);
  EXPECT_NEAR(n.x, 0.6f, 1e-5f);
  EXPECT_NEAR(n.y, 0.8f, 1e-5f);
}

TEST(Vec3Test, Length) {
  Vec3 a{1, 2, 2};
  EXPECT_NEAR(a.length(), 3.0f, 1e-5f);
}

// ── Mat4 tests ───────────────────────────────────────────────────────────────

TEST(Mat4Test, IdentityIsIdentity) {
  Mat4 id = Mat4::Identity();
  Vec3 v{1, 2, 3};
  Vec3 r = id.Transform(v);
  EXPECT_NEAR(r.x, 1, 1e-5f);
  EXPECT_NEAR(r.y, 2, 1e-5f);
  EXPECT_NEAR(r.z, 3, 1e-5f);
}

TEST(Mat4Test, Translation) {
  Mat4 t = Mat4::Translation({5, 6, 7});
  Vec3 v{1, 2, 3};
  Vec3 r = t.Transform(v);
  EXPECT_NEAR(r.x, 6, 1e-5f);
  EXPECT_NEAR(r.y, 8, 1e-5f);
  EXPECT_NEAR(r.z, 10, 1e-5f);
}

TEST(Mat4Test, Scale) {
  Mat4 s = Mat4::Scale({2, 3, 4});
  Vec3 v{1, 1, 1};
  Vec3 r = s.Transform(v);
  EXPECT_NEAR(r.x, 2, 1e-5f);
  EXPECT_NEAR(r.y, 3, 1e-5f);
  EXPECT_NEAR(r.z, 4, 1e-5f);
}

TEST(Mat4Test, PerspectiveNonZero) {
  Mat4 p = Mat4::Perspective(float(M_PI / 3.0), 1.0f, 0.1f, 100.0f);
  bool has_nonzero = false;
  for (int i = 0; i < 16; i++) {
    if (std::abs(p.m[i]) > 1e-5f) {
      has_nonzero = true;
      break;
    }
  }
  EXPECT_TRUE(has_nonzero);
}

TEST(Mat4Test, LookAtProducesValidMatrix) {
  Mat4 lk = Mat4::LookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
  Vec3 r = lk.Transform({0, 0, 0});
  EXPECT_NEAR(r.x, 0, 1e-4f);
  EXPECT_NEAR(r.y, 0, 1e-4f);
  EXPECT_NEAR(r.z, -5, 1e-4f);
}

TEST(Mat4Test, Multiply) {
  Mat4 t = Mat4::Translation({1, 0, 0});
  Mat4 s = Mat4::Scale({2, 2, 2});
  Mat4 ts = t * s;
  Vec3 r = ts.Transform({1, 0, 0});
  // Scale {1,0,0} -> {2,0,0}, then translate by {1,0,0} -> {3,0,0}
  EXPECT_NEAR(r.x, 3, 1e-4f);
  EXPECT_NEAR(r.y, 0, 1e-4f);
}

// ── Camera3D tests
// ────────────────────────────────────────────────────────────

TEST(Camera3DTest, ViewMatrixNonZero) {
  Camera3D cam;
  Mat4 v = cam.ViewMatrix();
  bool has_nonzero = false;
  for (float val : v.m) {
    if (std::abs(val) > 1e-5f) {
      has_nonzero = true;
      break;
    }
  }
  EXPECT_TRUE(has_nonzero);
}

TEST(Camera3DTest, ProjectionMatrixNonZero) {
  Camera3D cam;
  Mat4 p = cam.ProjectionMatrix(1.0f);
  bool has_nonzero = false;
  for (float val : p.m) {
    if (std::abs(val) > 1e-5f) {
      has_nonzero = true;
      break;
    }
  }
  EXPECT_TRUE(has_nonzero);
}

TEST(Camera3DTest, OrbitAroundChangesPosition) {
  Camera3D cam;
  Vec3 original = cam.position;
  cam.OrbitAround({0, 0, 0}, 0.5f, 0.1f);
  float dist = (cam.position - original).length();
  EXPECT_GT(dist, 0.0f);
  float orig_dist = original.length();
  float new_dist = cam.position.length();
  EXPECT_NEAR(orig_dist, new_dist, 1e-3f);
}

// ── SpatialScene tests
// ────────────────────────────────────────────────────────

TEST(SpatialSceneTest, DefaultConstruct) {
  SpatialScene scene;
  EXPECT_EQ(scene.WidgetCount(), 0);
}

TEST(SpatialSceneTest, AddWidgetReturnsId) {
  SpatialScene scene;
  SpatialWidget w;
  w.label = "test";
  int id = scene.AddWidget(w);
  EXPECT_EQ(id, 0);
  EXPECT_EQ(scene.WidgetCount(), 1);
}

TEST(SpatialSceneTest, GetWidgetReturnsWidget) {
  SpatialScene scene;
  SpatialWidget w;
  w.label = "hello";
  int id = scene.AddWidget(w);
  SpatialWidget* ptr = scene.GetWidget(id);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(ptr->label, "hello");
}

TEST(SpatialSceneTest, RemoveWidgetRemovesIt) {
  SpatialScene scene;
  SpatialWidget w;
  int id = scene.AddWidget(w);
  scene.RemoveWidget(id);
  EXPECT_EQ(scene.WidgetCount(), 0);
  EXPECT_EQ(scene.GetWidget(id), nullptr);
}

TEST(SpatialSceneTest, ProjectReturnsValidCoordsForFront) {
  SpatialScene scene;
  // Default camera at (0,0,5) looking at (0,0,0)
  // Origin is in front of camera
  auto proj = scene.Project({0, 0, 0}, 80, 24);
  EXPECT_TRUE(proj.visible);
  EXPECT_GT(proj.depth, 0.0f);
}

TEST(SpatialSceneTest, ProjectNotVisibleForBehindCamera) {
  SpatialScene scene;
  // Default camera at (0,0,5) looking at (0,0,0)
  // Point at z=10 is BEHIND camera
  auto proj = scene.Project({0, 0, 10}, 80, 24);
  EXPECT_FALSE(proj.visible);
}

TEST(SpatialSceneTest, RenderReturnsNonNull) {
  SpatialScene scene;
  SpatialWidget w;
  w.label = "test";
  w.position = {0, 0, 0};
  scene.AddWidget(w);
  auto el = scene.Render(80, 24);
  EXPECT_NE(el, nullptr);
}

TEST(SpatialSceneTest, BuildComponentReturnsNonNull) {
  SpatialScene scene;
  auto comp = scene.BuildComponent();
  EXPECT_NE(comp, nullptr);
  EXPECT_NE(comp->Render(), nullptr);
}

TEST(SpatialSceneTest, SpatialDockReturnsNonNull) {
  std::vector<ftxui::Component> widgets;
  for (int i = 0; i < 3; i++) {
    widgets.push_back(ftxui::Renderer([] { return ftxui::text("widget"); }));
  }
  auto comp = SpatialDock(std::move(widgets));
  ASSERT_NE(comp, nullptr);
  // SpatialDock creates scene internally; orbit stops on destruction
}

TEST(HUDTest, RenderReturnsNonNull) {
  SpatialScene scene;
  HUD hud;
  hud.AddCorner(ftxui::text("TL"), 0);
  hud.AddCrosshair(true);
  hud.AddCompass(true);
  auto scene_el = scene.Render(80, 24);
  auto el = hud.Render(std::move(scene_el));
  EXPECT_NE(el, nullptr);
}

}  // namespace
