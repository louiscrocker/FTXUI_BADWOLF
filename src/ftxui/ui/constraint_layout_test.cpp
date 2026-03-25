// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/constraint_layout.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/terminal.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── SizeConstraint construction
// ───────────────────────────────────────────────

TEST(ConstraintLayout, SizeConstraintCharsUnit) {
  auto sc = SizeConstraint::Chars(20);
  EXPECT_EQ(sc.unit, SizeConstraint::Unit::Chars);
  EXPECT_FLOAT_EQ(sc.value, 20.0f);
}

TEST(ConstraintLayout, SizeConstraintCharsValue) {
  auto sc = SizeConstraint::Chars(42);
  EXPECT_FLOAT_EQ(sc.value, 42.0f);
}

TEST(ConstraintLayout, SizeConstraintPercentUnit) {
  auto sc = SizeConstraint::Percent(50.0f);
  EXPECT_EQ(sc.unit, SizeConstraint::Unit::Percent);
}

TEST(ConstraintLayout, SizeConstraintPercentValue) {
  auto sc = SizeConstraint::Percent(75.0f);
  EXPECT_FLOAT_EQ(sc.value, 75.0f);
}

TEST(ConstraintLayout, SizeConstraintFillUnit) {
  auto sc = SizeConstraint::Fill();
  EXPECT_EQ(sc.unit, SizeConstraint::Unit::Fill);
}

TEST(ConstraintLayout, SizeConstraintAutoUnit) {
  auto sc = SizeConstraint::Auto();
  EXPECT_EQ(sc.unit, SizeConstraint::Unit::Auto);
}

// ── Constraints defaults
// ──────────────────────────────────────────────────────

TEST(ConstraintLayout, ConstraintsDefaultMinWidth) {
  Constraints c;
  EXPECT_EQ(c.min_width, 0);
}

TEST(ConstraintLayout, ConstraintsDefaultMaxWidth) {
  Constraints c;
  EXPECT_EQ(c.max_width, INT_MAX);
}

TEST(ConstraintLayout, ConstraintsPaddingSetsAllSides) {
  Constraints c;
  c.Padding(3);
  EXPECT_EQ(c.pad_left, 3);
  EXPECT_EQ(c.pad_right, 3);
  EXPECT_EQ(c.pad_top, 3);
  EXPECT_EQ(c.pad_bottom, 3);
}

TEST(ConstraintLayout, ConstraintsPaddingHSetsLeftRight) {
  Constraints c;
  c.PaddingH(2);
  EXPECT_EQ(c.pad_left, 2);
  EXPECT_EQ(c.pad_right, 2);
  EXPECT_EQ(c.pad_top, 0);
  EXPECT_EQ(c.pad_bottom, 0);
}

TEST(ConstraintLayout, ConstraintsPaddingVSetsTopBottom) {
  Constraints c;
  c.PaddingV(4);
  EXPECT_EQ(c.pad_top, 4);
  EXPECT_EQ(c.pad_bottom, 4);
  EXPECT_EQ(c.pad_left, 0);
  EXPECT_EQ(c.pad_right, 0);
}

// ── ConstraintBox
// ─────────────────────────────────────────────────────────────

TEST(ConstraintLayout, ConstraintBoxCharsReturnsElement) {
  Constraints c;
  c.width = SizeConstraint::Chars(20);
  auto el = ConstraintBox(ftxui::text("hello"), c);
  ASSERT_NE(el, nullptr);
}

TEST(ConstraintLayout, ConstraintBoxPercentReturnsElement) {
  Constraints c;
  c.width = SizeConstraint::Percent(50.0f);
  auto el = ConstraintBox(ftxui::text("hello"), c);
  ASSERT_NE(el, nullptr);
}

// ── ConstraintRow
// ─────────────────────────────────────────────────────────────

TEST(ConstraintLayout, ConstraintRowEmptyReturnsElement) {
  auto el = ConstraintRow({});
  ASSERT_NE(el, nullptr);
}

TEST(ConstraintLayout, ConstraintRowSingleChild) {
  Constraints c;
  c.width = SizeConstraint::Chars(10);
  auto el = ConstraintRow({{ftxui::text("A"), c}});
  ASSERT_NE(el, nullptr);
}

TEST(ConstraintLayout, ConstraintRowTwoFillChildren) {
  ftxui::Terminal::SetFallbackSize({80, 24});
  Constraints c;
  c.width = SizeConstraint::Fill();
  auto el = ConstraintRow({{ftxui::text("A"), c}, {ftxui::text("B"), c}});
  ASSERT_NE(el, nullptr);
}

// ── ConstraintColumn
// ──────────────────────────────────────────────────────────

TEST(ConstraintLayout, ConstraintColumnSingleChild) {
  Constraints c;
  c.height = SizeConstraint::Chars(5);
  auto el = ConstraintColumn({{ftxui::text("X"), c}});
  ASSERT_NE(el, nullptr);
}

// ── ConstraintGrid
// ────────────────────────────────────────────────────────────

TEST(ConstraintLayout, ConstraintGridConstructs) {
  ConstraintGrid grid;
  SUCCEED();
}

TEST(ConstraintLayout, ConstraintGridColumnsSetsByVector) {
  ConstraintGrid grid;
  grid.Columns({SizeConstraint::Chars(10), SizeConstraint::Fill()});
  auto el = grid.Build();
  ASSERT_NE(el, nullptr);
}

TEST(ConstraintLayout, ConstraintGridAddBuildsElement) {
  ftxui::Terminal::SetFallbackSize({80, 24});
  ConstraintGrid grid;
  grid.Columns(2, SizeConstraint::Fill());
  grid.Add(ftxui::text("A"));
  grid.Add(ftxui::text("B"));
  auto el = grid.Build();
  ASSERT_NE(el, nullptr);
}

// ── Responsive
// ────────────────────────────────────────────────────────────────

TEST(ConstraintLayout, ResponsiveConstructs) {
  Responsive r;
  SUCCEED();
}

TEST(ConstraintLayout, ResponsiveAtAddsBreakpoint) {
  Responsive r;
  r.At(0, ftxui::text("narrow"));
  auto el = r.Build();
  ASSERT_NE(el, nullptr);
}

TEST(ConstraintLayout, ResponsiveBuildReturnsElement) {
  ftxui::Terminal::SetFallbackSize({80, 24});
  Responsive r;
  r.XS(ftxui::text("xs")).MD(ftxui::text("md"));
  auto el = r.Build();
  ASSERT_NE(el, nullptr);
}

// ── Stack
// ─────────────────────────────────────────────────────────────────────

TEST(ConstraintLayout, StackTwoLayersReturnsElement) {
  auto el = Stack({ftxui::text("back"), ftxui::text("front")});
  ASSERT_NE(el, nullptr);
}

// ── CenterBoth
// ────────────────────────────────────────────────────────────────

TEST(ConstraintLayout, CenterBothReturnsElement) {
  auto el = CenterBoth(ftxui::text("centered"));
  ASSERT_NE(el, nullptr);
}

// ── Spacer / FlexSpacer
// ───────────────────────────────────────────────────────

TEST(ConstraintLayout, SpacerReturnsElement) {
  auto el = Spacer(4);
  ASSERT_NE(el, nullptr);
}

TEST(ConstraintLayout, FlexSpacerReturnsElement) {
  auto el = FlexSpacer();
  ASSERT_NE(el, nullptr);
}

}  // namespace
}  // namespace ftxui::ui
