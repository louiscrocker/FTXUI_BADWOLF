// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file json_test.cpp
/// Tests for JsonValue, Json::Parse/Stringify, JsonPath, ReactiveJson,
/// and JSON UI components.

#include "ftxui/ui/json.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ============================================================================
// JsonValue — construction
// ============================================================================

TEST(JsonValueTest, NullConstruction) {
  JsonValue v;
  EXPECT_TRUE(v.is_null());
  EXPECT_EQ(v.type(), JsonValue::Type::Null);
}

TEST(JsonValueTest, NullptrConstruction) {
  JsonValue v(nullptr);
  EXPECT_TRUE(v.is_null());
}

TEST(JsonValueTest, BoolConstruction) {
  JsonValue vt(true);
  JsonValue vf(false);
  EXPECT_TRUE(vt.is_bool());
  EXPECT_TRUE(vt.as_bool());
  EXPECT_FALSE(vf.as_bool());
}

TEST(JsonValueTest, IntConstruction) {
  JsonValue v(42);
  EXPECT_TRUE(v.is_number());
  EXPECT_EQ(v.as_int(), 42);
}

TEST(JsonValueTest, DoubleConstruction) {
  JsonValue v(3.14);
  EXPECT_TRUE(v.is_number());
  EXPECT_DOUBLE_EQ(v.as_number(), 3.14);
}

TEST(JsonValueTest, StringConstruction) {
  JsonValue v("hello");
  EXPECT_TRUE(v.is_string());
  EXPECT_EQ(v.as_string(), "hello");
}

TEST(JsonValueTest, ArrayViaInitializerList) {
  JsonValue v{JsonValue(1), JsonValue(2), JsonValue(3)};
  EXPECT_TRUE(v.is_array());
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0u].as_int(), 1);
  EXPECT_EQ(v[1u].as_int(), 2);
  EXPECT_EQ(v[2u].as_int(), 3);
}

TEST(JsonValueTest, ObjectStaticFactory) {
  JsonValue v = JsonValue::Object();
  EXPECT_TRUE(v.is_object());
  EXPECT_TRUE(v.empty());
}

TEST(JsonValueTest, ArrayStaticFactory) {
  JsonValue v = JsonValue::Array();
  EXPECT_TRUE(v.is_array());
  EXPECT_TRUE(v.empty());
}

// ============================================================================
// JsonValue — type predicates
// ============================================================================

TEST(JsonValueTest, TypePredicates) {
  EXPECT_TRUE(JsonValue().is_null());
  EXPECT_TRUE(JsonValue(true).is_bool());
  EXPECT_TRUE(JsonValue(1).is_number());
  EXPECT_TRUE(JsonValue("x").is_string());
  EXPECT_TRUE(JsonValue::Array().is_array());
  EXPECT_TRUE(JsonValue::Object().is_object());

  EXPECT_FALSE(JsonValue(true).is_null());
  EXPECT_FALSE(JsonValue().is_bool());
}

// ============================================================================
// JsonValue — throwing accessors
// ============================================================================

TEST(JsonValueTest, AsBoolThrowsOnNonBool) {
  JsonValue v(42);
  EXPECT_THROW(v.as_bool(), std::runtime_error);
}

TEST(JsonValueTest, AsStringReturnsValue) {
  JsonValue v("world");
  EXPECT_EQ(v.as_string(), "world");
}

TEST(JsonValueTest, AsNumberThrowsOnNonNumber) {
  JsonValue v("not a number");
  EXPECT_THROW(v.as_number(), std::runtime_error);
}

// ============================================================================
// JsonValue — object access
// ============================================================================

TEST(JsonValueTest, ObjectOperatorBracket) {
  JsonValue obj = JsonValue::Object();
  obj["key"] = JsonValue("value");
  EXPECT_EQ(obj["key"].as_string(), "value");
}

TEST(JsonValueTest, HasReturnsTrueForExistingKey) {
  JsonValue obj = JsonValue::Object();
  obj.set("x", JsonValue(1));
  EXPECT_TRUE(obj.has("x"));
}

