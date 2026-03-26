// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file sql_test.cpp
/// Tests for SqlValue, SqliteDb, DataFrame, ReactiveDataFrame and
/// DataFrameTable.

#include "ftxui/ui/sql.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ============================================================================
// SqlValue — construction
// ============================================================================

TEST(SqlValueTest, NullConstruction) {
  SqlValue v;
  EXPECT_TRUE(v.is_null());
  EXPECT_EQ(v.type(), SqlValue::Type::Null);
}

TEST(SqlValueTest, NullptrConstruction) {
  SqlValue v(nullptr);
  EXPECT_TRUE(v.is_null());
}

TEST(SqlValueTest, IntConstruction) {
  SqlValue v(int64_t{42});
  EXPECT_EQ(v.type(), SqlValue::Type::Integer);
  EXPECT_EQ(v.as_int(), int64_t{42});
}

TEST(SqlValueTest, RealConstruction) {
  SqlValue v(3.14);
  EXPECT_EQ(v.type(), SqlValue::Type::Real);
  EXPECT_DOUBLE_EQ(v.as_real(), 3.14);
}

TEST(SqlValueTest, TextConstruction) {
  SqlValue v(std::string("hello"));
  EXPECT_EQ(v.type(), SqlValue::Type::Text);
  EXPECT_EQ(v.as_text(), "hello");
}

// ============================================================================
// SqlValue — type predicates
// ============================================================================

TEST(SqlValueTest, TypeQueries) {
  SqlValue null_v;
  EXPECT_TRUE(null_v.is_null());

  SqlValue int_v(int64_t{1});
  EXPECT_FALSE(int_v.is_null());
  EXPECT_EQ(int_v.type(), SqlValue::Type::Integer);

  SqlValue real_v(2.5);
  EXPECT_FALSE(real_v.is_null());
  EXPECT_EQ(real_v.type(), SqlValue::Type::Real);

  SqlValue text_v(std::string("x"));
  EXPECT_FALSE(text_v.is_null());
  EXPECT_EQ(text_v.type(), SqlValue::Type::Text);
}

// ============================================================================
// SqlValue — to_string
// ============================================================================

TEST(SqlValueTest, ToString) {
  // Integer → "42"
  EXPECT_EQ(SqlValue(int64_t{42}).to_string(), "42");

  // Real → contains "3" (e.g. "3.14")
  std::string real_str = SqlValue(3.14).to_string();
  EXPECT_FALSE(real_str.empty());
  EXPECT_NE(real_str.find('3'), std::string::npos);

  // Text → verbatim
  EXPECT_EQ(SqlValue(std::string("hello")).to_string(), "hello");

  // Null → "NULL"
  EXPECT_EQ(SqlValue().to_string(), "NULL");
}

// ============================================================================
// SqlValue — safe accessors
// ============================================================================

TEST(SqlValueTest, GetInt) {
  SqlValue null_v;
  EXPECT_EQ(null_v.get_int(int64_t{99}), int64_t{99});

  SqlValue int_v(int64_t{7});
  EXPECT_EQ(int_v.get_int(int64_t{99}), int64_t{7});
}

TEST(SqlValueTest, GetReal) {
  SqlValue null_v;
  EXPECT_DOUBLE_EQ(null_v.get_real(1.5), 1.5);

  SqlValue real_v(2.5);
  EXPECT_DOUBLE_EQ(real_v.get_real(0.0), 2.5);
}

TEST(SqlValueTest, GetText) {
  SqlValue null_v;
  EXPECT_EQ(null_v.get_text("default"), "default");

  SqlValue text_v(std::string("hello"));
  EXPECT_EQ(text_v.get_text("default"), "hello");
}

// ============================================================================
// SqliteDb
// ============================================================================

TEST(SqliteDbTest, ConstructsWithoutCrash) {
  // Constructor calls Open(":memory:") internally.
  // Without BADWOLF_SQLITE it fails gracefully → IsOpen() == false.
  SqliteDb db;
  EXPECT_FALSE(db.IsOpen());
}

