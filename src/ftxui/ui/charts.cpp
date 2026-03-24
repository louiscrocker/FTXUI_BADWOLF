// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/charts.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/canvas.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

namespace {

// Return a color from the theme palette by index (cycles).
Color ThemeColor(int index) {
  const Theme& t = GetTheme();
  const std::array<Color, 5> palette = {
      t.primary, t.secondary, t.accent, t.error_color, t.warning_color,
  };
  return palette[static_cast<size_t>(index) % palette.size()];
}

// Resolve Color::Default to a theme palette color by series index.
Color ResolveColor(Color c, int index) {
  if (c == Color::Default) {
    return ThemeColor(index);
  }
  return c;
}

// Collect all points from multiple series into one vector.
template <typename SeriesEntry>
std::vector<std::pair<float, float>> AllPoints(
    const std::vector<SeriesEntry>& series) {
  std::vector<std::pair<float, float>> all;
  for (const auto& s : series) {
    all.insert(all.end(), s.points.begin(), s.points.end());
  }
  return all;
}

}  // namespace

// ── Sparkline
// ─────────────────────────────────────────────────────────────────

Element Sparkline(const std::vector<float>& values,
                  int width_chars,
                  Color color) {
  if (values.empty()) {
    return text(std::string(static_cast<size_t>(width_chars), ' '));
  }

  // height = 1 character row = 4 braille dot rows
  BrailleCanvas bc(width_chars, 1);

  std::vector<std::pair<float, float>> pts;
  pts.reserve(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    pts.emplace_back(static_cast<float>(i), values[i]);
  }
  bc.AutoFit(pts).Plot(pts, color);
  return bc.Render();
}

// ── LineChart
// ─────────────────────────────────────────────────────────────────

LineChart& LineChart::Series(std::string label,
                             std::vector<float> values,
                             Color color) {
  std::vector<std::pair<float, float>> pts;
  pts.reserve(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    pts.emplace_back(static_cast<float>(i), values[i]);
  }
  return Series(std::move(label), std::move(pts), color);
}

LineChart& LineChart::Series(std::string label,
                             std::vector<std::pair<float, float>> points,
                             Color color) {
  series_.push_back({std::move(label), std::move(points), color});
  return *this;
}

LineChart& LineChart::Title(std::string title) {
  title_ = std::move(title);
  return *this;
}

LineChart& LineChart::XLabel(std::string label) {
  xlabel_ = std::move(label);
  return *this;
}

LineChart& LineChart::YLabel(std::string label) {
  ylabel_ = std::move(label);
  return *this;
}

LineChart& LineChart::ShowLegend(bool v) {
  show_legend_ = v;
  return *this;
}

LineChart& LineChart::ShowAxes(bool v) {
  show_axes_ = v;
  return *this;
}

LineChart& LineChart::ShowGrid(bool v) {
  show_grid_ = v;
  return *this;
}

LineChart& LineChart::Domain(float xmin, float xmax) {
  has_domain_ = true;
  xmin_ = xmin;
  xmax_ = xmax;
  return *this;
}

LineChart& LineChart::Range(float ymin, float ymax) {
  has_range_ = true;
  ymin_ = ymin;
  ymax_ = ymax;
  return *this;
}