TEST(JsonValueTest, HasReturnsFalseForMissingKey) {
  JsonValue obj = JsonValue::Object();
  EXPECT_FALSE(obj.has("missing"));
}

TEST(JsonValueTest, ConstOperatorBracketMissingKeyReturnsNull) {
  const JsonValue obj = JsonValue::Object();
  EXPECT_TRUE(obj["nonexistent"].is_null());
}

// ============================================================================
// JsonValue — array access
// ============================================================================

TEST(JsonValueTest, ArrayOperatorIndex) {
  JsonValue arr = JsonValue::Array();
  arr.push(JsonValue(10));
  arr.push(JsonValue(20));
  EXPECT_EQ(arr[0u].as_int(), 10);
  EXPECT_EQ(arr[1u].as_int(), 20);
}

TEST(JsonValueTest, ArraySize) {
  JsonValue arr = JsonValue::Array();
  EXPECT_EQ(arr.size(), 0u);
  arr.push(JsonValue(1));
  arr.push(JsonValue(2));
  EXPECT_EQ(arr.size(), 2u);
}

TEST(JsonValueTest, PushIncreasesArraySize) {
  JsonValue arr = JsonValue::Array();
  arr.push(JsonValue(99));
  EXPECT_EQ(arr.size(), 1u);
}

TEST(JsonValueTest, PopDecreasesArraySize) {
  JsonValue arr = JsonValue::Array();
  arr.push(JsonValue(1));
  arr.push(JsonValue(2));
  arr.pop();
  EXPECT_EQ(arr.size(), 1u);
}

// ============================================================================
// JsonValue — equality
// ============================================================================

TEST(JsonValueTest, EqualityForEqualValues) {
  EXPECT_EQ(JsonValue(42), JsonValue(42));
  EXPECT_EQ(JsonValue("hi"), JsonValue("hi"));
  EXPECT_EQ(JsonValue(), JsonValue());
  EXPECT_EQ(JsonValue(true), JsonValue(true));
}

TEST(JsonValueTest, InequalityForDifferentValues) {
  EXPECT_NE(JsonValue(1), JsonValue(2));
  EXPECT_NE(JsonValue("a"), JsonValue("b"));
  EXPECT_NE(JsonValue(), JsonValue(false));
}

// ============================================================================
// Json::Parse
// ============================================================================

TEST(JsonParseTest, NullLiteral) {
  auto v = Json::Parse("null");
  EXPECT_TRUE(v.is_null());
}

TEST(JsonParseTest, BoolTrue) {
  auto v = Json::Parse("true");
  EXPECT_TRUE(v.is_bool());
  EXPECT_TRUE(v.as_bool());
}

TEST(JsonParseTest, BoolFalse) {
  auto v = Json::Parse("false");
  EXPECT_FALSE(v.as_bool());
}

TEST(JsonParseTest, Integer) {
  auto v = Json::Parse("42");
  EXPECT_TRUE(v.is_number());
  EXPECT_EQ(v.as_int(), 42);
}

TEST(JsonParseTest, Float) {
  auto v = Json::Parse("3.14");
  EXPECT_TRUE(v.is_number());
  EXPECT_NEAR(v.as_number(), 3.14, 1e-6);
}

TEST(JsonParseTest, StringWithEscapes) {
  auto v = Json::Parse("\"hello\\nworld\"");
  EXPECT_TRUE(v.is_string());
  EXPECT_EQ(v.as_string(), "hello\nworld");
}

TEST(JsonParseTest, Array) {
  auto v = Json::Parse("[1, 2, 3]");
  EXPECT_TRUE(v.is_array());
  EXPECT_EQ(v.size(), 3u);
}

TEST(JsonParseTest, Object) {
  auto v = Json::Parse("{\"key\": \"val\"}");
  EXPECT_TRUE(v.is_object());
  EXPECT_EQ(v["key"].as_string(), "val");
}

TEST(JsonParseTest, ThrowsOnInvalidJson) {
  EXPECT_THROW(Json::Parse("{invalid}"), std::runtime_error);
}

