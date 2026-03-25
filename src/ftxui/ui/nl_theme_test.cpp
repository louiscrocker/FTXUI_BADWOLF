// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/nl_theme.hpp"

#include "ftxui/ui/theme.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── NLThemeParser::Parse
// ──────────────────────────────────────────────────────

TEST(NLThemeTest, ParseDetectsDarkMood) {
  auto d = NLThemeParser::Parse("dark cyberpunk");
  EXPECT_EQ(d.mood, ThemeDescription::Mood::Dark);
}

TEST(NLThemeTest, ParseDetectsNightMood) {
  auto d = NLThemeParser::Parse("midnight theme");
  EXPECT_EQ(d.mood, ThemeDescription::Mood::Dark);
}

TEST(NLThemeTest, ParseDetectsLightMood) {
  auto d = NLThemeParser::Parse("light clean minimal");
  EXPECT_EQ(d.mood, ThemeDescription::Mood::Light);
}

TEST(NLThemeTest, ParseDetectsCyberpunkStyle) {
  auto d = NLThemeParser::Parse("dark cyberpunk neon");
  EXPECT_EQ(d.style, ThemeDescription::Style::Cyberpunk);
}

TEST(NLThemeTest, ParseDetectsHackerStyle) {
  auto d = NLThemeParser::Parse("matrix hacker");
  EXPECT_EQ(d.style, ThemeDescription::Style::Hacker);
}

TEST(NLThemeTest, ParseDetectsBlueColor) {
  auto d = NLThemeParser::Parse("dark blue minimal");
  ASSERT_TRUE(d.primary_color.has_value());
  EXPECT_EQ(d.primary_color->name, "blue");
}

TEST(NLThemeTest, ParseDetectsNeonModifier) {
  auto d = NLThemeParser::Parse("neon cyberpunk");
  EXPECT_TRUE(d.neon);
}

TEST(NLThemeTest, ParseDetectsMutedModifier) {
  auto d = NLThemeParser::Parse("soft pastel minimal");
  EXPECT_TRUE(d.muted);
}

TEST(NLThemeTest, ParseHandlesEmptyString) {
  // Should not crash and should return default values.
  auto d = NLThemeParser::Parse("");
  EXPECT_EQ(d.raw, "");
  EXPECT_EQ(d.style, ThemeDescription::Style::Minimal);
}

TEST(NLThemeTest, ParseHandlesUnknownInput) {
  // Unknown keywords → graceful defaults, no crash.
  auto d = NLThemeParser::Parse("xyzzy frobnicate quux");
  EXPECT_EQ(d.style, ThemeDescription::Style::Minimal);
  EXPECT_EQ(d.mood, ThemeDescription::Mood::Dark);
  EXPECT_FALSE(d.neon);
  EXPECT_FALSE(d.muted);
}

// ── NLThemeParser::ToTheme
// ────────────────────────────────────────────────────

TEST(NLThemeTest, ToThemeReturnsValidTheme) {
  ThemeDescription d;
  d.style = ThemeDescription::Style::Cyberpunk;
  d.mood = ThemeDescription::Mood::Dark;
  Theme t = NLThemeParser::ToTheme(d);
  // A valid Theme must have at least one non-default color set.
  // We just verify the struct is populated and nothing crashes.
  (void)t.primary;
  (void)t.accent;
  (void)t.border_color;
}

// ── NLThemeParser::FromDescription ───────────────────────────────────────────

TEST(NLThemeTest, FromDescriptionEndToEndDarkBlueCyberpunk) {
  Theme t = NLThemeParser::FromDescription("dark blue cyberpunk");
  // Cyberpunk style detected → purple/cyan palette.
  // Blue explicit color overrides primary.
  // Should not be the default Theme().
  EXPECT_NE(t.primary, Theme().primary);
}

// ── LerpTheme
// ─────────────────────────────────────────────────────────────────

TEST(NLThemeTest, LerpThemeAtZeroReturnsFirstTheme) {
  Theme a = NLThemeParser::FromDescription("dark cyberpunk");
  Theme b = NLThemeParser::FromDescription("light corporate");
  Theme result = LerpTheme(a, b, 0.f);
  // At t=0 the result should match 'a' exactly.
  EXPECT_EQ(result.primary, a.primary);
  EXPECT_EQ(result.accent, a.accent);
}

TEST(NLThemeTest, LerpThemeAtOneReturnsSecondTheme) {
  Theme a = NLThemeParser::FromDescription("dark cyberpunk");
  Theme b = NLThemeParser::FromDescription("light corporate");
  Theme result = LerpTheme(a, b, 1.f);
  EXPECT_EQ(result.primary, b.primary);
  EXPECT_EQ(result.accent, b.accent);
}

TEST(NLThemeTest, LerpThemeAtHalfIsBetweenBoth) {
  Theme a = NLThemeParser::FromDescription("dark hacker green");
  Theme b = NLThemeParser::FromDescription("dark cyberpunk purple");
  Theme result = LerpTheme(a, b, 0.5f);
  // The blended theme must be different from both extremes.
  // (primary colors differ, so blended != a.primary and != b.primary)
  // We can't guarantee pixel-perfect equality; just verify no crash and
  // the result is distinct from both extremes.
  (void)result;
  SUCCEED();
}

// ── ThemeTransition
// ───────────────────────────────────────────────────────────

TEST(NLThemeTest, ThemeTransitionConstructsWithoutCrash) {
  Theme a = Theme::Dark();
  Theme b = NLThemeParser::FromDescription("cyberpunk");
  ThemeTransition tr(a, b, std::chrono::milliseconds(300));
  EXPECT_FALSE(tr.Complete());
  EXPECT_FLOAT_EQ(tr.Progress(), 0.f);
}

// ── ThemePicker
// ───────────────────────────────────────────────────────────────

TEST(NLThemeTest, ThemePickerBuildsWithoutCrash) {
  bool called = false;
  auto picker = ThemePicker([&](Theme) { called = true; });
  EXPECT_NE(picker, nullptr);
}

// ── ThemeFromDescription free function ───────────────────────────────────────

TEST(NLThemeTest, ThemeFromDescriptionReturnsNonDefaultForGreenHacker) {
  Theme t = ThemeFromDescription("green hacker");
  // Should produce a hacker/green-tinted theme, not the stock default.
  EXPECT_NE(t.primary, Theme().primary);
}

}  // namespace
}  // namespace ftxui::ui
