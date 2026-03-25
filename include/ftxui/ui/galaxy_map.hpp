// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_GALAXY_MAP_HPP
#define FTXUI_UI_GALAXY_MAP_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/reactive_list.hpp"

namespace ftxui::ui {

/// @brief A star or other celestial object to display on the galaxy map.
struct StarObject {
  std::string name;
  float ra;        ///< Right ascension in degrees (0-360) — maps to longitude
  float dec;       ///< Declination in degrees (-90 to +90) — maps to latitude
  float distance;  ///< Distance in parsecs (for Z-depth shading)
  ftxui::Color color = ftxui::Color::White;
  char symbol = '*';  ///< Display symbol: '*' star, '+' station, 'o' planet
};

/// @brief A fleet or unit marker on the space map.
struct FleetMarker {
  std::string name;
  float ra, dec;
  ftxui::Color color;
  std::string faction;  ///< "rebel", "imperial", "federation", "klingon", etc.
};

/// @brief A hyperspace route or sensor sweep arc.
struct SpaceArc {
  float ra1, dec1;
  float ra2, dec2;
  ftxui::Color color;
  std::string label;
};

/// @brief Builder for the 3D galaxy map component.
///
/// Renders a celestial sphere using orthographic projection onto a braille
/// canvas. RA (right ascension) maps to longitude and Dec (declination) maps
/// to latitude, just as in the Globe component.
///
/// @code
/// auto map = GalaxyMap()
///     .StarField(StarFields::StarWars())
///     .Grid(true)
///     .Center(0.f, 0.f)
///     .Zoom(1.0f)
///     .OnSelect([](std::string name){ /* ... */ })
///     .Build();
/// @endcode
class GalaxyMap {
 public:
  GalaxyMap();

  /// @brief Add procedurally generated background stars.
  GalaxyMap& Stars(int count = 500, int seed = 42);

  /// @brief Add a specific star/object.
  GalaxyMap& AddStar(StarObject star);

  /// @brief Replace all stars with the provided field.
  GalaxyMap& StarField(std::vector<StarObject> stars);

  /// @brief Add a fleet marker.
  GalaxyMap& AddFleet(FleetMarker fleet);

  /// @brief Add a hyperspace route arc.
  GalaxyMap& AddRoute(SpaceArc arc);

  /// @brief Bind fleet markers to a ReactiveList (auto-updates on push/remove).
  GalaxyMap& BindFleets(std::shared_ptr<ReactiveList<FleetMarker>> fleets);

  /// @brief Center view (ra/dec in degrees).
  GalaxyMap& Center(float ra, float dec);

  /// @brief Zoom level (1.0 = default full sphere).
  GalaxyMap& Zoom(float zoom);

  /// @brief Enable RA/Dec grid lines.
  GalaxyMap& Grid(bool show = true);

  /// @brief Enable slow auto-rotation (pan across the sky).
  GalaxyMap& Rotate(float deg_per_second = 0.5f);

  /// @brief Callback when a star/object is selected (Enter key).
  GalaxyMap& OnSelect(std::function<void(std::string name)> fn);

  /// @brief Build the component.
  ftxui::Component Build();

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

/// @brief Pre-built star fields for common science fiction universes.
namespace StarFields {

/// @brief Famous Star Wars locations as celestial coordinates.
std::vector<StarObject> StarWars();

/// @brief Star Trek locations.
std::vector<StarObject> StarTrek();

/// @brief Random procedural galaxy.
std::vector<StarObject> Procedural(int count = 1000, int seed = 42);

}  // namespace StarFields

}  // namespace ftxui::ui

#endif  // FTXUI_UI_GALAXY_MAP_HPP
