// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/form.hpp"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

TEST(FormTest, EmptyFormBuildsAndRenders) {
  auto comp = Form().Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, FieldBuildsAndRenders) {
  std::string name;
  auto comp = Form().Field("Name", &name, "Your name").Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, PasswordBuildsAndRenders) {
  std::string pass;
  auto comp = Form().Password("Pass", &pass).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, MultilineBuildsAndRenders) {
  std::string bio;
  auto comp = Form().Multiline("Bio", &bio, "Write something...").Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, CheckBuildsAndRenders) {
  bool checked = false;
  auto comp = Form().Check("Subscribe", &checked).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, SubmitBuildsAndRenders) {
  bool submitted = false;
  auto comp =
      Form().Submit("OK", [&] { submitted = true; }).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, TitleBuildsAndRenders) {
  auto comp = Form().Title("My Form").Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, CancelBuildsAndRenders) {
  auto comp = Form().Cancel("Cancel", [] {}).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, SectionAndSeparatorRender) {
  std::string val;
  auto comp = Form()
                  .Section("Personal")
                  .Field("Name", &val)
                  .Separator()
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, SelectBuildsAndRenders) {
  std::vector<std::string> opts = {"A", "B", "C"};
  int selected = 0;
  auto comp = Form().Select("Choice", &opts, &selected).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, RadioBuildsAndRenders) {
  std::vector<std::string> opts = {"X", "Y"};
  int selected = 0;
  auto comp = Form().Radio("Pick", &opts, &selected).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, IntegerSliderBuildsAndRenders) {
  int val = 50;
  auto comp = Form().Integer("Count", &val, 0, 100, 5).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, FloatSliderBuildsAndRenders) {
  float val = 0.5f;
  auto comp = Form().Float("Level", &val, 0.f, 1.f, 0.1f).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, LabelWidthDoesNotCrash) {
  std::string name;
  auto comp = Form().LabelWidth(20).Field("Name", &name).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, AllFieldTypesTogetherBuildsAndRenders) {
  std::string name, email, password;
  bool subscribe = false;
  std::vector<std::string> roles = {"Admin", "User"};
  int role_idx = 0;

  auto comp = Form()
                  .Title("Sign Up")
                  .Field("Name", &name, "Full name")
                  .Field("Email", &email, "you@example.com")
                  .Password("Password", &password)
                  .Check("Subscribe", &subscribe)
                  .Select("Role", &roles, &role_idx)
                  .Submit("Register", [] {})
                  .Cancel("Cancel", [] {})
                  .Build();

  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(FormTest, ImplicitComponentConversion) {
  std::string val;
  ftxui::Component comp = Form().Field("X", &val);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
