// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#pragma once

#include <string>

namespace ftxui::ui {

/// Returns the Natural Earth 110m world map as a GeoJSON string.
/// Coastlines (128 polygons) + country borders (333 lines), 8123 total points.
/// Source: Natural Earth (public domain), naturalearthdata.com
std::string WorldMapGeoJSON();

}  // namespace ftxui::ui