TEST(SqliteDbTest, OpenAndClose) {
  SqliteDb db;
  if (!db.IsOpen()) {
    db.Open(":memory:");
  }
  if (!db.IsOpen()) {
    GTEST_SKIP() << "SQLite not available";
  }
  EXPECT_TRUE(db.IsOpen());
  db.Close();
  EXPECT_FALSE(db.IsOpen());
}

// ============================================================================
// DataFrame — construction and dimensions
// ============================================================================

TEST(DataFrameTest, DefaultConstruction) {
  DataFrame df;
  EXPECT_TRUE(df.empty());
  EXPECT_EQ(df.rows(), 0u);
  EXPECT_EQ(df.cols(), 0u);
}

TEST(DataFrameTest, FromCSVSimple) {
  DataFrame df = DataFrame::FromCSV("a,b,c\n1,2,3\n4,5,6\n");
  EXPECT_EQ(df.rows(), 2u);
  EXPECT_EQ(df.cols(), 3u);
}

TEST(DataFrameTest, FromCSVHeaders) {
  DataFrame df = DataFrame::FromCSV("a,b,c\n1,2,3\n4,5,6\n");
  const auto& cols = df.columns();
  ASSERT_EQ(cols.size(), 3u);
  EXPECT_EQ(cols[0], "a");
  EXPECT_EQ(cols[1], "b");
  EXPECT_EQ(cols[2], "c");
}

// ============================================================================
// DataFrame — transformations
// ============================================================================

TEST(DataFrameTest, Filter) {
  // Use FromJson so values are integers for reliable equality comparison.
  auto json =
      Json::Parse("[{\"a\":1,\"b\":10},{\"a\":2,\"b\":20},{\"a\":1,\"b\":30}]");
  DataFrame df = DataFrame::FromJson(json);
  DataFrame filtered =
      df.Filter([](const SqlRow& r, const std::vector<std::string>& cols) {
        for (size_t i = 0; i < cols.size(); ++i) {
          if (cols[i] == "a" && i < r.size()) {
            return r[i] == SqlValue(int64_t{1});
          }
        }
        return false;
      });
  EXPECT_EQ(filtered.rows(), 2u);
}

TEST(DataFrameTest, SortByAscending) {
  // Lexicographic sort: "10" < "20" < "30" — ascending gives "10" first.
  DataFrame df = DataFrame::FromCSV("name,score\nBob,30\nAlice,10\nCarol,20\n");
  DataFrame sorted = df.SortBy("score", true);
  ASSERT_GE(sorted.rows(), 1u);
  EXPECT_EQ(sorted.at(0, "score").to_string(), "10");
}

TEST(DataFrameTest, SortByDescending) {
  // Lexicographic sort descending: "30" first.
  DataFrame df = DataFrame::FromCSV("name,score\nBob,30\nAlice,10\nCarol,20\n");
  DataFrame sorted = df.SortBy("score", false);
  ASSERT_GE(sorted.rows(), 1u);
  EXPECT_EQ(sorted.at(0, "score").to_string(), "30");
}

TEST(DataFrameTest, Select) {
  DataFrame df = DataFrame::FromCSV("a,b,c\n1,2,3\n");
  DataFrame selected = df.Select({"a", "c"});
  ASSERT_EQ(selected.cols(), 2u);
  const auto& cols = selected.columns();
  EXPECT_EQ(cols[0], "a");
  EXPECT_EQ(cols[1], "c");
}

TEST(DataFrameTest, Head) {
  const std::string csv = "n\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n";
  DataFrame df = DataFrame::FromCSV(csv);
  ASSERT_EQ(df.rows(), 10u);
  DataFrame head = df.Head(3);
  EXPECT_EQ(head.rows(), 3u);
}

TEST(DataFrameTest, Tail) {
  const std::string csv = "n\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n";
  DataFrame df = DataFrame::FromCSV(csv);
  ASSERT_EQ(df.rows(), 10u);
  DataFrame tail = df.Tail(3);
  ASSERT_EQ(tail.rows(), 3u);
  EXPECT_EQ(tail.at(0, "n").to_string(), "8");
  EXPECT_EQ(tail.at(2, "n").to_string(), "10");
}