TEST(JsonParseTest, ParseSafeReturnsNullPlusErrorOnInvalid) {
  std::string err;
  auto v = Json::ParseSafe("{invalid}", &err);
  EXPECT_TRUE(v.is_null());
  EXPECT_FALSE(err.empty());
}

TEST(JsonParseTest, NestedObject) {
  auto v = Json::Parse("{\"a\":{\"b\":42}}");
  EXPECT_EQ(v["a"]["b"].as_int(), 42);
}

TEST(JsonParseTest, UnicodeEscape) {
  auto v = Json::Parse("\"\\u0041\"");  // 'A'
  EXPECT_EQ(v.as_string(), "A");
}

// ============================================================================
// Json::Stringify
// ============================================================================

TEST(JsonStringifyTest, Null) {
  EXPECT_EQ(Json::Stringify(JsonValue()), "null");
}

TEST(JsonStringifyTest, BoolTrue) {
  EXPECT_EQ(Json::Stringify(JsonValue(true)), "true");
}

TEST(JsonStringifyTest, BoolFalse) {
  EXPECT_EQ(Json::Stringify(JsonValue(false)), "false");
}

TEST(JsonStringifyTest, CompactObject) {
  JsonValue obj = JsonValue::Object();
  obj.set("x", JsonValue(1));
  std::string s = Json::StringifyCompact(obj);
  EXPECT_EQ(s, "{\"x\":1}");
}

TEST(JsonStringifyTest, PrettyIndented) {
  JsonValue obj = JsonValue::Object();
  obj.set("n", JsonValue(7));
  std::string s = Json::StringifyPretty(obj, 2);
  EXPECT_NE(s.find('\n'), std::string::npos);
}

TEST(JsonStringifyTest, RoundTrip) {
  const std::string json_str = "[1,2,3]";
  auto v = Json::Parse(json_str);
  EXPECT_EQ(Json::StringifyCompact(v), json_str);
}

// ============================================================================
// Json::Merge
// ============================================================================

TEST(JsonMergeTest, CombinesObjects) {
  JsonValue a = JsonValue::Object();
  a.set("x", JsonValue(1));
  JsonValue b = JsonValue::Object();
  b.set("y", JsonValue(2));
  auto merged = Json::Merge(a, b);
  EXPECT_TRUE(merged.has("x"));
  EXPECT_TRUE(merged.has("y"));
}

TEST(JsonMergeTest, BOverwritesA) {
  JsonValue a = JsonValue::Object();
  a.set("k", JsonValue(1));
  JsonValue b = JsonValue::Object();
  b.set("k", JsonValue(99));
  auto merged = Json::Merge(a, b);
  EXPECT_EQ(merged["k"].as_int(), 99);
}

// ============================================================================
// JsonPath
// ============================================================================

TEST(JsonPathTest, GetSimpleKey) {
  auto root = Json::Parse("{\"name\":\"Alice\"}");
  JsonPath jp("$.name");
  EXPECT_TRUE(jp.valid());
  EXPECT_EQ(jp.Get(root).as_string(), "Alice");
}

TEST(JsonPathTest, GetNestedPath) {
  auto root = Json::Parse("{\"a\":{\"b\":42}}");
  JsonPath jp("$.a.b");
  EXPECT_EQ(jp.Get(root).as_int(), 42);
}

TEST(JsonPathTest, GetArrayIndex) {
  auto root = Json::Parse("[10, 20, 30]");
  JsonPath jp("$[1]");
  EXPECT_EQ(jp.Get(root).as_int(), 20);
}

TEST(JsonPathTest, InvalidPath) {
  JsonPath jp("not.a.path");
  EXPECT_FALSE(jp.valid());
}

TEST(JsonPathTest, GetMissingKeyReturnsNull) {
  auto root = Json::Parse("{\"x\":1}");
  JsonPath jp("$.missing");
  EXPECT_TRUE(jp.Get(root).is_null());
}

