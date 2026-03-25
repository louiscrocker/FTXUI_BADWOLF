// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_CONSTRAINT_LAYOUT_HPP
#define FTXUI_UI_CONSTRAINT_LAYOUT_HPP

#include <climits>
#include <functional>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

// ── Size constraint
// ───────────────────────────────────────────────────────────
struct SizeConstraint {
  enum class Unit { Chars, Percent, Fill, Auto };
  Unit unit = Unit::Auto;
  float value = 0;

  static SizeConstraint Chars(int n);
  static SizeConstraint Percent(float pct);
  static SizeConstraint Fill();
  static SizeConstraint Auto();
};

// ── Constraint spec for one element ──────────────────────────────────────────
struct Constraints {
  SizeConstraint width = SizeConstraint::Auto();
  SizeConstraint height = SizeConstraint::Auto();
  int min_width = 0;
  int max_width = INT_MAX;
  int min_height = 0;
  int max_height = INT_MAX;
  int grow = 0;
  int shrink = 1;

  int pad_left = 0, pad_right = 0, pad_top = 0, pad_bottom = 0;
  Constraints& Padding(int all);
  Constraints& PaddingH(int h);
  Constraints& PaddingV(int v);
};

// ── Responsive breakpoints
// ────────────────────────────────────────────────────
class Responsive {
 public:
  Responsive& At(int min_cols, ftxui::Element el);
  Responsive& At(int min_cols, std::function<ftxui::Element()> factory);

  Responsive& XS(ftxui::Element el);  // 0+  cols
  Responsive& SM(ftxui::Element el);  // 60+ cols
  Responsive& MD(ftxui::Element el);  // 80+ cols
  Responsive& LG(ftxui::Element el);  // 120+ cols
  Responsive& XL(ftxui::Element el);  // 180+ cols

  ftxui::Element Build() const;
  ftxui::Component BuildComponent() const;

 private:
  struct Breakpoint {
    int min_cols;
    std::function<ftxui::Element()> factory;
  };
  std::vector<Breakpoint> breakpoints_;
};

// ── ConstraintBox
// ─────────────────────────────────────────────────────────────
ftxui::Element ConstraintBox(ftxui::Element inner, Constraints c);
ftxui::Component ConstraintBox(ftxui::Component inner, Constraints c);

// ── ConstraintRow / ConstraintColumn ─────────────────────────────────────────
struct ConstraintChild {
  ftxui::Element element;
  Constraints constraints;
};

ftxui::Element ConstraintRow(std::vector<ConstraintChild> children);
ftxui::Element ConstraintColumn(std::vector<ConstraintChild> children);

// ── ConstraintGrid
// ────────────────────────────────────────────────────────────
class ConstraintGrid {
 public:
  ConstraintGrid& Columns(std::vector<SizeConstraint> col_widths);
  ConstraintGrid& Columns(int count,
                          SizeConstraint each = SizeConstraint::Fill());
  ConstraintGrid& Gap(int h_gap, int v_gap = 1);

  ConstraintGrid& Add(ftxui::Element el, int col_span = 1, int row_span = 1);
  ConstraintGrid& Add(ftxui::Component c, int col_span = 1);

  ftxui::Element Build() const;
  ftxui::Component BuildComponent() const;

 private:
  struct Cell {
    ftxui::Element element;
    int col_span = 1;
    int row_span = 1;
  };
  std::vector<SizeConstraint> col_widths_;
  std::vector<Cell> cells_;
  int h_gap_ = 0;
  int v_gap_ = 0;
};

// ── AspectRatio
// ───────────────────────────────────────────────────────────────
ftxui::Element AspectRatio(ftxui::Element inner, int w_units, int h_units);

// ── Stack
// ─────────────────────────────────────────────────────────────────────
ftxui::Element Stack(std::vector<ftxui::Element> layers);

// ── Spacer
// ────────────────────────────────────────────────────────────────────
ftxui::Element Spacer(int chars = 1);
ftxui::Element FlexSpacer();

// ── Center
// ────────────────────────────────────────────────────────────────────
ftxui::Element CenterH(ftxui::Element inner);
ftxui::Element CenterV(ftxui::Element inner);
ftxui::Element CenterBoth(ftxui::Element inner);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_CONSTRAINT_LAYOUT_HPP
