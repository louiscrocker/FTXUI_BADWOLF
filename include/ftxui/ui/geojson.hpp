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

/// @brief A geographic coordinate (longitude/latitude in degrees).
struct GeoPoint {
  double lon = 0;
  double lat = 0;
};

/// @brief A closed ring of geographic points (exterior/interior polygon ring).
using GeoRing = std::vector<GeoPoint>;

/// @brief Raw geometry from a GeoJSON feature (Point, LineString, Polygon, etc.).
struct GeoGeometry {
  std::string type;
  std::vector<GeoPoint> points;
  std::vector<GeoRing> rings;
  std::vector<std::vector<GeoRing>> multipolygon;
};

/// @brief A single GeoJSON feature with geometry and key/value properties.
struct GeoFeature {
  GeoGeometry geometry;
  std::map<std::string, std::string> properties;
  std::string name() const;
};

/// @brief A parsed GeoJSON FeatureCollection with pre-computed bounds.
struct GeoCollection {
  std::vector<GeoFeature> features;
  double min_lon = 180;
  double max_lon = -180;
  double min_lat = 90;
  double max_lat = -90;
  /// @brief Recompute min/max lon/lat from all features.
  void ComputeBounds();
};

/// @brief Parse a GeoJSON string into a GeoCollection.
GeoCollection ParseGeoJSON(std::string_view json);
/// @brief Load a GeoJSON file from disk into a GeoCollection.
GeoCollection LoadGeoJSON(std::string_view path);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_GEOJSON_HPP