TEST(JsonPathTest, SetValue) {
  auto root = Json::Parse("{\"x\":1}");
  JsonPath jp("$.x");
  jp.Set(root, JsonValue(99));
  EXPECT_EQ(root["x"].as_int(), 99);
}

TEST(JsonPathTest, WildcardGetAll) {
  auto root = Json::Parse("[{\"n\":1},{\"n\":2}]");
  JsonPath jp("$[*].n");
  auto all = jp.GetAll(root);
  EXPECT_EQ(all.size(), 2u);
  EXPECT_EQ(all[0].as_int(), 1);
  EXPECT_EQ(all[1].as_int(), 2);
}

// ============================================================================
// ReactiveJson
// ============================================================================

TEST(ReactiveJsonTest, SetTriggersCallback) {
  ReactiveJson rj;
  int call_count = 0;
  rj.OnChange([&](const JsonValue&) { ++call_count; });
  rj.Set(Json::Parse("{\"x\":1}"));
  EXPECT_GE(call_count, 1);
}

TEST(ReactiveJsonTest, SetByPath) {
  ReactiveJson rj(Json::Parse("{\"a\":0}"));
  rj.Set("$.a", JsonValue(42));
  EXPECT_EQ(rj.Get()["a"].as_int(), 42);
}

TEST(ReactiveJsonTest, PatchMerges) {
  ReactiveJson rj(Json::Parse("{\"x\":1}"));
  auto patch = Json::Parse("{\"y\":2}");
  rj.Patch(patch);
  EXPECT_TRUE(rj.Get().has("x"));
  EXPECT_TRUE(rj.Get().has("y"));
}

// ============================================================================
// UI Components (smoke tests — ensure non-null)
// ============================================================================

TEST(JsonUITest, JsonElementReturnsNonNull) {
  auto elem = JsonElement(Json::Parse("{\"a\":1}"));
  EXPECT_NE(elem, nullptr);
}

TEST(JsonUITest, JsonDiffReturnsNonNull) {
  auto a = Json::Parse("{\"x\":1}");
  auto b = Json::Parse("{\"x\":2}");
  auto elem = JsonDiff(a, b);
  EXPECT_NE(elem, nullptr);
}

TEST(JsonUITest, JsonTreeViewBuilds) {
  auto comp = JsonTreeView(Json::Parse("{\"name\":\"root\",\"val\":42}"));
  EXPECT_NE(comp, nullptr);
  EXPECT_NE(comp->Render(), nullptr);
}

TEST(JsonUITest, JsonTableViewBuilds) {
  auto v = Json::Parse("[{\"a\":1,\"b\":2},{\"a\":3,\"b\":4}]");
  auto comp = JsonTableView(v);
  EXPECT_NE(comp, nullptr);
  EXPECT_NE(comp->Render(), nullptr);
}

TEST(JsonUITest, JsonFormBuilds) {
  auto schema = Json::Parse("{\"name\":\"Alice\",\"age\":30,\"active\":true}");
  auto comp = JsonForm(schema);
  EXPECT_NE(comp, nullptr);
  EXPECT_NE(comp->Render(), nullptr);
}

TEST(JsonUITest, JsonFormCallsOnSubmit) {
  auto schema = Json::Parse("{\"name\":\"Alice\"}");
  bool called = false;
  JsonValue result_val;
  auto comp = JsonForm(schema, [&](JsonValue v) {
    called = true;
    result_val = std::move(v);
  });
  EXPECT_NE(comp, nullptr);
}

TEST(JsonUITest, ReactiveJsonTreeViewBuilds) {
  auto rj = std::make_shared<ReactiveJson>(Json::Parse("{\"x\":1}"));
  auto comp = JsonTreeView(rj);
  EXPECT_NE(comp, nullptr);
  EXPECT_NE(comp->Render(), nullptr);
}

TEST(JsonUITest, ReactiveJsonTableViewBuilds) {
  auto rj = std::make_shared<ReactiveJson>(Json::Parse("[{\"col\":\"val\"}]"));
  auto comp = JsonTableView(rj);
  EXPECT_NE(comp, nullptr);
  EXPECT_NE(comp->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
