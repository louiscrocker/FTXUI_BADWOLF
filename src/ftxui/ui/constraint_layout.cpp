// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/constraint_layout.hpp"

#include <algorithm>
#include <climits>
#include <numeric>
#include <utility>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/terminal.hpp"

namespace ftxui::ui {

// ── SizeConstraint
// ────────────────────────────────────────────────────────────

SizeConstraint SizeConstraint::Chars(int n) {
  return {Unit::Chars, static_cast<float>(n)};
}

SizeConstraint SizeConstraint::Percent(float pct) {
  return {Unit::Percent, pct};
}

SizeConstraint SizeConstraint::Fill() {
  return {Unit::Fill, 0};
}

SizeConstraint SizeConstraint::Auto() {
  return {Unit::Auto, 0};
}

// ── Constraints
// ───────────────────────────────────────────────────────────────

Constraints& Constraints::Padding(int all) {
  pad_left = pad_right = pad_top = pad_bottom = all;
  return *this;
}

Constraints& Constraints::PaddingH(int h) {
  pad_left = pad_right = h;
  return *this;
}

Constraints& Constraints::PaddingV(int v) {
  pad_top = pad_bottom = v;
  return *this;
}

// ── Internal helpers
// ──────────────────────────────────────────────────────────

namespace {

// Resolve a single constraint given available space (returns -1 for Fill/Auto).
int ResolveConstraint(const SizeConstraint& sc, int available) {
  switch (sc.unit) {
    case SizeConstraint::Unit::Chars:
      return std::min(static_cast<int>(sc.value), available);
    case SizeConstraint::Unit::Percent:
      return static_cast<int>(available * sc.value / 100.0f);
    case SizeConstraint::Unit::Fill:
    case SizeConstraint::Unit::Auto:
      return -1;  // deferred
  }
  return -1;
}

// Clamp an integer between lo and hi.
int Clamp(int v, int lo, int hi) {
  return std::max(lo, std::min(v, hi));
}

// Apply Constraints to an element (width axis).
ftxui::Element ApplyWidthConstraints(ftxui::Element el,
                                     const Constraints& c,
                                     int available) {
  int w = ResolveConstraint(c.width, available);
  if (w == -1) {
    // Auto / Fill: respect min/max but don't force a specific width.
    if (c.min_width > 0) {
      el = el | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, c.min_width);
    }
    if (c.max_width < INT_MAX) {
      el = el | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, c.max_width);
    }
  } else {
    w = Clamp(w, c.min_width, c.max_width);
    el = el | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, w);
  }
  if (c.pad_left || c.pad_right || c.pad_top || c.pad_bottom) {
    el = ftxui::hbox({
        ftxui::text(std::string(c.pad_left, ' ')),
        ftxui::vbox({
            ftxui::text(std::string(c.pad_top, '\n')),
            el,
            ftxui::text(std::string(c.pad_bottom, '\n')),
        }),
        ftxui::text(std::string(c.pad_right, ' ')),
    });
  }
  return el;
}

// Apply Constraints to an element (height axis).
ftxui::Element ApplyHeightConstraints(ftxui::Element el,
                                      const Constraints& c,
                                      int available) {
  int h = ResolveConstraint(c.height, available);
  if (h == -1) {
    if (c.min_height > 0) {
      el = el | ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, c.min_height);
    }
    if (c.max_height < INT_MAX) {
      el = el | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, c.max_height);
    }
  } else {
    h = Clamp(h, c.min_height, c.max_height);
    el = el | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, h);
  }
  return el;
}

}  // namespace

// ── ConstraintBox
// ─────────────────────────────────────────────────────────────

ftxui::Element ConstraintBox(ftxui::Element inner, Constraints c) {
  // Resolve at render time using terminal width for percentage/fill.
  auto dims = ftxui::Terminal::Size();
  inner = ApplyWidthConstraints(inner, c, dims.dimx);
  inner = ApplyHeightConstraints(inner, c, dims.dimy);
  return inner;
}

ftxui::Component ConstraintBox(ftxui::Component inner, Constraints c) {
  return ftxui::Renderer(inner, [inner, c]() -> ftxui::Element {
    return ConstraintBox(inner->Render(), c);
  });
}

// ── ConstraintRow
// ─────────────────────────────────────────────────────────────

