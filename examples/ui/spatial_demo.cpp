// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
/// @file spatial_demo.cpp
/// Demonstrates the SpatialScene / AR computing layer:
/// 3D widget positioning, depth-sorted rendering, perspective projection.
///
/// Controls (all tabs):
///   Tab/Shift-Tab — switch tabs
///   q             — quit
///
/// Tab 1 Spatial Dock: auto-rotating circle of widgets (mouse drag to orbit)
/// Tab 2 3D Grid:      9 widgets in 3×3 grid, WASD camera movement
/// Tab 3 Free:         arrow keys move selected widget in 3D space
/// Tab 4 HUD:          compass + crosshair overlay on spatial scene

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/spatial.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Helper: make a simple labelled widget
// ─────────────────────────────────────
static ftxui::Component MakeWidget(const std::string& title,
                                   const std::string& body,
                                   ftxui::Color color = Color::Cyan) {
  return ftxui::Renderer([title, body, color] {
    return ftxui::vbox({
               ftxui::text(title) | ftxui::bold | ftxui::color(color),
               ftxui::separator(),
               ftxui::text(body),
           }) |
           ftxui::border;
  });
}

// ── Tab 1: Spatial Dock
// ───────────────────────────────────────────────────────
static ftxui::Component MakeSpatialDockTab() {
  std::vector<ftxui::Component> widgets = {
      MakeWidget("Clock", "12:34:56", Color::Cyan),
      MakeWidget("Counter", "Count: 42", Color::Green),
      MakeWidget("Log", "INFO: ok", Color::White),
      MakeWidget("Spark", "1 2 3 5 7", Color::Yellow),
      MakeWidget("Weather", "22C Sunny", Color::Blue),
      MakeWidget("System", "CPU: 12%", Color::Magenta),
  };

  auto scene = std::make_shared<SpatialScene>(Camera3D{{0, 2, 8}});
  for (auto& w : widgets) {
    SpatialWidget sw;
    sw.content = w;
    sw.content_width = 14;
    sw.content_height = 4;
    scene->AddWidget(std::move(sw));
  }
  SpatialLayouts::Circle(*scene, 4.0f, 0.0f);
  scene->StartOrbit(20.0f);

  struct MouseState {
    bool dragging = false;
    int last_x = 0;
    int last_y = 0;
  };
  auto ms = std::make_shared<MouseState>();

  auto renderer =
      ftxui::Renderer([scene] { return scene->Render(80, 22) | ftxui::flex; });

  return ftxui::CatchEvent(renderer, [scene, ms](ftxui::Event e) {
    if (e.is_mouse()) {
      auto& m = e.mouse();
      if (m.button == ftxui::Mouse::Left) {
        if (m.motion == ftxui::Mouse::Pressed) {
          ms->dragging = true;
          ms->last_x = m.x;
          ms->last_y = m.y;
        } else if (m.motion == ftxui::Mouse::Released) {
          ms->dragging = false;
        } else if (m.motion == ftxui::Mouse::Moved && ms->dragging) {
          float dyaw = float(m.x - ms->last_x) * 0.05f;
          float dpitch = float(m.y - ms->last_y) * 0.05f;
          scene->camera().OrbitAround({0, 0, 0}, dyaw, dpitch);
          ms->last_x = m.x;
          ms->last_y = m.y;
        }
      }
      if (m.button == ftxui::Mouse::WheelUp) {
        scene->camera().Zoom(-0.5f);
      }
      if (m.button == ftxui::Mouse::WheelDown) {
        scene->camera().Zoom(0.5f);
      }
    }
    return false;
  });
}