// ============================================================================
// DataFrame — aggregates (use FromJson for proper numeric types)
// ============================================================================

TEST(DataFrameTest, Sum) {
  auto json = Json::Parse(
      "[{\"val\":1},{\"val\":2},{\"val\":3},{\"val\":4},{\"val\":5}]");
  DataFrame df = DataFrame::FromJson(json);
  EXPECT_DOUBLE_EQ(df.Sum("val"), 15.0);
}

TEST(DataFrameTest, Mean) {
  auto json = Json::Parse(
      "[{\"val\":1},{\"val\":2},{\"val\":3},{\"val\":4},{\"val\":5}]");
  DataFrame df = DataFrame::FromJson(json);
  EXPECT_DOUBLE_EQ(df.Mean("val"), 3.0);
}

TEST(DataFrameTest, MinMax) {
  auto json = Json::Parse(
      "[{\"val\":1},{\"val\":2},{\"val\":3},{\"val\":4},{\"val\":5}]");
  DataFrame df = DataFrame::FromJson(json);
  EXPECT_DOUBLE_EQ(df.Min("val"), 1.0);
  EXPECT_DOUBLE_EQ(df.Max("val"), 5.0);
}

TEST(DataFrameTest, Count) {
  auto json = Json::Parse(
      "[{\"val\":1},{\"val\":2},{\"val\":3},{\"val\":4},{\"val\":5}]");
  DataFrame df = DataFrame::FromJson(json);
  EXPECT_EQ(df.Count("val"), 5u);
}

// ============================================================================
// DataFrame — mutation / export
// ============================================================================

TEST(DataFrameTest, AddColumn) {
  DataFrame df = DataFrame::FromCSV("a,b\n1,2\n3,4\n");
  const size_t original_cols = df.cols();
  DataFrame df2 = df.AddColumn(
      "c", [](const SqlRow&) { return SqlValue(std::string("x")); });
  EXPECT_EQ(df2.cols(), original_cols + 1u);
  EXPECT_EQ(df2.rows(), df.rows());
}

TEST(DataFrameTest, ToCSVRoundTrip) {
  DataFrame df = DataFrame::FromCSV("a,b,c\n1,2,3\n4,5,6\n");
  const std::string csv = df.ToCSV();
  DataFrame df2 = DataFrame::FromCSV(csv);
  EXPECT_EQ(df2.rows(), df.rows());
  EXPECT_EQ(df2.cols(), df.cols());
}

TEST(DataFrameTest, ToJson) {
  DataFrame df = DataFrame::FromCSV("a,b\n1,2\n3,4\n5,6\n");
  JsonValue json = df.ToJson();
  EXPECT_TRUE(json.is_array());
  EXPECT_EQ(json.size(), 3u);
}

// ============================================================================
// DataFrame — UI components
// ============================================================================

TEST(DataFrameTest, AsSummaryNotNull) {
  DataFrame df = DataFrame::FromCSV("a,b\n1,2\n3,4\n");
  ftxui::Element elem = df.AsSummary();
  EXPECT_NE(elem, nullptr);
}

TEST(DataFrameTest, TableReturnsComponent) {
  DataFrame df = DataFrame::FromCSV("a,b\n1,2\n3,4\n");
  ftxui::Component comp = DataFrameTable(df);
  ASSERT_NE(comp, nullptr);
  EXPECT_NE(comp->Render(), nullptr);
}

// ============================================================================
// ReactiveDataFrame
// ============================================================================

TEST(ReactiveDataFrameTest, SetTriggersCallback) {
  ReactiveDataFrame rdf;
  int call_count = 0;
  rdf.OnChange([&](const DataFrame&) { ++call_count; });
  rdf.Set(DataFrame::FromCSV("x\n1\n2\n"));
  EXPECT_GE(call_count, 1);
}

}  // namespace
}  // namespace ftxui::ui