ftxui::Element ConstraintRow(std::vector<ConstraintChild> children) {
  if (children.empty()) {
    return ftxui::hbox(ftxui::Elements{});
  }

  int available = ftxui::Terminal::Size().dimx;

  // Pass 1: resolve non-Fill/Auto constraints.
  std::vector<int> resolved(children.size(), -1);
  int used = 0;
  int fill_count = 0;

  for (size_t i = 0; i < children.size(); ++i) {
    const auto& c = children[i].constraints;
    int w = ResolveConstraint(c.width, available);
    if (w >= 0) {
      w = Clamp(w, c.min_width, c.max_width);
      resolved[i] = w;
      used += w;
    } else {
      fill_count++;
    }
  }

  // Pass 2: distribute remaining space among Fill/Auto children.
  int remaining = std::max(0, available - used);
  int fill_each = fill_count > 0 ? remaining / fill_count : 0;
  int fill_extra = fill_count > 0 ? remaining % fill_count : 0;
  int fill_idx = 0;

  ftxui::Elements elems;
  for (size_t i = 0; i < children.size(); ++i) {
    ftxui::Element el = children[i].element;
    const auto& c = children[i].constraints;
    int w = resolved[i];
    if (w < 0) {
      // Fill / Auto: give equal share of remainder.
      w = fill_each + (fill_idx < fill_extra ? 1 : 0);
      w = Clamp(w, c.min_width, c.max_width);
      fill_idx++;
    }
    if (w > 0) {
      el = el | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, w);
    }
    elems.push_back(el);
  }

  return ftxui::hbox(std::move(elems));
}

// ── ConstraintColumn
// ──────────────────────────────────────────────────────────

ftxui::Element ConstraintColumn(std::vector<ConstraintChild> children) {
  if (children.empty()) {
    return ftxui::vbox(ftxui::Elements{});
  }

  int available = ftxui::Terminal::Size().dimy;

  std::vector<int> resolved(children.size(), -1);
  int used = 0;
  int fill_count = 0;

  for (size_t i = 0; i < children.size(); ++i) {
    const auto& c = children[i].constraints;
    int h = ResolveConstraint(c.height, available);
    if (h >= 0) {
      h = Clamp(h, c.min_height, c.max_height);
      resolved[i] = h;
      used += h;
    } else {
      fill_count++;
    }
  }

  int remaining = std::max(0, available - used);
  int fill_each = fill_count > 0 ? remaining / fill_count : 0;
  int fill_extra = fill_count > 0 ? remaining % fill_count : 0;
  int fill_idx = 0;

  ftxui::Elements elems;
  for (size_t i = 0; i < children.size(); ++i) {
    ftxui::Element el = children[i].element;
    const auto& c = children[i].constraints;
    int h = resolved[i];
    if (h < 0) {
      h = fill_each + (fill_idx < fill_extra ? 1 : 0);
      h = Clamp(h, c.min_height, c.max_height);
      fill_idx++;
    }
    if (h > 0) {
      el = el | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, h);
    }
    elems.push_back(el);
  }

  return ftxui::vbox(std::move(elems));
}

// ── ConstraintGrid
// ────────────────────────────────────────────────────────────

ConstraintGrid& ConstraintGrid::Columns(
    std::vector<SizeConstraint> col_widths) {
  col_widths_ = std::move(col_widths);
  return *this;
}

ConstraintGrid& ConstraintGrid::Columns(int count, SizeConstraint each) {
  col_widths_.assign(count, each);
  return *this;
}

ConstraintGrid& ConstraintGrid::Gap(int h_gap, int v_gap) {
  h_gap_ = h_gap;
  v_gap_ = v_gap;
  return *this;
}

ConstraintGrid& ConstraintGrid::Add(ftxui::Element el,
                                    int col_span,
                                    int row_span) {
  cells_.push_back({std::move(el), col_span, row_span});
  return *this;
}

ConstraintGrid& ConstraintGrid::Add(ftxui::Component c, int col_span) {
  cells_.push_back({c->Render(), col_span, 1});
  return *this;
}

ftxui::Element ConstraintGrid::Build() const {
  if (col_widths_.empty() || cells_.empty()) {
    return ftxui::vbox(ftxui::Elements{});
  }

  int num_cols = static_cast<int>(col_widths_.size());
  int available = ftxui::Terminal::Size().dimx;

  // Resolve column widths (same two-pass as ConstraintRow).
  std::vector<int> col_px(num_cols, -1);
  int used = 0;
  int fill_count = 0;
  for (int c = 0; c < num_cols; ++c) {
    int w = ResolveConstraint(col_widths_[c], available);
    if (w >= 0) {
      col_px[c] = w;
      used += w;
    } else {
      fill_count++;
    }
  }
  // Account for horizontal gaps in available space.
  int total_h_gap = h_gap_ * (num_cols - 1);
  int remaining = std::max(0, available - used - total_h_gap);
  int fill_each = fill_count > 0 ? remaining / fill_count : 0;
  int fill_extra = fill_count > 0 ? remaining % fill_count : 0;
  int fill_idx = 0;
  for (int c = 0; c < num_cols; ++c) {
    if (col_px[c] < 0) {
      col_px[c] = fill_each + (fill_idx < fill_extra ? 1 : 0);
      fill_idx++;
    }
  }

  // Place cells into rows.
  ftxui::Elements rows;
  int cell_idx = 0;
  while (cell_idx < static_cast<int>(cells_.size())) {
    ftxui::Elements row_elems;
    int col = 0;
    while (col < num_cols && cell_idx < static_cast<int>(cells_.size())) {
      const auto& cell = cells_[cell_idx];
      int span = std::min(cell.col_span, num_cols - col);
      // Sum column widths for spanned columns plus internal gaps.
      int cell_w = 0;
      for (int s = 0; s < span; ++s) {
        cell_w += col_px[col + s];
        if (s > 0) {
          cell_w += h_gap_;
        }
      }
      ftxui::Element el =
          cell.element | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, cell_w);
      row_elems.push_back(el);
      if (h_gap_ > 0 && col + span < num_cols) {
        row_elems.push_back(ftxui::text(std::string(h_gap_, ' ')));
      }
      col += span;
      cell_idx++;
    }
    // Fill empty trailing columns.
    while (col < num_cols) {
      row_elems.push_back(ftxui::text("") |
                          ftxui::size(ftxui::WIDTH, ftxui::EQUAL, col_px[col]));
      col++;
    }
    rows.push_back(ftxui::hbox(std::move(row_elems)));
    if (v_gap_ > 0 && cell_idx < static_cast<int>(cells_.size())) {
      for (int g = 0; g < v_gap_; ++g) {
        rows.push_back(ftxui::text(""));
      }
    }
  }
  return ftxui::vbox(std::move(rows));
}

