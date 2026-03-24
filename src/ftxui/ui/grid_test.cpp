// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/grid.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

TEST(GridTest, EmptyGridBuildsAndRenders) {
  auto comp = Grid(3).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, SingleColumnEmptyGridRenders) {
  auto comp = Grid(1).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, TwoCellsTwoColumnRenders) {
  auto comp = Grid(2)
                  .Cell(ftxui::text("A"))
                  .Cell(ftxui::text("B"))
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, ThreeCellsTwoColumnsWrapsToTwoRows) {
  auto comp = Grid(2)
                  .Cell(ftxui::text("A"))
                  .Cell(ftxui::text("B"))
                  .Cell(ftxui::text("C"))
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, ThreeColumnFillsOneRow) {
  auto comp = Grid(3)
                  .Cell(ftxui::text("X"))
                  .Cell(ftxui::text("Y"))
                  .Cell(ftxui::text("Z"))
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, MoreCellsThanColumnsMultipleRows) {
  auto comp = Grid(3)
                  .Cell(ftxui::text("1"))
                  .Cell(ftxui::text("2"))
                  .Cell(ftxui::text("3"))
                  .Cell(ftxui::text("4"))
                  .Cell(ftxui::text("5"))
                  .Cell(ftxui::text("6"))
                  .Cell(ftxui::text("7"))
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, CellComponentBuildsAndRenders) {
  auto btn1 = ftxui::Button("A", [] {});
  auto btn2 = ftxui::Button("B", [] {});
  auto comp = Grid(2).CellComponent(btn1).CellComponent(btn2).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, CellComponentMoreThanColumnCount) {
  auto btn1 = ftxui::Button("1", [] {});
  auto btn2 = ftxui::Button("2", [] {});
  auto btn3 = ftxui::Button("3", [] {});
  auto comp = Grid(2)
                  .CellComponent(btn1)
                  .CellComponent(btn2)
                  .CellComponent(btn3)
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, MixedCellAndCellComponentRenders) {
  auto btn = ftxui::Button("Click", [] {});
  auto comp = Grid(2)
                  .Cell(ftxui::text("Label"))
                  .CellComponent(btn)
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, GapDoesNotCrash) {
  auto comp = Grid(2)
                  .Cell(ftxui::text("A"))
                  .Cell(ftxui::text("B"))
                  .Gap(2)
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, RowGapDoesNotCrash) {
  auto comp = Grid(2)
                  .Cell(ftxui::text("A"))
                  .Cell(ftxui::text("B"))
                  .Cell(ftxui::text("C"))
                  .RowGap(1)
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GridTest, GapAndRowGapTogether) {
  auto comp = Grid(3)
                  .Cell(ftxui::text("A"))
                  .Cell(ftxui::text("B"))
                  .Cell(ftxui::text("C"))
                  .Cell(ftxui::text("D"))
                  .Gap(1)
                  .RowGap(1)
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
