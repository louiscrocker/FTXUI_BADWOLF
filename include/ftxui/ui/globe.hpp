// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_GLOBE_HPP
#define FTXUI_UI_GLOBE_HPP

#include <memory>

#include "ftxui/component/component.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

/// @brief Animated spinning globe using orthographic (azimuthal) projection.
///
/// Renders the Natural Earth 110m coastlines as a 3D sphere on a braille
/// canvas that fills the allocated terminal space.
///
/// @code
/// auto globe = GlobeMap()
///     .LineColor(Color::GreenLight)
///     .RotationSpeed(0.8)      // degrees per animation tick (~50 ms)
///     .InitialLon(0.0)         // starting center longitude
///     .InitialLat(20.0)        // starting center latitude (tilt)
///     .ShowGraticule(true)
///     .GraticuleStep(30.0f)
///     .Build();
/// @endcode
class GlobeMap {
 public:
  GlobeMap();

  /// Line (coastline / border) colour.
  GlobeMap& LineColor(ftxui::Color c);

  /// Point feature colour.
  GlobeMap& PointColor(ftxui::Color c);

  /// Rotation speed in degrees per animation tick (~50 ms frame time).
  /// Positive values spin eastward; 0 = static.
  GlobeMap& RotationSpeed(double deg_per_tick);

  /// Starting center longitude (degrees, -180 … 180).
  GlobeMap& InitialLon(double lon);

  /// Starting center latitude / tilt (degrees, -90 … 90).
  GlobeMap& InitialLat(double lat);

  /// Show latitude / longitude grid lines.
  GlobeMap& ShowGraticule(bool v = true);

  /// Grid-line spacing in degrees (default 30).
  GlobeMap& GraticuleStep(float degrees = 30.0f);

  /// Build and return an FTXUI Component.
  /// The component auto-starts its animation thread and fills available space.
  ftxui::Component Build();

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_GLOBE_HPP
