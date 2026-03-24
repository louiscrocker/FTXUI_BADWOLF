// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_CHARTS_HPP
#define FTXUI_UI_CHARTS_HPP

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

// ── Sparkline ─────────────────────────────────────────────────────────────────

/// @brief Inline braille mini-chart — renders a line graph in a single row.
///
/// Produces a single-character-tall element using braille dot rendering.
/// Perfect for embedding trend indicators in status bars or table cells.
///
/// @param values       Data points (auto-normalized to min/max).
/// @param width_chars  Width in terminal character cells.
/// @param color        Line color.
/// @ingroup ui
ftxui::Element Sparkline(const std::vector<float>& values,
                          int width_chars = 20,
                          ftxui::Color color = ftxui::Color::Cyan);

// ── LineChart ─────────────────────────────────────────────────────────────────

/// @brief Fluent builder for a braille line-chart component.
///
/// @ingroup ui
class LineChart {
 public:
  /// Add a series with y-values at x = 0, 1, 2, …
  LineChart& Series(std::string label,
                    std::vector<float> values,
                    ftxui::Color color = ftxui::Color::Default);

  /// Add a series with explicit (x, y) pairs.
  LineChart& Series(std::string label,
                    std::vector<std::pair<float, float>> points,
                    ftxui::Color color = ftxui::Color::Default);

  LineChart& Title(std::string title);
  LineChart& XLabel(std::string label);
  LineChart& YLabel(std::string label);
  LineChart& ShowLegend(bool v = true);
  LineChart& ShowAxes(bool v = true);
  LineChart& ShowGrid(bool v = true);
  LineChart& Domain(float xmin, float xmax);
  LineChart& Range(float ymin, float ymax);

  /// @brief Build a static Renderer component.
  ftxui::Component Build();

 private:
  struct SeriesEntry {
    std::string label;
    std::vector<std::pair<float, float>> points;
    ftxui::Color color;
  };

  std::vector<SeriesEntry> series_;
  std::string title_;
  std::string xlabel_;
  std::string ylabel_;
  bool show_legend_ = true;
  bool show_axes_ = true;
  bool show_grid_ = true;
  bool has_domain_ = false;
  bool has_range_ = false;
  float xmin_ = 0.f;
  float xmax_ = 1.f;
  float ymin_ = 0.f;
  float ymax_ = 1.f;
};

// ── BarChart ──────────────────────────────────────────────────────────────────

/// @brief Fluent builder for a bar-chart component.
///
/// @ingroup ui
class BarChart {
 public:
  BarChart& Bar(std::string label,
                float value,
                ftxui::Color color = ftxui::Color::Default);
  BarChart& Title(std::string title);
  BarChart& Horizontal(bool v = true);
  BarChart& ShowValues(bool v = true);
  BarChart& MaxValue(float v);

  /// @brief Build a static Renderer component.
  ftxui::Component Build();

 private:
  struct BarEntry {
    std::string label;
    float value;
    ftxui::Color color;
  };

  std::vector<BarEntry> bars_;
  std::string title_;
  bool horizontal_ = false;
  bool show_values_ = true;
  bool has_max_ = false;
  float max_value_ = 0.f;
};

// ── ScatterPlot ───────────────────────────────────────────────────────────────

/// @brief Fluent builder for a braille scatter-plot component.
///
/// @ingroup ui
class ScatterPlot {
 public:
  ScatterPlot& Series(std::string label,
                      std::vector<std::pair<float, float>> points,
                      ftxui::Color color = ftxui::Color::Default);
  ScatterPlot& Title(std::string title);
  ScatterPlot& ShowAxes(bool v = true);
  ScatterPlot& ShowGrid(bool v = true);

  /// @brief Build a static Renderer component.
  ftxui::Component Build();

 private:
  struct SeriesEntry {
    std::string label;
    std::vector<std::pair<float, float>> points;
    ftxui::Color color;
  };

  std::vector<SeriesEntry> series_;
  std::string title_;
  bool show_axes_ = true;
  bool show_grid_ = true;
};

// ── Histogram ─────────────────────────────────────────────────────────────────

/// @brief Fluent builder for a histogram component.
///
/// @ingroup ui
class Histogram {
 public:
  Histogram& Data(std::vector<float> values);
  Histogram& Bins(int n);
  Histogram& Title(std::string title);
  Histogram& Color(ftxui::Color c);

  /// @brief Build a static Renderer component.
  ftxui::Component Build();

 private:
  std::vector<float> data_;
  int bins_ = 10;
  std::string title_;
  ftxui::Color color_ = ftxui::Color::Default;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_CHARTS_HPP