ftxui::Component ConstraintGrid::BuildComponent() const {
  auto copy = *this;
  return ftxui::Renderer([copy]() -> ftxui::Element { return copy.Build(); });
}

// ── Responsive
// ────────────────────────────────────────────────────────────────

Responsive& Responsive::At(int min_cols, ftxui::Element el) {
  breakpoints_.push_back({min_cols, [el]() -> ftxui::Element { return el; }});
  return *this;
}

Responsive& Responsive::At(int min_cols,
                           std::function<ftxui::Element()> factory) {
  breakpoints_.push_back({min_cols, std::move(factory)});
  return *this;
}

Responsive& Responsive::XS(ftxui::Element el) {
  return At(0, std::move(el));
}
Responsive& Responsive::SM(ftxui::Element el) {
  return At(60, std::move(el));
}
Responsive& Responsive::MD(ftxui::Element el) {
  return At(80, std::move(el));
}
Responsive& Responsive::LG(ftxui::Element el) {
  return At(120, std::move(el));
}
Responsive& Responsive::XL(ftxui::Element el) {
  return At(180, std::move(el));
}

ftxui::Element Responsive::Build() const {
  int width = ftxui::Terminal::Size().dimx;

  // Select the highest breakpoint that fits.
  const Breakpoint* best = nullptr;
  for (const auto& bp : breakpoints_) {
    if (width >= bp.min_cols) {
      if (!best || bp.min_cols >= best->min_cols) {
        best = &bp;
      }
    }
  }
  if (best) {
    return best->factory();
  }

  // Fallback: use lowest breakpoint if no breakpoint matches (width < all
  // mins).
  if (!breakpoints_.empty()) {
    const Breakpoint* lowest = &breakpoints_[0];
    for (const auto& bp : breakpoints_) {
      if (bp.min_cols < lowest->min_cols) {
        lowest = &bp;
      }
    }
    return lowest->factory();
  }

  return ftxui::text("");
}

ftxui::Component Responsive::BuildComponent() const {
  auto copy = *this;
  return ftxui::Renderer([copy]() -> ftxui::Element { return copy.Build(); });
}

// ── AspectRatio
// ───────────────────────────────────────────────────────────────

ftxui::Element AspectRatio(ftxui::Element inner, int w_units, int h_units) {
  // Terminal cells are roughly 2:1 height-to-width in pixels, so we halve
  // the height in character rows to approximate a square-ish result.
  int terminal_w = ftxui::Terminal::Size().dimx;
  // Use terminal width as the reference width.
  int width = terminal_w;
  // height in rows ≈ width * (h_units / w_units) / 2 (cell aspect correction)
  int height = static_cast<int>(width * static_cast<float>(h_units) /
                                static_cast<float>(w_units) / 2.0f);
  height = std::max(1, height);
  return inner | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width) |
         ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, height);
}

// ── Stack
// ─────────────────────────────────────────────────────────────────────

ftxui::Element Stack(std::vector<ftxui::Element> layers) {
  return ftxui::dbox(std::move(layers));
}

// ── Spacer
// ────────────────────────────────────────────────────────────────────

ftxui::Element Spacer(int chars) {
  return ftxui::text(std::string(chars, ' '));
}

ftxui::Element FlexSpacer() {
  return ftxui::filler();
}

// ── Center
// ────────────────────────────────────────────────────────────────────

ftxui::Element CenterH(ftxui::Element inner) {
  return ftxui::hbox({ftxui::filler(), std::move(inner), ftxui::filler()});
}

ftxui::Element CenterV(ftxui::Element inner) {
  return ftxui::vbox({ftxui::filler(), std::move(inner), ftxui::filler()});
}

ftxui::Element CenterBoth(ftxui::Element inner) {
  return CenterV(CenterH(std::move(inner)));
}

}  // namespace ftxui::ui
