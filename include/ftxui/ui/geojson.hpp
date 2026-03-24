// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_GEOJSON_HPP
#define FTXUI_UI_GEOJSON_HPP

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ftxui::ui {

struct GeoPoint {
  double lon = 0;
  double lat = 0;
};

using GeoRing = std::vector<GeoPoint>;

struct GeoGeometry {
  std::string type;
  std::vector<GeoPoint> points;
  std::vector<GeoRing> rings;
  std::vector<std::vector<GeoRing>> multipolygon;
};

struct GeoFeature {
  GeoGeometry geometry;
  std::map<std::string, std::string> properties;
  std::string name() const;
};

struct GeoCollection {
  std::vector<GeoFeature> features;
  double min_lon = 180;
  double max_lon = -180;
  double min_lat = 90;
  double max_lat = -90;
  void ComputeBounds();
};

GeoCollection ParseGeoJSON(std::string_view json);
GeoCollection LoadGeoJSON(std::string_view path);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_GEOJSON_HPP
