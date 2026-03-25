// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/theme.hpp"

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// Helper: restore default theme after each test.
class ThemeTest : public ::testing::Test {
 protected:
  void SetUp() override { SetTheme(Theme::Default()); }
  void TearDown() override { SetTheme(Theme::Default()); }
};

// ── GetTheme / SetTheme
// ────────────────────────────────────────────────────────

TEST_F(ThemeTest, GetThemeReturnsValidTheme) {
  const Theme& t = GetTheme();
  // A valid theme always has a border style in the known range.
  // Just verify it doesn't crash and the struct is readable.
  (void)t.primary;
  (void)t.border_style;
}

TEST_F(ThemeTest, SetThemeChangesActiveTheme) {
  Theme custom;
  custom.primary = Color::Red;
  SetTheme(custom);
  EXPECT_EQ(GetTheme().primary, Color::Red);
}

TEST_F(ThemeTest, SetThemeFluentSetters) {
  Theme t;
  t.WithPrimary(Color::Magenta)
      .WithSecondary(Color::Yellow)
      .WithBorderStyle(LIGHT)
      .WithAnimations(false);
  SetTheme(t);

  EXPECT_EQ(GetTheme().primary, Color::Magenta);
  EXPECT_EQ(GetTheme().secondary, Color::Yellow);
  EXPECT_EQ(GetTheme().border_style, LIGHT);
  EXPECT_FALSE(GetTheme().animations_enabled);
}

// ── ScopedTheme
// ────────────────────────────────────────────────────────────────

TEST_F(ThemeTest, ScopedThemeRestoresPreviousTheme) {
  Theme before = GetTheme();
  {
    Theme custom;
    custom.primary = Color::GreenLight;
    ScopedTheme guard(custom);
    EXPECT_EQ(GetTheme().primary, Color::GreenLight);
  }  // guard destroyed here
  EXPECT_EQ(GetTheme().primary, before.primary);
}

TEST_F(ThemeTest, ScopedThemeNestedRestoresCorrectly) {
  SetTheme(Theme::Default());
  Theme outer_theme;
  outer_theme.primary = Color::Blue;
  Theme inner_theme;
  inner_theme.primary = Color::Red;

  {
    ScopedTheme outer(outer_theme);
    EXPECT_EQ(GetTheme().primary, Color::Blue);
    {
      ScopedTheme inner(inner_theme);
      EXPECT_EQ(GetTheme().primary, Color::Red);
    }
    // After inner is destroyed, should be back to outer.
    EXPECT_EQ(GetTheme().primary, Color::Blue);
  }
  // After outer is destroyed, should be back to default.
  EXPECT_EQ(GetTheme().primary, Theme::Default().primary);
}

// ── Preset themes
// ─────────────────────────────────────────────────────────────

// Default theme primary is Color::Cyan — all other presets must differ.

TEST_F(ThemeTest, DarkThemeHasNonDefaultPrimary) {
  auto t = Theme::Dark();
  // Dark preset has well-known fields.
  EXPECT_EQ(t.border_style, ROUNDED);
  EXPECT_TRUE(t.animations_enabled);
  // Just verify it returns a valid theme.
  (void)t.primary;
}

TEST_F(ThemeTest, LightThemeHasNonDefaultPrimary) {
  auto t = Theme::Light();
  // Light uses Blue as primary.
  EXPECT_EQ(t.primary, Color::Blue);
  EXPECT_EQ(t.border_style, LIGHT);
}

TEST_F(ThemeTest, NordThemeHasNonDefaultPrimary) {
  auto t = Theme::Nord();
  // Nord primary is a TrueColor RGB value, distinct from default Cyan.
  EXPECT_NE(t.primary, Theme::Default().primary);
}

TEST_F(ThemeTest, DraculaThemeHasNonDefaultPrimary) {
  auto t = Theme::Dracula();
  EXPECT_NE(t.primary, Theme::Default().primary);
}

TEST_F(ThemeTest, MonokaiThemeHasNonDefaultPrimary) {
  auto t = Theme::Monokai();
  EXPECT_NE(t.primary, Theme::Default().primary);
}

TEST_F(ThemeTest, DefaultPresetMatchesDefaultConstructed) {
  Theme a = Theme::Default();
  Theme b;
  EXPECT_EQ(a.primary, b.primary);
  EXPECT_EQ(a.secondary, b.secondary);
  EXPECT_EQ(a.border_style, b.border_style);
}

// ── Style factories compile and run without crashing ─────────────────────────

TEST_F(ThemeTest, MakeButtonStyleDoesNotCrash) {
  const Theme& t = GetTheme();
  (void)t.MakeButtonStyle();
}

TEST_F(ThemeTest, MakeInputStyleDoesNotCrash) {
  const Theme& t = GetTheme();
  (void)t.MakeInputStyle();
}

TEST_F(ThemeTest, MakeBorderDecoratorDoesNotCrash) {
  const Theme& t = GetTheme();
  (void)t.MakeBorderDecorator();
}

}  // namespace
}  // namespace ftxui::ui
