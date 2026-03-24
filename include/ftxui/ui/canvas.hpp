// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_CANVAS_HPP
#define FTXUI_UI_CANVAS_HPP

#include <functional>
#include <utility>
#include <vector>

#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

/// @brief High-level braille canvas with a logical coordinate system.
///
/// Wraps `ftxui::Canvas` (braille dot grid) and exposes a fluent API
/// with logical (float) coordinates that are mapped to dot positions.
///
/// Terminal cell layout: 1 char = 2 dots wide, 4 dots tall.
///
/// @ingroup ui
class BrailleCanvas {
 public:
  /// @param width_chars  Canvas width  in terminal character cells.
  /// @param height_chars Canvas height in terminal character cells.
  BrailleCanvas(int width_chars, int height_chars);

  /// Set the logical x-axis domain [xmin, xmax].
  BrailleCanvas& Domain(float xmin, float xmax);

  /// Set the logical y-axis range [ymin, ymax].
  BrailleCanvas& Range(float ymin, float ymax);

  /// Auto-fit domain + range to the given points with 5 % padding.
  BrailleCanvas& AutoFit(const std::vector<std::pair<float, float>>& pts);

  /// Draw a connected poly-line through @p pts (logical coords).
  BrailleCanvas& Plot(const std::vector<std::pair<float, float>>& pts,
                      ftxui::Color color = ftxui::Color::White);

  /// Draw isolated dots (scatter plot) at @p pts (logical coords).
  BrailleCanvas& Scatter(const std::vector<std::pair<float, float>>& pts,
                         ftxui::Color color = ftxui::Color::White);

  /// Sample @p fn over the current domain and draw as a line.
  BrailleCanvas& DrawFunction(std::function<float(float)> fn,
                              int samples = 300,
                              ftxui::Color color = ftxui::Color::Cyan);

  /// Draw a horizontal line at logical y value.
  BrailleCanvas& HLine(float y, ftxui::Color color = ftxui::Color::GrayDark);

  /// Draw a vertical line at logical x value.
  BrailleCanvas& VLine(float x, ftxui::Color color = ftxui::Color::GrayDark);

  /// Draw x-axis and y-axis through (0,0), clamped to the canvas edges.
  BrailleCanvas& Axes(ftxui::Color color = ftxui::Color::GrayLight);

  /// Draw a light grid of @p h_lines horizontal and @p v_lines vertical
  /// evenly-spaced lines.
  BrailleCanvas& Grid(int h_lines = 4,
                      int v_lines = 4,
                      ftxui::Color color = ftxui::Color::GrayDark);

  /// Produce an FTXUI Element (canvas DOM node) from all accumulated commands.
  ftxui::Element Render() const;

 private:
  int w_chars_;
  int h_chars_;
  float xmin_ = 0.f;
  float xmax_ = 1.f;
  float ymin_ = 0.f;
  float ymax_ = 1.f;

  /// Draw commands accumulated in order.
  /// Each receives the canvas plus its actual dot dimensions at render time.
  std::vector<std::function<void(ftxui::Canvas&, int w_dots, int h_dots)>>
      cmds_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_CANVAS_HPP
