// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/registry.hpp"

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// Helper: build a fresh registry with known entries to avoid singleton
// pollution between tests.  We test Registry methods directly on a local
// instance by calling the public API through the singleton, but reset state
// between each test fixture via a tiny helper that registers then unregisters.

// We use a simple counter component factory.
ftxui::Component MakeStub() {
  return ftxui::Container::Vertical({});
}

// --------------------------------------------------------------------------
// Singleton
// --------------------------------------------------------------------------

TEST(RegistryTest, SingletonIsStable) {
  Registry& a = Registry::Get();
  Registry& b = Registry::Get();
  EXPECT_EQ(&a, &b);
}

// --------------------------------------------------------------------------
// Register + List
// --------------------------------------------------------------------------

TEST(RegistryTest, RegisterAndListSingle) {
  Registry& reg = Registry::Get();
  const size_t before = reg.List().size();

  reg.Register({"test-alpha", "1.0.0", "Alpha test component", "tester", {}},
               MakeStub);

  const auto list = reg.List();
  EXPECT_EQ(list.size(), before + 1);

  bool found = false;
  for (const auto& m : list) {
    if (m.name == "test-alpha") {
      found = true;
      EXPECT_EQ(m.version, "1.0.0");
      EXPECT_EQ(m.description, "Alpha test component");
      EXPECT_EQ(m.author, "tester");
    }
  }
  EXPECT_TRUE(found);
}

TEST(RegistryTest, RegisterMultiple) {
  Registry& reg = Registry::Get();
  const size_t before = reg.List().size();

  reg.Register({"test-beta", "2.0.0", "Beta", "b", {}}, MakeStub);
  reg.Register({"test-gamma", "3.0.0", "Gamma", "g", {}}, MakeStub);

  EXPECT_GE(reg.List().size(), before + 2);
}

TEST(RegistryTest, RegisterReplacesDuplicate) {
  Registry& reg = Registry::Get();

  reg.Register({"test-delta", "1.0.0", "Old", "x", {}}, MakeStub);
  const size_t after_first = reg.List().size();

  reg.Register({"test-delta", "2.0.0", "New", "y", {}}, MakeStub);
  const size_t after_second = reg.List().size();

  // No duplicate — same count.
  EXPECT_EQ(after_first, after_second);

  // Version updated.
  auto list = reg.List();
  for (const auto& m : list) {
    if (m.name == "test-delta") {
      EXPECT_EQ(m.version, "2.0.0");
      EXPECT_EQ(m.description, "New");
    }
  }
}

// --------------------------------------------------------------------------
// Search by name
// --------------------------------------------------------------------------

TEST(RegistryTest, SearchByName) {
  Registry& reg = Registry::Get();
  reg.Register({"search-needle", "1.0.0", "Has needle in name", "t", {}},
               MakeStub);
  reg.Register({"search-haystack", "1.0.0", "No match here", "t", {}},
               MakeStub);

  auto results = reg.Search("needle");
  bool found = false;
  for (const auto& m : results) {
    if (m.name == "search-needle") {
      found = true;
    }
    EXPECT_NE(m.name, "search-haystack");
  }
  EXPECT_TRUE(found);
}

TEST(RegistryTest, SearchCaseInsensitive) {
  Registry& reg = Registry::Get();
  reg.Register({"ci-widget", "1.0.0", "Case Insensitive Test", "t", {}},
               MakeStub);

  EXPECT_FALSE(reg.Search("CASE").empty());
  EXPECT_FALSE(reg.Search("case").empty());
  EXPECT_FALSE(reg.Search("Case").empty());
}

// --------------------------------------------------------------------------
// Search by tag
// --------------------------------------------------------------------------

TEST(RegistryTest, SearchByTag) {
  Registry& reg = Registry::Get();
  reg.Register({"tagged-comp",
                "1.0.0",
                "Tagged component",
                "t",
                {"visualization", "chart"}},
               MakeStub);

  auto results = reg.Search("visualization");
  bool found = false;
  for (const auto& m : results) {
    if (m.name == "tagged-comp") {
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST(RegistryTest, SearchByTagPartial) {
  Registry& reg = Registry::Get();
  reg.Register(
      {"partial-tag-comp", "1.0.0", "Partial tag test", "t", {"filesystem"}},
      MakeStub);

  EXPECT_FALSE(reg.Search("file").empty());
}

// --------------------------------------------------------------------------
// Create component
// --------------------------------------------------------------------------

TEST(RegistryTest, CreateKnownComponent) {
  Registry& reg = Registry::Get();
  reg.Register({"create-me", "1.0.0", "Create test", "t", {}}, MakeStub);

  auto comp = reg.Create("create-me");
  EXPECT_NE(comp, nullptr);
}

TEST(RegistryTest, CreateUnknownReturnsNull) {
  Registry& reg = Registry::Get();
  auto comp = reg.Create("does-not-exist-xyz");
  EXPECT_EQ(comp, nullptr);
}

}  // namespace
}  // namespace ftxui::ui
