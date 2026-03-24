// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_GEOMAP_HPP
#define FTXUI_UI_GEOMAP_HPP

#include <functional>
#include <memory>
#include <string_view>

#include "ftxui/component/component.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/geojson.hpp"

namespace ftxui::ui {

enum class Projection {
  Equirectangular,
  Mercator,
};

/// @brief Terminal map renderer for GeoJSON data using braille Canvas.
///
/// @code
/// auto collection = ui::LoadGeoJSON("world.geojson");
/// auto map = ui::GeoMap()
///     .Data(collection)
///     .SetProjection(Projection::Mercator)
///     .LineColor(Color::Green)
///     .ShowGraticule(true)
///     .Overlay(cities, Color::Yellow)           // city markers
///     .AddArc(-77, 38.9, 37.6, 55.75, Color::Red) // missile arc DC→Moscow
///     .OnSelect([](const GeoFeature& f){ /* ... */ })
///     .Build();
/// @endcode
class GeoMap {
 public:
  GeoMap();

  GeoMap& Data(const GeoCollection& col);
  GeoMap& Data(std::string_view geojson_str);
  GeoMap& LoadFile(std::string_view path);

  GeoMap& SetProjection(Projection p);
  GeoMap& PointColor(ftxui::Color c);
  GeoMap& LineColor(ftxui::Color c);
  GeoMap& FillColor(ftxui::Color c);
  GeoMap& ShowGraticule(bool v = true);
  GeoMap& GraticuleStep(float degrees = 30.0f);
  GeoMap& OnSelect(std::function<void(const GeoFeature&)> fn);

  /// Draw an additional GeoCollection on top of the base map with a
  /// custom color.  Call multiple times to stack overlays.
  GeoMap& Overlay(const GeoCollection& col, ftxui::Color color);

  /// Draw a great-circle arc between two (lon,lat) positions.
  /// @param steps  number of interpolation segments (default 32)
  GeoMap& AddArc(double lon1, double lat1, double lon2, double lat2,
                 ftxui::Color color, int steps = 32);

  ftxui::Component Build();

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_GEOMAP_HPP
