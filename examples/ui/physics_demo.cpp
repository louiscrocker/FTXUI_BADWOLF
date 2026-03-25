// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
//
// physics_demo.cpp — 4-tab demo of the FTXUI physics engine.
//
// Tab 1: Gravity Sandbox — add bodies, watch them fall
// Tab 2: Pong           — two-player terminal pong (W/S vs Up/Down)
// Tab 3: Breakout       — classic brick breaker (←/→ to move)
// Tab 4: Custom         — live simulation with configurable gravity

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/animation.hpp"
#include "ftxui/ui/physics.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Custom simulation tab
// ─────────────────────────────────────────────────────

Component CustomTab() {
  struct CustomState {
    std::shared_ptr<PhysicsWorld> world;
    std::shared_ptr<PhysicsRenderer> renderer;
    std::string gravity_str = "9.8";
    std::string restitution_str = "0.7";
    float gravity = 9.8f;
    float restitution = 0.7f;
    int cb_id = -1;

    CustomState() { Rebuild(); }

    ~CustomState() {
      if (cb_id != -1) {
        AnimationLoop::Instance().RemoveCallback(cb_id);
      }
    }

    void Rebuild() {
      if (cb_id != -1) {
        AnimationLoop::Instance().RemoveCallback(cb_id);
        cb_id = -1;
      }
      try {
        gravity = std::stof(gravity_str);
      } catch (...) {
        gravity = 9.8f;
      }
      try {
        restitution = std::stof(restitution_str);
        restitution = std::max(0.0f, std::min(1.0f, restitution));
      } catch (...) {
        restitution = 0.7f;
      }

      world = std::make_shared<PhysicsWorld>(Vec2{0, gravity});
      renderer = std::make_shared<PhysicsRenderer>(world, 40.0f, 20.0f);
      world->SetBounds(40.0f, 20.0f);

      for (int i = 0; i < 6; ++i) {
        float x = 5.0f + i * 5.5f;
        float r = 0.5f + static_cast<float>(i % 3) * 0.3f;
        int id = world->AddCircle({x, 2.0f}, r, 1.0f);
        if (auto* b = world->GetBody(id)) {
          b->restitution = restitution;
        }
      }

      auto world_ptr = world;
      cb_id = AnimationLoop::Instance().OnFrame(
          [world_ptr](float dt) { world_ptr->Step(dt); });
    }
  };

  auto state = std::make_shared<CustomState>();

  auto gravity_input = Input(&state->gravity_str, "gravity");
  auto rest_input = Input(&state->restitution_str, "restitution");

  auto apply_btn = Button("Apply", [state]() { state->Rebuild(); });
  auto clear_btn = Button("Clear", [state]() { state->world->Clear(); });
  auto add_btn = Button("Add Ball", [state]() {
    static int cnt = 0;
    float x = 5.0f + static_cast<float>(cnt % 8) * 4.0f;
    int id = state->world->AddCircle({x, 1.0f}, 0.6f, 1.0f, Color::Cyan);
    if (auto* b = state->world->GetBody(id)) {
      b->restitution = state->restitution;
    }
    cnt++;
  });

  auto ctrl = Container::Horizontal({
      gravity_input,
      rest_input,
      apply_btn,
      clear_btn,
      add_btn,
  });

  auto body = Renderer(ctrl, [state, ctrl]() {
    int n = static_cast<int>(state->world->Bodies().size());
    return vbox({
               hbox({
                   text(" Custom Simulation") | color(Color::Yellow) | flex,
                   text(" Bodies: " + std::to_string(n)) |
                       color(Color::GrayLight),
               }),
               hbox({
                   text(" Gravity: ") | color(Color::GrayLight),
                   ctrl->ChildAt(0)->Render() | size(WIDTH, EQUAL, 8),
                   text("  Restitution: ") | color(Color::GrayLight),
                   ctrl->ChildAt(1)->Render() | size(WIDTH, EQUAL, 6),
                   text("  "),
                   ctrl->ChildAt(2)->Render(),
                   text(" "),
                   ctrl->ChildAt(3)->Render(),
                   text(" "),
                   ctrl->ChildAt(4)->Render(),
               }),
               state->renderer->Render() | flex,
           }) |
           border;
  });

  return body;
}

// ── Main
// ──────────────────────────────────────────────────────────────────────

int main() {
  AnimationLoop::Instance().SetFPS(60);

  int selected_tab = 0;

  auto gravity_tab = Games::Gravity();
  auto pong_tab = Games::Pong();
  auto breakout_tab = Games::Breakout();
  auto custom_tab = CustomTab();

  auto tab_content = Container::Tab(
      {gravity_tab, pong_tab, breakout_tab, custom_tab}, &selected_tab);

  std::vector<std::string> tab_names = {
      "🌍 Gravity Sandbox",
      "🏓 Pong",
      "🧱 Breakout",
      "⚙️  Custom",
  };

  auto tab_toggle = Toggle(&tab_names, &selected_tab);

  auto layout = Container::Vertical({tab_toggle, tab_content});

  auto renderer = Renderer(layout, [&]() {
    return vbox({
               hbox({
                   tab_toggle->Render(),
                   filler(),
                   text(" [Q]uit") | color(Color::GrayDark),
               }),
               separator(),
               tab_content->Render() | flex,
           }) |
           border;
  });

  auto with_quit = CatchEvent(renderer, [](Event e) {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    return false;
  });

  App::Fullscreen().Loop(with_quit);
  return 0;
}
