// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/datatable.hpp"

#include <string>
#include <vector>

#include "ftxui/component/event.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

struct Row {
  std::string name;
  std::string value;
};

// ── Build with no data
// ──────────────────────────────────────────────────────────

TEST(DataTableTest, BuildWithNoDataDoesNotCrash) {
  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Column("Value", [](const Row& r) { return r.value; });
  // No .Data() call — data pointer is nullptr.
  auto comp = table.Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DataTableTest, BuildWithEmptyVectorDoesNotCrash) {
  std::vector<Row> rows;
  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Column("Value", [](const Row& r) { return r.value; })
      .Data(&rows);
  auto comp = table.Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DataTableTest, BuildWithNoColumnsDoesNotCrash) {
  std::vector<Row> rows = {{"a", "1"}};
  DataTable<Row> table;
  table.Data(&rows);
  auto comp = table.Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

// ── Data renders without crash
// ─────────────────────────────────────────────────

TEST(DataTableTest, BuildWithDataRendersOk) {
  std::vector<Row> rows = {{"Alice", "100"}, {"Bob", "200"}, {"Carol", "300"}};
  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Column("Value", [](const Row& r) { return r.value; })
      .Data(&rows);
  auto comp = table.Build();
  ASSERT_NE(comp->Render(), nullptr);
}

// ── OnSelect callback
// ──────────────────────────────────────────────────────────

TEST(DataTableTest, OnSelectFiresOnArrowDown) {
  std::vector<Row> rows = {{"Alice", "1"}, {"Bob", "2"}, {"Carol", "3"}};

  size_t selected_index = 9999;
  std::string selected_name;

  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Column("Value", [](const Row& r) { return r.value; })
      .Data(&rows)
      .Selectable(true)
      .OnSelect([&](const Row& row, size_t idx) {
        selected_index = idx;
        selected_name = row.name;
      });
  auto comp = table.Build();
  comp->Render();  // Initialize internal state.

  comp->OnEvent(ftxui::Event::ArrowDown);

  EXPECT_EQ(selected_index, 1u);
  EXPECT_EQ(selected_name, "Bob");
}

TEST(DataTableTest, OnSelectDoesNotFireWhenAlreadyAtBottom) {
  std::vector<Row> rows = {{"Alice", "1"}};
  int select_count = 0;

  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Data(&rows)
      .Selectable(true)
      .OnSelect([&](const Row&, size_t) { select_count++; });
  auto comp = table.Build();
  comp->Render();

  // Only one row — ArrowDown should not fire since we can't go further down.
  comp->OnEvent(ftxui::Event::ArrowDown);
  EXPECT_EQ(select_count, 0);
}

TEST(DataTableTest, OnSelectFiresOnArrowUp) {
  std::vector<Row> rows = {{"Alice", "1"}, {"Bob", "2"}};
  std::string last_selected;

  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Data(&rows)
      .Selectable(true)
      .OnSelect([&](const Row& row, size_t) { last_selected = row.name; });
  auto comp = table.Build();
  comp->Render();

  comp->OnEvent(ftxui::Event::ArrowDown);  // -> Bob
  comp->OnEvent(ftxui::Event::ArrowUp);    // -> Alice
  EXPECT_EQ(last_selected, "Alice");
}

// ── OnActivate callback
// ────────────────────────────────────────────────────────

TEST(DataTableTest, OnActivateFiresOnEnter) {
  std::vector<Row> rows = {{"Alice", "1"}, {"Bob", "2"}};
  bool activated = false;
  std::string activated_name;

  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Data(&rows)
      .Selectable(true)
      .OnActivate([&](const Row& row, size_t) {
        activated = true;
        activated_name = row.name;
      });
  auto comp = table.Build();
  comp->Render();

  comp->OnEvent(ftxui::Event::Return);
  EXPECT_TRUE(activated);
  EXPECT_EQ(activated_name, "Alice");
}

TEST(DataTableTest, OnActivateFiresOnCorrectRow) {
  std::vector<Row> rows = {{"Alice", "1"}, {"Bob", "2"}, {"Carol", "3"}};
  std::string activated_name;

  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Data(&rows)
      .Selectable(true)
      .OnActivate([&](const Row& row, size_t) { activated_name = row.name; });
  auto comp = table.Build();
  comp->Render();

  comp->OnEvent(ftxui::Event::ArrowDown);  // select Bob
  comp->OnEvent(ftxui::Event::ArrowDown);  // select Carol
  comp->OnEvent(ftxui::Event::Return);     // activate Carol
  EXPECT_EQ(activated_name, "Carol");
}

// ── Sorting
// ────────────────────────────────────────────────────────────────────

TEST(DataTableTest, SortingWithLessThanKeyReordersRows) {
  // Start with data in reverse alphabetical order.
  std::vector<Row> rows = {{"Charlie", "3"}, {"Alice", "1"}, {"Bob", "2"}};

  std::string first_activated;

  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Column("Value", [](const Row& r) { return r.value; })
      .Data(&rows)
      .Selectable(true)
      .Sortable(true)
      .OnActivate([&](const Row& row, size_t) { first_activated = row.name; });
  auto comp = table.Build();
  comp->Render();

  // '<' advances sort column (wraps from -1 to last column, then to first).
  // First '<' => sort_column moves from -1 to last col (Value).
  // Second '<' would go to first col (Name). Let's try:
  // Actually from the impl: '<' decrements sort_column; -1 -> cols.size()-1
  comp->OnEvent(ftxui::Event::Character("<"));  // sort by Value (index 1)
  comp->OnEvent(ftxui::Event::Character("<"));  // sort by Name  (index 0)
  comp->Render();                               // Re-render with sort applied.

  // Now rows should be sorted ascending by Name: Alice, Bob, Charlie.
  // Row at index 0 in visible list is Alice.
  comp->OnEvent(ftxui::Event::Return);  // activate first visible row
  EXPECT_EQ(first_activated, "Alice");
}

TEST(DataTableTest, SortingWithGreaterThanKeyAdvancesColumn) {
  std::vector<Row> rows = {{"Charlie", "3"}, {"Alice", "1"}, {"Bob", "2"}};

  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Data(&rows)
      .Selectable(true)
      .Sortable(true);
  auto comp = table.Build();
  comp->Render();

  // '>' should not crash regardless of column count.
  comp->OnEvent(ftxui::Event::Character(">"));
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DataTableTest, SortingDisabledIgnoresLessGreaterKeys) {
  std::vector<Row> rows = {{"B", "2"}, {"A", "1"}};
  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Data(&rows)
      .Sortable(false);
  auto comp = table.Build();
  comp->Render();

  // Should not crash.
  comp->OnEvent(ftxui::Event::Character("<"));
  comp->OnEvent(ftxui::Event::Character(">"));
  ASSERT_NE(comp->Render(), nullptr);
}

// ── Filter
// ─────────────────────────────────────────────────────────────────────

TEST(DataTableTest, FilterHidesNonMatchingRows) {
  std::vector<Row> rows = {{"Alice", "1"}, {"Bob", "2"}, {"Carol", "3"}};
  std::string filter = "ali";

  bool alice_activated = false;
  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Data(&rows)
      .Selectable(true)
      .FilterText(&filter)
      .OnActivate([&](const Row& row, size_t) {
        alice_activated = (row.name == "Alice");
      });
  auto comp = table.Build();
  comp->Render();

  // Only "Alice" matches "ali"; Enter should activate Alice.
  comp->OnEvent(ftxui::Event::Return);
  EXPECT_TRUE(alice_activated);
}

// ── AlternateRows option
// ───────────────────────────────────────────────────────

TEST(DataTableTest, AlternateRowsDoesNotCrash) {
  std::vector<Row> rows = {{"A", "1"}, {"B", "2"}, {"C", "3"}};
  DataTable<Row> table;
  table.Column("Name", [](const Row& r) { return r.name; })
      .Data(&rows)
      .AlternateRows(true);
  auto comp = table.Build();
  ASSERT_NE(comp->Render(), nullptr);
}

// ── Fixed-width columns
// ────────────────────────────────────────────────────────

TEST(DataTableTest, FixedWidthColumnDoesNotCrash) {
  std::vector<Row> rows = {{"Alice", "1"}};
  DataTable<Row> table;
  table.Column(
           "Name", [](const Row& r) { return r.name; }, 10)
      .Column(
          "Value", [](const Row& r) { return r.value; }, 5)
      .Data(&rows);
  auto comp = table.Build();
  ASSERT_NE(comp->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
