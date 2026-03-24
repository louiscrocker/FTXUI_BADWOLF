// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

BrailleCanvas::BrailleCanvas(int width_chars, int height_chars)
    : w_chars_(width_chars), h_chars_(height_chars) {}

BrailleCanvas& BrailleCanvas::Domain(float xmin, float xmax) {
  xmin_ = xmin;
  xmax_ = xmax;
  return *this;
}

BrailleCanvas& BrailleCanvas::Range(float ymin, float ymax) {
  ymin_ = ymin;
  ymax_ = ymax;
  return *this;
}

BrailleCanvas& BrailleCanvas::AutoFit(
    const std::vector<std::pair<float, float>>& pts) {
  if (pts.empty()) {
    return *this;
  }
  float xlo = std::numeric_limits<float>::max();
  float xhi = std::numeric_limits<float>::lowest();
  float ylo = std::numeric_limits<float>::max();
  float yhi = std::numeric_limits<float>::lowest();
  for (const auto& [x, y] : pts) {
    xlo = std::min(xlo, x);
    xhi = std::max(xhi, x);
    ylo = std::min(ylo, y);
    yhi = std::max(yhi, y);
  }
  float xpad = (xhi - xlo) * 0.05f;
  float ypad = (yhi - ylo) * 0.05f;
  if (xpad == 0.f) {
    xpad = 0.5f;
  }
  if (ypad == 0.f) {
    ypad = 0.5f;
  }
  xmin_ = xlo - xpad;
  xmax_ = xhi + xpad;
  ymin_ = ylo - ypad;
  ymax_ = yhi + ypad;
  return *this;
}

BrailleCanvas& BrailleCanvas::Plot(
    const std::vector<std::pair<float, float>>& pts,
    ftxui::Color color) {
  cmds_.push_back([this, pts, color](ftxui::Canvas& c, int w, int h) {
    if (pts.size() < 2) {
      for (const auto& [x, y] : pts) {
        c.DrawPoint(ToPixX(x, w), ToPixY(y, h), true, color);
      }
      return;
    }
    for (size_t i = 1; i < pts.size(); ++i) {
      c.DrawPointLine(ToPixX(pts[i - 1].first, w), ToPixY(pts[i - 1].second, h),
                      ToPixX(pts[i].first, w), ToPixY(pts[i].second, h), color);
    }
  });
  return *this;
}

BrailleCanvas& BrailleCanvas::Scatter(
    const std::vector<std::pair<float, float>>& pts,
    ftxui::Color color) {
  cmds_.push_back([this, pts, color](ftxui::Canvas& c, int w, int h) {
    for (const auto& [x, y] : pts) {
      int px = ToPixX(x, w);
      int py = ToPixY(y, h);
      c.DrawPoint(px, py, true, color);
      // Draw a small cross for visibility.
      c.DrawPoint(px - 1, py, true, color);
      c.DrawPoint(px + 1, py, true, color);
      c.DrawPoint(px, py - 1, true, color);
      c.DrawPoint(px, py + 1, true, color);
    }
  });
  return *this;
}

BrailleCanvas& BrailleCanvas::DrawFunction(std::function<float(float)> fn,
                                            int samples,
                                            ftxui::Color color) {
  std::vector<std::pair<float, float>> pts;
  pts.reserve(samples);
  for (int i = 0; i < samples; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(samples - 1);
    float x = xmin_ + t * (xmax_ - xmin_);
    float y = fn(x);
    pts.emplace_back(x, y);
  }
  return Plot(pts, color);
}

BrailleCanvas& BrailleCanvas::HLine(float y, ftxui::Color color) {
  cmds_.push_back([this, y, color](ftxui::Canvas& c, int w, int h) {
    int py = ToPixY(y, h);
    c.DrawPointLine(0, py, w - 1, py, color);
  });
  return *this;
}

BrailleCanvas& BrailleCanvas::VLine(float x, ftxui::Color color) {
  cmds_.push_back([this, x, color](ftxui::Canvas& c, int w, int h) {
    int px = ToPixX(x, w);
    c.DrawPointLine(px, 0, px, h - 1, color);
  });
  return *this;
}

BrailleCanvas& BrailleCanvas::Axes(ftxui::Color color) {
  // Draw y-axis (x=0 if in domain, else left edge)
  float ax = (xmin_ <= 0.f && 0.f <= xmax_) ? 0.f : xmin_;
  // Draw x-axis (y=0 if in range, else bottom edge)
  float ay = (ymin_ <= 0.f && 0.f <= ymax_) ? 0.f : ymin_;
  VLine(ax, color);
  HLine(ay, color);
  return *this;
}

BrailleCanvas& BrailleCanvas::Grid(int h_lines,
                                    int v_lines,
                                    ftxui::Color color) {
  // Horizontal grid lines
  for (int i = 0; i <= h_lines; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(h_lines);
    float y = ymin_ + t * (ymax_ - ymin_);
    HLine(y, color);
  }
  // Vertical grid lines
  for (int i = 0; i <= v_lines; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(v_lines);
    float x = xmin_ + t * (xmax_ - xmin_);
    VLine(x, color);
  }
  return *this;
}

ftxui::Element BrailleCanvas::Render() const {
  int w_dots = w_chars_ * 2;
  int h_dots = h_chars_ * 4;

  // Capture everything needed by value.
  auto cmds_copy = cmds_;

  // The canvas(w, h, fn) element uses w/h as minimum-size hints, but at
  // Render() time the actual canvas passed to fn is sized to the allocated
  // box.  Pass the actual dimensions to each command so coordinate mapping
  // always reflects the real draw area.
  return ftxui::canvas(w_dots, h_dots, [cmds_copy](ftxui::Canvas& c) {
    int w = c.width();
    int h = c.height();
    for (const auto& cmd : cmds_copy) {
      cmd(c, w, h);
    }
  });
}

int BrailleCanvas::ToPixX(float x, int w_dots) const {
  if (xmax_ == xmin_) {
    return w_dots / 2;
  }
  float t = (x - xmin_) / (xmax_ - xmin_);
  int px = static_cast<int>(t * static_cast<float>(w_dots - 1) + 0.5f);
  return std::max(0, std::min(w_dots - 1, px));
}

int BrailleCanvas::ToPixY(float y, int h_dots) const {
  if (ymax_ == ymin_) {
    return h_dots / 2;
  }
  // Flip: logical ymax maps to dot row 0 (top), ymin maps to dot row h-1 (bot)
  float t = (y - ymin_) / (ymax_ - ymin_);
  int py = static_cast<int>((1.f - t) * static_cast<float>(h_dots - 1) + 0.5f);
  return std::max(0, std::min(h_dots - 1, py));
}

}  // namespace ftxui::ui
