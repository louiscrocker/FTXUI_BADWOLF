// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file charts_demo.cpp
/// Fullscreen demo of the BrailleCanvas chart suite:
///   Top-left:     LineChart — sin / cos / clamped-tan over [0, 2π]
///   Top-right:    BarChart  — 6 bars (monthly revenue)
///   Bottom-left:  ScatterPlot — 2 point clusters
///   Bottom-right: Histogram — approximation of a normal distribution

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

static constexpr float kPi = 3.14159265f;

// ── LineChart — trigonometric functions ───────────────────────────────────────

Component MakeLineChart() {
  std::vector<std::pair<float, float>> sin_pts, cos_pts, tan_pts;
  constexpr int kN = 120;
  for (int i = 0; i <= kN; ++i) {
    float x = (static_cast<float>(i) / static_cast<float>(kN)) * 2.f * kPi;
    sin_pts.emplace_back(x, std::sin(x));
    cos_pts.emplace_back(x, std::cos(x));
    // Clamp tan to [-2, 2] so the chart stays readable.
    float t = std::tan(x);
    if (t > 2.f) {
      t = 2.f;
    }
    if (t < -2.f) {
      t = -2.f;
    }
    tan_pts.emplace_back(x, t);
  }

  return LineChart()
      .Title("Trigonometric Functions  [0 … 2π]")
      .Series("sin(x)", sin_pts, Color::Cyan)
      .Series("cos(x)", cos_pts, Color::Yellow)
      .Series("tan(x)", tan_pts, Color::Red)
      .Domain(0.f, 2.f * kPi)
      .Range(-2.2f, 2.2f)
      .ShowLegend(true)
      .ShowAxes(true)
      .ShowGrid(true)
      .Build();
}

// ── BarChart — monthly revenue ────────────────────────────────────────────────

Component MakeBarChart() {
  return BarChart()
      .Title("Monthly Revenue  (×1000 $)")
      .Bar("Jan", 42.f, Color::Cyan)
      .Bar("Feb", 55.f, Color::Blue)
      .Bar("Mar", 38.f, Color::GreenLight)
      .Bar("Apr", 70.f, Color::Yellow)
      .Bar("May", 63.f, Color::Magenta)
      .Bar("Jun", 81.f, Color::Red)
      .Horizontal(true)
      .ShowValues(true)
      .Build();
}

// ── ScatterPlot — two clusters ────────────────────────────────────────────────

Component MakeScatterPlot() {
  // Cluster A — centred at (2, 3)
  std::vector<std::pair<float, float>> a = {
      {1.8f, 3.1f}, {2.1f, 2.9f}, {2.3f, 3.4f}, {1.7f, 2.7f}, {2.0f, 3.2f},
      {2.5f, 3.0f}, {1.9f, 3.6f}, {2.2f, 2.8f}, {1.6f, 3.3f}, {2.4f, 3.1f},
      {2.0f, 2.5f}, {1.5f, 3.0f}, {2.6f, 3.5f}, {2.1f, 3.8f}, {1.8f, 2.6f},
  };
  // Cluster B — centred at (5, 7)
  std::vector<std::pair<float, float>> b = {
      {4.8f, 7.1f}, {5.2f, 6.8f}, {5.0f, 7.4f}, {4.7f, 7.0f}, {5.3f, 7.2f},
      {4.9f, 6.6f}, {5.1f, 7.5f}, {5.4f, 7.1f}, {4.6f, 6.9f}, {5.0f, 7.3f},
      {5.5f, 6.7f}, {4.8f, 7.6f}, {5.2f, 7.0f}, {4.5f, 7.2f}, {5.1f, 6.5f},
  };

  return ScatterPlot()
      .Title("Two Gaussian Clusters")
      .Series("Cluster A", a, Color::Cyan)
      .Series("Cluster B", b, Color::Yellow)
      .ShowAxes(true)
      .ShowGrid(true)
      .Build();
}

// ── Histogram — normal-ish distribution ──────────────────────────────────────

Component MakeHistogram() {
  // Hand-crafted samples that approximate N(5, 1.5²).
  std::vector<float> data = {
      3.2f, 3.5f, 3.7f, 3.8f, 4.0f, 4.1f, 4.1f, 4.2f, 4.3f, 4.3f,
      4.4f, 4.5f, 4.5f, 4.6f, 4.6f, 4.7f, 4.7f, 4.8f, 4.8f, 4.9f,
      4.9f, 5.0f, 5.0f, 5.0f, 5.1f, 5.1f, 5.1f, 5.2f, 5.2f, 5.3f,
      5.3f, 5.4f, 5.4f, 5.5f, 5.5f, 5.6f, 5.6f, 5.7f, 5.8f, 5.9f,
      6.0f, 6.1f, 6.2f, 6.3f, 6.4f, 6.5f, 6.6f, 6.8f, 7.0f, 7.4f,
  };

  return Histogram()
      .Data(data)
      .Bins(12)
      .Title("Distribution  N(5, 1.5²)")
      .Color(Color::GreenLight)
      .Build();
}

// ── Sparkline strip ───────────────────────────────────────────────────────────

Element MakeSparklineRow() {
  std::vector<float> cpu = {0.4f, 0.5f, 0.6f, 0.7f, 0.5f, 0.8f,
                             0.9f, 0.6f, 0.4f, 0.5f, 0.3f, 0.7f,
                             0.6f, 0.8f, 0.7f, 0.5f, 0.4f, 0.6f,
                             0.9f, 0.7f};
  std::vector<float> mem = {0.6f, 0.6f, 0.7f, 0.7f, 0.7f, 0.8f,
                             0.8f, 0.8f, 0.9f, 0.9f, 0.9f, 0.9f,
                             0.8f, 0.8f, 0.7f, 0.7f, 0.8f, 0.8f,
                             0.9f, 0.9f};

  return hbox({
      text("CPU: ") | dim,
      Sparkline(cpu, 20, Color::Cyan),
      text("  MEM: ") | dim,
      Sparkline(mem, 20, Color::Yellow),
  });
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  auto line_chart = MakeLineChart();
  auto bar_chart = MakeBarChart();
  auto scatter = MakeScatterPlot();
  auto histogram = MakeHistogram();

  // Wrap each chart in a titled panel.
  auto top_left = Panel("Line Chart", line_chart);
  auto top_right = Panel("Bar Chart", bar_chart);
  auto bot_left = Panel("Scatter Plot", scatter);
  auto bot_right = Panel("Histogram", histogram);

  // Assemble: 2×2 grid
  auto top_row_c = Container::Horizontal({top_left, top_right});
  auto bot_row_c = Container::Horizontal({bot_left, bot_right});
  auto grid_c = Container::Vertical({top_row_c, bot_row_c});

  auto root = Renderer(grid_c, [&]() -> Element {
    return vbox({
        text("  ftxui::ui  Chart Suite Demo") | bold | hcenter |
            color(GetTheme().primary),
        separatorEmpty(),
        hbox({top_left->Render() | flex, top_right->Render() | flex}) | flex,
        hbox({bot_left->Render() | flex, bot_right->Render() | flex}) | flex,
        separator(),
        MakeSparklineRow() | hcenter,
        text("press  q / Ctrl-C  to quit") | dim | hcenter,
    }) |
        flex;
  });

  // Handle 'q' to quit.
  auto handled = CatchEvent(root, [](Event e) {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      App::Active()->Exit();
      return true;
    }
    return false;
  });

  RunFullscreen(handled);
  return 0;
}