// ── Tab 2: 3D Grid
// ────────────────────────────────────────────────────────────
static ftxui::Component MakeGridTab() {
  auto scene = std::make_shared<SpatialScene>(Camera3D{{0, 4, 10}});
  const char* names[] = {"Alpha", "Beta", "Gamma", "Delta", "Epsilon",
                         "Zeta",  "Eta",  "Theta", "Iota"};
  for (int i = 0; i < 9; i++) {
    SpatialWidget sw;
    sw.content = MakeWidget(names[i], "data: " + std::to_string(i * 7));
    sw.content_width = 12;
    sw.content_height = 4;
    sw.label = names[i];
    scene->AddWidget(std::move(sw));
  }
  SpatialLayouts::Grid(*scene, 3, 3.0f);

  auto renderer =
      ftxui::Renderer([scene] { return scene->Render(80, 22) | ftxui::flex; });

  return ftxui::CatchEvent(renderer, [scene](ftxui::Event e) {
    if (e == ftxui::Event::Character('w') || e == ftxui::Event::ArrowUp) {
      scene->camera().Pan({0, 0, -0.5f});
      return true;
    }
    if (e == ftxui::Event::Character('s') || e == ftxui::Event::ArrowDown) {
      scene->camera().Pan({0, 0, 0.5f});
      return true;
    }
    if (e == ftxui::Event::Character('a') || e == ftxui::Event::ArrowLeft) {
      scene->camera().Pan({-0.5f, 0, 0});
      return true;
    }
    if (e == ftxui::Event::Character('d') || e == ftxui::Event::ArrowRight) {
      scene->camera().Pan({0.5f, 0, 0});
      return true;
    }
    if (e == ftxui::Event::Character('+')) {
      scene->camera().Zoom(-0.5f);
      return true;
    }
    if (e == ftxui::Event::Character('-')) {
      scene->camera().Zoom(0.5f);
      return true;
    }
    return false;
  });
}

// ── Tab 3: Free Placement
// ─────────────────────────────────────────────────────
static ftxui::Component MakeFreePlacementTab() {
  auto scene = std::make_shared<SpatialScene>();
  auto selected_id = std::make_shared<int>(0);

  {
    SpatialWidget w;
    w.content = MakeWidget("Box A", "pos: 0,0,0", Color::Red);
    w.position = {0, 0, 0};
    w.label = "A";
    *selected_id = scene->AddWidget(std::move(w));
  }
  {
    SpatialWidget w;
    w.content = MakeWidget("Box B", "pos: 3,0,-2", Color::Green);
    w.position = {3, 0, -2};
    w.label = "B";
    scene->AddWidget(std::move(w));
  }
  {
    SpatialWidget w;
    w.content = MakeWidget("Box C", "pos: -2,1,1", Color::Blue);
    w.position = {-2, 1, 1};
    w.label = "C";
    scene->AddWidget(std::move(w));
  }

  auto ids = scene->WidgetIds();

  auto renderer = ftxui::Renderer([scene, selected_id, ids] {
    auto scene_el = scene->Render(76, 20);
    SpatialWidget* sw = scene->GetWidget(*selected_id);
    std::string pos_info = "Selected: none";
    if (sw) {
      pos_info = "Selected [" + sw->label +
                 "] x=" + std::to_string(int(sw->position.x)) +
                 " y=" + std::to_string(int(sw->position.y)) +
                 " z=" + std::to_string(int(sw->position.z));
    }
    return ftxui::vbox({
        scene_el | ftxui::flex,
        ftxui::separator(),
        ftxui::text(pos_info) | ftxui::color(Color::Yellow),
        ftxui::text("<- ->: move XZ  PgUp/PgDn: move Y  Tab: select widget"),
    });
  });

  return ftxui::CatchEvent(renderer, [scene, selected_id, ids](ftxui::Event e) {
    if (e == ftxui::Event::ArrowLeft) {
      scene->UpdateWidget(*selected_id,
                          [](SpatialWidget& w) { w.position.x -= 0.5f; });
      return true;
    }
    if (e == ftxui::Event::ArrowRight) {
      scene->UpdateWidget(*selected_id,
                          [](SpatialWidget& w) { w.position.x += 0.5f; });
      return true;
    }
    if (e == ftxui::Event::ArrowUp) {
      scene->UpdateWidget(*selected_id,
                          [](SpatialWidget& w) { w.position.z -= 0.5f; });
      return true;
    }
    if (e == ftxui::Event::ArrowDown) {
      scene->UpdateWidget(*selected_id,
                          [](SpatialWidget& w) { w.position.z += 0.5f; });
      return true;
    }
    if (e == ftxui::Event::PageUp) {
      scene->UpdateWidget(*selected_id,
                          [](SpatialWidget& w) { w.position.y += 0.5f; });
      return true;
    }
    if (e == ftxui::Event::PageDown) {
      scene->UpdateWidget(*selected_id,
                          [](SpatialWidget& w) { w.position.y -= 0.5f; });
      return true;
    }
    if (e == ftxui::Event::Tab) {
      if (!ids.empty()) {
        auto it = std::find(ids.begin(), ids.end(), *selected_id);
        if (it != ids.end()) {
          ++it;
        }
        if (it == ids.end()) {
          it = ids.begin();
        }
        *selected_id = *it;
      }
      return true;
    }
    return false;
  });
}

