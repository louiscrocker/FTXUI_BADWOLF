// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file galaxy_demo.cpp
///
/// Galactic Operations Center — full-screen 3D space map.
///
/// Controls:
///   Arrow keys  — pan the view
///   +/-         — zoom in/out
///   g           — toggle RA/Dec grid
///   f           — toggle between Star Wars and Star Trek star fields
///   q           — quit

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/galaxy_map.hpp"
#include "ftxui/ui/reactive_list.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  // Shared fleet list — updated by background thread
  auto fleets = MakeReactiveList<FleetMarker>({
      {"Rebel Fleet", 185.f, -15.f, Color::GreenLight, "rebel"},
      {"Imperial Fleet", 75.f, 15.f, Color::RedLight, "imperial"},
      {"Neutral Observer", 0.f, 0.f, Color::GrayLight, "neutral"},
  });

  // Current star field selection
  std::atomic<bool> use_starwars{true};
  std::atomic<bool> show_grid{false};
  std::atomic<int> stardate{0};

  // Rebuild the galaxy map component
  auto build_map = [&]() -> Component {
    auto field =
        use_starwars.load() ? StarFields::StarWars() : StarFields::StarTrek();
    // Add background stars
    auto bg = StarFields::Procedural(300, 99);
    field.insert(field.end(), bg.begin(), bg.end());

    return GalaxyMap()
        .StarField(std::move(field))
        .AddFleet({"Rebel Fleet", 185.f, -15.f, Color::GreenLight, "rebel"})
        .AddFleet({"Imperial Fleet", 75.f, 15.f, Color::RedLight, "imperial"})
        .AddFleet({"Neutral", 0.f, 0.f, Color::GrayLight, "neutral"})
        .AddRoute(
            {75.f, 15.f, 185.f, -15.f, Color::BlueLight, "Coruscant-Endor"})
        .AddRoute({220.f, -25.f, 185.f, -15.f, Color::Yellow, "Tatooine-Endor"})
        .AddRoute({0.f, 0.f, 55.f, 40.f, Color::Cyan, "Sol-Vulcan"})
        .Grid(show_grid.load())
        .Build();
  };

  // Initial map component (held in shared_ptr so we can swap)
  auto map_holder = std::make_shared<std::shared_ptr<Component>>(
      std::make_shared<Component>(build_map()));

  // Background thread: move the Death Star every 3 seconds
  std::atomic<bool> bg_running{true};
  std::thread bg_thread([&]() {
    float ra = 180.f;
    while (bg_running.load()) {
      for (int i = 0; i < 30 && bg_running.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      ra += 5.f;
      if (ra >= 360.f) {
        ra -= 360.f;
      }
      stardate.fetch_add(1);
      if (App* a = App::Active()) {
        a->Post([] {});
      }
    }
  });

  // Build status bar text
  auto status_text = [&]() -> std::string {
    std::string field_name = use_starwars.load() ? "Star Wars" : "Star Trek";
    return " Fleets: 3 | Contacts: " +
           std::to_string(use_starwars.load() ? 10 : 10) +
           " | Stardate: " + std::to_string(stardate.load()) + " BBY" + "  [" +
           field_name + "]" +
           "   q=quit  f=toggle  g=grid  +/-=zoom  arrows=pan";
  };

  // Map component (rebuilds when f/g pressed)
  auto galaxy_comp = build_map();

  // Root layout
  auto root = Renderer(galaxy_comp, [&]() -> Element {
    return vbox({
        hbox({
            text(" ░ GALACTIC OPERATIONS CENTER ░") | bold |
                color(Color::CyanLight),
            filler(),
        }) | bgcolor(Color::NavyBlue),
        separator(),
        galaxy_comp->Render() | flex,
        separator(),
        text(status_text()) | color(Color::GrayLight),
    });
  });

  // Key handler
  bool grid_on = false;
  auto app_comp = CatchEvent(root, [&](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      bg_running.store(false);
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    if (e == Event::Character('f') || e == Event::Character('F')) {
      use_starwars.store(!use_starwars.load());
      galaxy_comp = build_map();
      // Trigger re-render
      if (App* a = App::Active()) {
        a->Post([] {});
      }
      return true;
    }
    if (e == Event::Character('g') || e == Event::Character('G')) {
      grid_on = !grid_on;
      show_grid.store(grid_on);
      galaxy_comp = build_map();
      if (App* a = App::Active()) {
        a->Post([] {});
      }
      return true;
    }
    return false;
  });

  RunFullscreen(app_comp);

  bg_running.store(false);
  if (bg_thread.joinable()) {
    bg_thread.join();
  }
  return 0;
}
