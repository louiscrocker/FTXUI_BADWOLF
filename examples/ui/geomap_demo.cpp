// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @brief GeoJSON world map demo using FTXUI braille Canvas renderer.
///
/// Controls:
///   Arrow keys — pan
///   + / -      — zoom in / out
///   R          — reset view
///   Enter      — select nearest feature
///   Q          — quit

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/geomap.hpp"
#include "ftxui/ui/geojson.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// Simplified world country outlines (~10-20 vertices each).
// Coordinates are [longitude, latitude] pairs.
static const char kWorldGeoJSON[] = R"GEO({
"type":"FeatureCollection",
"features":[
  {"type":"Feature","properties":{"name":"USA"},"geometry":{"type":"Polygon","coordinates":[[
    [-125,49],[-95,49],[-75,45],[-67,47],[-70,43],[-77,35],[-81,25],[-97,26],
    [-105,22],[-117,32],[-124,36],[-125,49]
  ]]}},
  {"type":"Feature","properties":{"name":"Canada"},"geometry":{"type":"Polygon","coordinates":[[
    [-141,60],[-125,49],[-95,49],[-75,45],[-67,47],[-60,47],[-52,47],
    [-55,60],[-79,63],[-95,63],[-120,65],[-141,60]
  ]]}},
  {"type":"Feature","properties":{"name":"Mexico"},"geometry":{"type":"Polygon","coordinates":[[
    [-117,32],[-97,26],[-94,18],[-90,18],[-88,16],[-83,10],[-85,10],
    [-91,15],[-105,22],[-117,32]
  ]]}},
  {"type":"Feature","properties":{"name":"Brazil"},"geometry":{"type":"Polygon","coordinates":[[
    [-73,-4],[-60,5],[-50,5],[-35,-5],[-35,-22],[-50,-33],[-55,-34],
    [-65,-55],[-70,-55],[-75,-45],[-80,-2],[-73,-4]
  ]]}},
  {"type":"Feature","properties":{"name":"Argentina"},"geometry":{"type":"Polygon","coordinates":[[
    [-73,-40],[-55,-35],[-53,-34],[-65,-55],[-70,-55],[-75,-50],[-73,-40]
  ]]}},
  {"type":"Feature","properties":{"name":"UK"},"geometry":{"type":"Polygon","coordinates":[[
    [-5,50],[1,51],[1,53],[-3,58],[-5,55],[-6,53],[-5,50]
  ]]}},
  {"type":"Feature","properties":{"name":"France"},"geometry":{"type":"Polygon","coordinates":[[
    [-4,43],[8,43],[8,47],[6,50],[2,51],[-2,49],[-4,48],[-4,43]
  ]]}},
  {"type":"Feature","properties":{"name":"Germany"},"geometry":{"type":"Polygon","coordinates":[[
    [6,47],[15,47],[15,55],[14,54],[9,55],[6,54],[5,50],[6,47]
  ]]}},
  {"type":"Feature","properties":{"name":"Russia"},"geometry":{"type":"Polygon","coordinates":[[
    [28,55],[40,50],[60,52],[90,65],[140,65],[145,55],[130,42],
    [90,45],[60,42],[38,42],[28,45],[28,55]
  ]]}},
  {"type":"Feature","properties":{"name":"China"},"geometry":{"type":"Polygon","coordinates":[[
    [73,20],[90,20],[100,8],[115,8],[120,20],[135,35],[135,48],
    [120,52],[90,52],[75,40],[73,38],[73,20]
  ]]}},
  {"type":"Feature","properties":{"name":"India"},"geometry":{"type":"Polygon","coordinates":[[
    [68,8],[80,8],[88,20],[92,27],[80,35],[72,28],[68,22],[68,8]
  ]]}},
  {"type":"Feature","properties":{"name":"Australia"},"geometry":{"type":"Polygon","coordinates":[[
    [114,-22],[114,-35],[120,-38],[148,-38],[154,-28],[150,-18],
    [138,-12],[120,-14],[114,-22]
  ]]}},
  {"type":"Feature","properties":{"name":"Africa"},"geometry":{"type":"Polygon","coordinates":[[
    [-18,15],[-18,5],[0,0],[15,-5],[35,-5],[40,-10],[45,-15],[37,-22],
    [18,-35],[12,-35],[-5,-15],[-17,-12],[-18,0],[-18,15]
  ]]}},
  {"type":"Feature","properties":{"name":"Japan"},"geometry":{"type":"Polygon","coordinates":[[
    [130,31],[131,34],[135,35],[141,41],[145,44],[141,46],
    [135,38],[130,33],[130,31]
  ]]}},
  {"type":"Feature","properties":{"name":"Egypt"},"geometry":{"type":"Polygon","coordinates":[[
    [25,22],[35,22],[35,31],[33,31],[28,30],[25,28],[25,22]
  ]]}}
]
})GEO";

int main() {
  std::string selected = "None";

  auto map_comp = GeoMap()
      .Data(kWorldGeoJSON)
      .SetProjection(Projection::Equirectangular)
      .LineColor(Color::Green)
      .PointColor(Color::Yellow)
      .ShowGraticule(true)
      .GraticuleStep(30.0f)
      .OnSelect([&](const GeoFeature& f) { selected = f.name(); })
      .Build();

  auto root = Renderer(map_comp, [&]() -> Element {
    return vbox({
        text("  ★  GeoJSON World Map  ★") | bold | color(Color::Cyan) | hcenter,
        separator(),
        map_comp->Render() | flex,
        separator(),
        hbox({
            text(" Selected: ") | bold,
            text(selected) | color(Color::Yellow),
            filler(),
            text("[←→↑↓] pan  [+/-] zoom  [R] reset  [Enter] select  [Q] quit")
                | color(Color::GrayDark),
            text(" "),
        }),
    });
  });

  auto app_comp = CatchEvent(root, [](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      App::Active()->Exit();
      return true;
    }
    return false;
  });

  RunFullscreen(app_comp);
  return 0;
}