// ── Tab 4: HUD Demo
// ───────────────────────────────────────────────────────────
static ftxui::Component MakeHUDTab() {
  auto scene = std::make_shared<SpatialScene>(Camera3D{{0, 2, 8}});
  for (int i = 0; i < 4; i++) {
    SpatialWidget w;
    w.content = MakeWidget("Node " + std::to_string(i), "active");
    w.position = {std::cos(float(i) * float(M_PI) / 2) * 3.0f, 0,
                  std::sin(float(i) * float(M_PI) / 2) * 3.0f};
    w.label = "N" + std::to_string(i);
    scene->AddWidget(std::move(w));
  }

  HUD hud;
  hud.AddCorner(ftxui::vbox({ftxui::text("Spatial AR") | ftxui::bold,
                             ftxui::text("v1.0")}),
                0);
  hud.AddCorner(
      ftxui::vbox({ftxui::text("FPS: 60"), ftxui::text("Widgets: 4")}), 1);
  hud.AddCorner(ftxui::text("q: quit"), 2);
  hud.AddCrosshair(true);
  hud.AddCompass(true);

  return ftxui::Renderer([scene, hud = std::move(hud)] {
    auto scene_el = scene->Render(80, 22) | ftxui::flex;
    return hud.Render(std::move(scene_el));
  });
}

// ── Main
// ──────────────────────────────────────────────────────────────────────
int main() {
  int tab_index = 0;
  std::vector<std::string> tab_names = {"Spatial Dock", "3D Grid",
                                        "Free Placement", "HUD"};
  auto tabs = ftxui::Container::Tab(
      {
          MakeSpatialDockTab(),
          MakeGridTab(),
          MakeFreePlacementTab(),
          MakeHUDTab(),
      },
      &tab_index);

  auto layout = ftxui::Renderer(tabs, [&] {
    return ftxui::vbox({
        ftxui::hbox({
            ftxui::text("Spatial AR Computing  "),
            ftxui::text(tab_names[0]) |
                (tab_index == 0 ? ftxui::bold : ftxui::dim),
            ftxui::text("  "),
            ftxui::text(tab_names[1]) |
                (tab_index == 1 ? ftxui::bold : ftxui::dim),
            ftxui::text("  "),
            ftxui::text(tab_names[2]) |
                (tab_index == 2 ? ftxui::bold : ftxui::dim),
            ftxui::text("  "),
            ftxui::text(tab_names[3]) |
                (tab_index == 3 ? ftxui::bold : ftxui::dim),
        }) | ftxui::border,
        tabs->Render() | ftxui::flex,
    });
  });

  auto root = ftxui::CatchEvent(layout, [&](ftxui::Event e) {
    if (e == ftxui::Event::Character('q')) {
      if (App* a = App::Active()) {
        a->Exit(0);
      }
      return true;
    }
    if (e == ftxui::Event::Tab) {
      tab_index = (tab_index + 1) % int(tab_names.size());
      return true;
    }
    if (e == ftxui::Event::TabReverse) {
      tab_index =
          (tab_index - 1 + int(tab_names.size())) % int(tab_names.size());
      return true;
    }
    return false;
  });

  ftxui::ui::App app;
  app.Add(root);
  app.Run();
  return 0;
}
