// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/state.hpp"

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

TEST(StateTest, DefaultValue) {
  State<int> s;
  EXPECT_EQ(s.Get(), 0);
}

TEST(StateTest, DefaultValueCustom) {
  State<int> s(42);
  EXPECT_EQ(s.Get(), 42);
}

TEST(StateTest, SetUpdatesValue) {
  State<int> s(0);
  s.Set(99);
  EXPECT_EQ(s.Get(), 99);
}

TEST(StateTest, MutateModifiesInPlace) {
  State<int> s(10);
  s.Mutate([](int& v) { v += 5; });
  EXPECT_EQ(s.Get(), 15);
}

TEST(StateTest, MutateString) {
  State<std::string> s("hello");
  s.Mutate([](std::string& v) { v += " world"; });
  EXPECT_EQ(s.Get(), "hello world");
}

TEST(StateTest, ShareReturnsSameData) {
  State<int> a(7);
  State<int> b = a.Share();

  // Both see the same value.
  EXPECT_EQ(b.Get(), 7);

  // Mutate through 'a', observe through 'b'.
  a.Set(100);
  EXPECT_EQ(b.Get(), 100);

  // Mutate through 'b', observe through 'a'.
  b.Set(200);
  EXPECT_EQ(a.Get(), 200);
}

TEST(StateTest, GetReturnsCopy) {
  State<int> s(5);
  int copy = s.Get();
  copy = 999;
  // Mutating the copy must not affect the state.
  EXPECT_EQ(s.Get(), 5);
}

TEST(StateTest, RefReturnsReference) {
  State<int> s(3);
  const int& ref = s.Ref();
  EXPECT_EQ(ref, 3);
  s.Set(42);
  EXPECT_EQ(ref, 42);  // Ref tracks the live value.
}

TEST(StateTest, DereferenceOperator) {
  State<int> s(21);
  EXPECT_EQ(*s, 21);
  *s = 22;
  EXPECT_EQ(s.Get(), 22);
}

TEST(StateTest, ArrowOperator) {
  State<std::string> s("hi");
  EXPECT_EQ(s->size(), 2u);
}

TEST(StateTest, PtrReturnsPointer) {
  State<int> s(8);
  int* p = s.Ptr();
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(*p, 8);
  *p = 16;
  EXPECT_EQ(s.Get(), 16);
}

}  // namespace
}  // namespace ftxui::ui