Component LineChart::Build() {
  // Capture by value so the component is self-contained.
  auto series = series_;
  auto title = title_;
  auto xlabel = xlabel_;
  auto ylabel = ylabel_;
  bool show_legend = show_legend_;
  bool show_axes = show_axes_;
  bool show_grid = show_grid_;
  bool has_domain = has_domain_;
  bool has_range = has_range_;
  float xmin = xmin_, xmax = xmax_;
  float ymin = ymin_, ymax = ymax_;

  return Renderer([=]() -> Element {
    // Canvas occupies most of the cell — we use a fixed size that looks good.
    // The actual rendering size adapts at Render() time when using
    // canvas(fn) without explicit width/height, but we want a consistent
    // coordinate mapping so we pick reasonable defaults.
    constexpr int kW = 20;
    constexpr int kH = 8;

    BrailleCanvas bc(kW, kH);

    // Determine domain/range.
    if (!has_domain || !has_range) {
      auto all = AllPoints(series);
      if (!all.empty()) {
        bc.AutoFit(all);
      }
    }
    if (has_domain) {
      bc.Domain(xmin, xmax);
    }
    if (has_range) {
      bc.Range(ymin, ymax);
    }

    if (show_grid) {
      bc.Grid(4, 4, GetTheme().border_color);
    }
    if (show_axes) {
      bc.Axes(GetTheme().text_muted);
    }

    for (size_t i = 0; i < series.size(); ++i) {
      Color c = ResolveColor(series[i].color, static_cast<int>(i));
      bc.Plot(series[i].points, c);
    }

    Element chart_elem = bc.Render();

    // Build legend.
    Elements legend_entries;
    if (show_legend && !series.empty()) {
      for (size_t i = 0; i < series.size(); ++i) {
        Color c = ResolveColor(series[i].color, static_cast<int>(i));
        legend_entries.push_back(
            hbox({text("━━ ") | color(c),
                  text(series[i].label) | color(GetTheme().text)}));
      }
    }

    // Compose the full element.
    Elements rows;
    if (!title.empty()) {
      rows.push_back(text(title) | bold | hcenter | color(GetTheme().primary));
    }

    Element main_row = chart_elem | flex;
    if (show_legend && !legend_entries.empty()) {
      Elements legend_col;
      legend_col.push_back(filler());
      for (auto& e : legend_entries) {
        legend_col.push_back(e);
      }
      legend_col.push_back(filler());
      main_row = hbox({chart_elem | flex, separatorEmpty(),
                       vbox(std::move(legend_col)) | size(WIDTH, EQUAL, 16)});
    }
    rows.push_back(main_row | flex);

    if (!xlabel.empty()) {
      rows.push_back(text(xlabel) | hcenter | dim);
    }

    // Add ylabel on the left if requested (simple: prepend a rotated label
    // isn't trivially possible in FTXUI, so we use a dim side text).
    Element result = vbox(std::move(rows)) | flex;
    if (!ylabel.empty()) {
      result = hbox({text(ylabel) | dim | vcenter, result | flex});
    }
    return result;
  });
}

// ── BarChart
// ──────────────────────────────────────────────────────────────────

BarChart& BarChart::Bar(std::string label, float value, Color color) {
  bars_.push_back({std::move(label), value, color});
  return *this;
}

BarChart& BarChart::Title(std::string title) {
  title_ = std::move(title);
  return *this;
}

BarChart& BarChart::Horizontal(bool v) {
  horizontal_ = v;
  return *this;
}

BarChart& BarChart::ShowValues(bool v) {
  show_values_ = v;
  return *this;
}

BarChart& BarChart::MaxValue(float v) {
  has_max_ = true;
  max_value_ = v;
  return *this;
}

Component BarChart::Build() {
  auto bars = bars_;
  auto title = title_;
  bool horizontal = horizontal_;
  bool show_values = show_values_;
  bool has_max = has_max_;
  float max_value = max_value_;

  return Renderer([=]() -> Element {
    // Compute max for normalisation.
    float mv = 0.f;
    for (const auto& b : bars) {
      mv = std::max(mv, b.value);
    }
    if (has_max) {
      mv = max_value;
    }
    if (mv == 0.f) {
      mv = 1.f;
    }

    Elements rows;
    if (!title.empty()) {
      rows.push_back(text(title) | bold | hcenter | color(GetTheme().primary));
      rows.push_back(separatorEmpty());
    }

    for (size_t i = 0; i < bars.size(); ++i) {
      Color c = ResolveColor(bars[i].color, static_cast<int>(i));
      float ratio = std::clamp(bars[i].value / mv, 0.f, 1.f);

      if (horizontal) {
        // Horizontal bar: label | gauge | value
        std::string val_str =
            show_values
                ? (" " + std::to_string(static_cast<int>(bars[i].value)))
                : "";
        rows.push_back(hbox({
            text(bars[i].label) | size(WIDTH, EQUAL, 8),
            text(" │") | dim,
            gauge(ratio) | color(c) | flex,
            text(val_str) | color(c),
        }));
      } else {
        // Vertical bar: drawn via BrailleCanvas column
        // (We still use a gauge-based column for simplicity.)
        rows.push_back(hbox({
            text(bars[i].label) | size(WIDTH, EQUAL, 8),
            text(" "),
            gauge(ratio) | color(c) | flex,
            show_values
                ? (text(" " + std::to_string(static_cast<int>(bars[i].value))) |
                   color(c) | size(WIDTH, EQUAL, 7))
                : text(""),
        }));
      }
    }

    return vbox(std::move(rows)) | flex;
  });
}

// ── ScatterPlot
// ───────────────────────────────────────────────────────────────

ScatterPlot& ScatterPlot::Series(std::string label,
                                 std::vector<std::pair<float, float>> points,
                                 Color color) {
  series_.push_back({std::move(label), std::move(points), color});
  return *this;
}

ScatterPlot& ScatterPlot::Title(std::string title) {
  title_ = std::move(title);
  return *this;
}

