// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/textinput.hpp"

#include <string>

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

TEST(TextInputTest, BuildWithLabelDoesNotCrash) {
  auto comp = TextInput("label").Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, BuildWithEmptyLabelDoesNotCrash) {
  auto comp = TextInput("").Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, BindRendersWithExternalString) {
  std::string value = "hello";
  auto comp = TextInput("Email").Bind(&value).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, BindEmptyStringRendersOk) {
  std::string value;
  auto comp = TextInput("Name").Bind(&value).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, MaxLengthRendersOk) {
  std::string value;
  auto comp = TextInput("Short").MaxLength(5).Bind(&value).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, ValidateRendersOk) {
  std::string value;
  auto comp = TextInput("Email")
                  .Bind(&value)
                  .Validate(
                      [](std::string_view s) {
                        return s.find('@') != std::string_view::npos;
                      },
                      "Must contain @")
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, PasswordModeRendersOk) {
  auto comp = TextInput("Password").Password().Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, PasswordFalseRendersOk) {
  auto comp = TextInput("Text").Password(false).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, PlaceholderRendersOk) {
  std::string value;
  auto comp =
      TextInput("Note").Bind(&value).Placeholder("Type here...").Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, OnChangeCallbackDoesNotCrashOnBuild) {
  bool called = false;
  std::string value;
  auto comp =
      TextInput("X").Bind(&value).OnChange([&] { called = true; }).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, OnSubmitCallbackDoesNotCrashOnBuild) {
  bool submitted = false;
  auto comp = TextInput("X").OnSubmit([&] { submitted = true; }).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, LabelWidthDoesNotCrash) {
  std::string value;
  auto comp = TextInput("Long Label").LabelWidth(30).Bind(&value).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(TextInputTest, AllOptionsTogetherBuildsOk) {
  std::string value;
  bool changed = false;
  bool submitted = false;
  auto comp = TextInput("Field")
                  .Bind(&value)
                  .Placeholder("Enter text")
                  .MaxLength(20)
                  .Validate([](std::string_view s) { return !s.empty(); },
                            "Cannot be empty")
                  .OnChange([&] { changed = true; })
                  .OnSubmit([&] { submitted = true; })
                  .LabelWidth(10)
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
