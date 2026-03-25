// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file globe_demo.cpp
///
/// Full-screen animated 3D spinning globe demo.
///
/// Controls:
///   Arrow keys  — pan/tilt the globe
///   +/-         — faster / slower rotation
///   G           — toggle graticule (lat/lon grid)
///   Q           — quit

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/globe.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  auto globe_comp = GlobeMap()
                        .LineColor(Color::GreenLight)
                        .RotationSpeed(0.8)
                        .InitialLon(0.0)
                        .InitialLat(20.0)
                        .ShowGraticule(true)
                        .GraticuleStep(30.0f)
                        .Build();

  auto root = Renderer(globe_comp, [&]() -> Element {
    return vbox({
        text("  ★  Spinning Globe  ★") | bold | color(Color::Cyan) | hcenter,
        separator(),
        globe_comp->Render() | flex,
        separator(),
        hbox({
            text(" [←→↑↓] pan/tilt") | color(Color::GrayDark),
            text("  [+/-] speed") | color(Color::GrayDark),
            text("  [G] graticule") | color(Color::GrayDark),
            text("  [Q] quit") | color(Color::GrayDark),
            filler(),
        }),
    });
  });

  auto app_comp = CatchEvent(root, [](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    return false;
  });

  RunFullscreen(app_comp);
  return 0;
}
