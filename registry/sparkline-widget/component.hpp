// BadWolf Component: sparkline-widget v1.0.0
// Install: badwolf install sparkline-widget
//
// Self-contained sparkline widget — no external dependencies beyond FTXUI.
// Drop this header into your project and include it directly.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace badwolf {

/// @brief Renders a compact braille sparkline from a vector of doubles.
///
/// Values are auto-normalised to the [min, max] range of the data.
/// Each terminal cell encodes up to 4 rows via Unicode Braille patterns
/// (U+2800–U+28FF), giving 4× vertical resolution per character row.
///
/// @param data   Data points (any range; NaN/inf are treated as 0).
/// @param width  Width in terminal character cells.
/// @param color  Line colour (default Cyan).
/// @returns      A single-row ftxui::Element suitable for embedding anywhere.
inline ftxui::Element SparklineWidget(
    const std::vector<double>& data,
    int width = 20,
    ftxui::Color color = ftxui::Color::Cyan) {
  if (data.empty() || width <= 0) {
    return ftxui::text(std::string(static_cast<size_t>(width), ' '));
  }

  // ── Normalise ────────────────────────────────────────────────────────────
  double lo = data[0], hi = data[0];
  for (double v : data) {
    if (std::isfinite(v)) {
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
  }
  if (lo == hi) { hi = lo + 1.0; }

  auto normalise = [&](double v) -> double {
    if (!std::isfinite(v)) return 0.0;
    return (v - lo) / (hi - lo);  // [0, 1]
  };

  // ── Braille encoding ──────────────────────────────────────────────────────
  // Each braille cell is 2 columns × 4 rows.
  // Dot layout (bit positions in Unicode):
  //   col0  col1
  //    0     3   (row 0 — top)
  //    1     4
  //    2     5
  //    6     7   (row 3 — bottom)
  //
  // We use a single-row sparkline: each character cell gets one data sample.
  // We map the normalised value to one of 8 braille dot heights.

  // Braille characters for single-column filled bar (left column only).
  // Index = number of dots filled from bottom (0..4).
  static const char* const kBars[] = {
      " ",   // 0 — empty
      "⡀",  // 1 — bottom dot
      "⡄",  // 2
      "⡆",  // 3
      "⡇",  // 4 — full left column
  };

  // For a richer "line" feel we use both columns and pick the closest level.
  // Simple approach: map [0,1] → 0..7 (8 braille heights), use UTF-8 blocks.
  // We use ▁▂▃▄▅▆▇█ (U+2581–U+2588) — one character per sample, 8 heights.
  static const char* const kBlocks[] = {
      " ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█",
  };
  constexpr int kLevels = 8;

  // Resample data to `width` points.
  std::string result;
  result.reserve(static_cast<size_t>(width) * 4);  // up to 4 UTF-8 bytes each

  for (int i = 0; i < width; ++i) {
    // Map cell index → data index (nearest neighbour resampling).
    size_t di =
        static_cast<size_t>(static_cast<double>(i) *
                             static_cast<double>(data.size()) /
                             static_cast<double>(width));
    if (di >= data.size()) di = data.size() - 1;

    double norm = normalise(data[di]);
    int level = static_cast<int>(std::round(norm * kLevels));
    level = std::max(0, std::min(kLevels, level));
    result += kBlocks[level];
  }

  return ftxui::text(result) | ftxui::color(color);
}

}  // namespace badwolf
