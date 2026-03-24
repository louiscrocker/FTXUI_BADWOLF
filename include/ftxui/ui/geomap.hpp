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

  ftxui::Component Build();

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_GEOMAP_HPP