ScatterPlot& ScatterPlot::ShowAxes(bool v) {
  show_axes_ = v;
  return *this;
}

ScatterPlot& ScatterPlot::ShowGrid(bool v) {
  show_grid_ = v;
  return *this;
}

Component ScatterPlot::Build() {
  auto series = series_;
  auto title = title_;
  bool show_axes = show_axes_;
  bool show_grid = show_grid_;

  return Renderer([=]() -> Element {
    constexpr int kW = 20;
    constexpr int kH = 8;

    BrailleCanvas bc(kW, kH);

    auto all = AllPoints(series);
    if (!all.empty()) {
      bc.AutoFit(all);
    }

    if (show_grid) {
      bc.Grid(4, 4, GetTheme().border_color);
    }
    if (show_axes) {
      bc.Axes(GetTheme().text_muted);
    }

    for (size_t i = 0; i < series.size(); ++i) {
      Color c = ResolveColor(series[i].color, static_cast<int>(i));
      bc.Scatter(series[i].points, c);
    }

    Element chart_elem = bc.Render();

    // Build legend.
    Elements legend_entries;
    for (size_t i = 0; i < series.size(); ++i) {
      Color c = ResolveColor(series[i].color, static_cast<int>(i));
      legend_entries.push_back(hbox({
          text("• ") | color(c),
          text(series[i].label) | color(GetTheme().text),
      }));
    }

    Elements rows;
    if (!title.empty()) {
      rows.push_back(text(title) | bold | hcenter | color(GetTheme().primary));
    }

    Element main_row = chart_elem | flex;
    if (!legend_entries.empty()) {
      Elements legend_col;
      legend_col.push_back(filler());
      for (auto& e : legend_entries) {
        legend_col.push_back(e);
      }
      legend_col.push_back(filler());
      main_row = hbox({chart_elem | flex, separatorEmpty(),
                       vbox(std::move(legend_col)) | size(WIDTH, EQUAL, 14)});
    }
    rows.push_back(main_row | flex);

    return vbox(std::move(rows)) | flex;
  });
}

// ── Histogram
// ─────────────────────────────────────────────────────────────────

Histogram& Histogram::Data(std::vector<float> values) {
  data_ = std::move(values);
  return *this;
}

Histogram& Histogram::Bins(int n) {
  bins_ = n;
  return *this;
}

Histogram& Histogram::Title(std::string title) {
  title_ = std::move(title);
  return *this;
}

Histogram& Histogram::Color(ftxui::Color c) {
  color_ = c;
  return *this;
}

Component Histogram::Build() {
  auto data = data_;
  int bins = bins_;
  auto title = title_;
  ftxui::Color bar_col = color_;

  return Renderer([=]() -> Element {
    if (data.empty()) {
      return text("(no data)") | dim;
    }

    float lo = *std::min_element(data.begin(), data.end());
    float hi = *std::max_element(data.begin(), data.end());
    if (lo == hi) {
      hi = lo + 1.f;
    }
    float step = (hi - lo) / static_cast<float>(bins);

    std::vector<int> counts(static_cast<size_t>(bins), 0);
    for (float v : data) {
      int idx = static_cast<int>((v - lo) / step);
      idx = std::clamp(idx, 0, bins - 1);
      counts[static_cast<size_t>(idx)]++;
    }

    int max_count = *std::max_element(counts.begin(), counts.end());
    if (max_count == 0) {
      max_count = 1;
    }

    ftxui::Color resolved =
        (bar_col == ftxui::Color::Default) ? GetTheme().primary : bar_col;

    Elements rows;
    if (!title.empty()) {
      rows.push_back(text(title) | bold | hcenter |
                     ftxui::color(GetTheme().primary));
      rows.push_back(separatorEmpty());
    }

    for (int i = 0; i < bins; ++i) {
      float bin_lo = lo + static_cast<float>(i) * step;
      float ratio = static_cast<float>(counts[static_cast<size_t>(i)]) /
                    static_cast<float>(max_count);

      char label_buf[16];
      std::snprintf(label_buf, sizeof(label_buf), "%6.1f", bin_lo);

      rows.push_back(hbox({
          text(std::string(label_buf)) | dim | size(WIDTH, EQUAL, 7),
          text(" │") | dim,
          gauge(ratio) | ftxui::color(resolved) | flex,
          text(" " + std::to_string(counts[static_cast<size_t>(i)])) |
              size(WIDTH, EQUAL, 5),
      }));
    }

    return vbox(std::move(rows)) | flex;
  });
}

}  // namespace ftxui::ui
