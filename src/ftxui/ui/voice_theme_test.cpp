// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/voice_theme.hpp"

#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── 1. VoiceThemeConfigDefaults
// ───────────────────────────────────────────────
TEST(VoiceThemeConfigTest, Defaults) {
  VoiceThemeConfig cfg;
  EXPECT_EQ(cfg.transition_duration, std::chrono::milliseconds(800));
  EXPECT_TRUE(cfg.announce_theme);
  EXPECT_FALSE(cfg.continuous_listening);
  EXPECT_GE(cfg.wake_words.size(), 3u);
}

// ── 2. VoiceThemeCommandDefaults
// ──────────────────────────────────────────────
TEST(VoiceThemeCommandTest, Defaults) {
  VoiceThemeCommand cmd;
  EXPECT_FALSE(cmd.is_theme_command);
  EXPECT_FLOAT_EQ(cmd.confidence, 0.0f);
}

// ── 3. ParseCommandWithApply
// ──────────────────────────────────────────────────
TEST(VoiceThemeParseTest, ParseCommandWithApply) {
  auto cmd = VoiceThemeController::ParseCommand("apply the matrix theme");
  EXPECT_TRUE(cmd.is_theme_command);
  EXPECT_EQ(cmd.theme_name, "matrix");
}

// ── 4. ParseCommandWithActivateLcars ─────────────────────────────────────────
TEST(VoiceThemeParseTest, ParseCommandWithActivateLcars) {
  auto cmd = VoiceThemeController::ParseCommand("activate lcars");
  EXPECT_TRUE(cmd.is_theme_command);
  EXPECT_EQ(cmd.theme_name, "lcars");
}

// ── 5. ParseCommandWithSwitchTo
// ───────────────────────────────────────────────
TEST(VoiceThemeParseTest, ParseCommandWithSwitchTo) {
  auto cmd =
      VoiceThemeController::ParseCommand("switch to dark blue cyberpunk");
  EXPECT_TRUE(cmd.is_theme_command);
  EXPECT_EQ(cmd.theme_name, "dark blue cyberpunk");
}

// ── 6. ParseCommandNoWakeWord
// ─────────────────────────────────────────────────
TEST(VoiceThemeParseTest, ParseCommandNoWakeWord) {
  auto cmd = VoiceThemeController::ParseCommand("hello world");
  EXPECT_FALSE(cmd.is_theme_command);
}

// ── 7. ParseCommandCaseInsensitive
// ────────────────────────────────────────────
TEST(VoiceThemeParseTest, ParseCommandCaseInsensitive) {
  auto cmd = VoiceThemeController::ParseCommand("Apply THE Matrix");
  EXPECT_TRUE(cmd.is_theme_command);
  EXPECT_EQ(cmd.theme_name, "matrix");
}

// ── 8. ParseCommandWithUse
// ────────────────────────────────────────────────────
TEST(VoiceThemeParseTest, ParseCommandWithUse) {
  auto cmd = VoiceThemeController::ParseCommand("use enterprise");
  EXPECT_TRUE(cmd.is_theme_command);
  EXPECT_EQ(cmd.theme_name, "enterprise");
}

// ── 9. NamedThemeMatrix
// ───────────────────────────────────────────────────────
TEST(VoiceThemeNamedTest, NamedThemeMatrix) {
  Theme t = VoiceThemeController::NamedTheme("matrix");
  EXPECT_NE(t.primary, Theme::Default().text);
}

// ── 10. NamedThemeLcars
// ───────────────────────────────────────────────────────
TEST(VoiceThemeNamedTest, NamedThemeLcars) {
  Theme t = VoiceThemeController::NamedTheme("lcars");
  EXPECT_NE(t.primary, Theme::Default().text);
}

// ── 11. NamedThemeUnknown
// ─────────────────────────────────────────────────────
TEST(VoiceThemeNamedTest, NamedThemeUnknown) {
  Theme t;
  EXPECT_NO_THROW(
      { t = VoiceThemeController::NamedTheme("blue retro cyber"); });
  (void)t;
}

// ── 12. AvailableThemeNamesHas8Plus
// ───────────────────────────────────────────
TEST(VoiceThemeNamesTest, AvailableThemeNamesHas8Plus) {
  auto names = VoiceThemeController::AvailableThemeNames();
  EXPECT_GE(names.size(), 8u);
}

// ── 13. ControllerConstructsWithoutCrash
// ──────────────────────────────────────
TEST(VoiceThemeControllerTest, ConstructsWithoutCrash) {
  EXPECT_NO_THROW({ VoiceThemeController ctrl; });
}

// ── 14. LiveThemeBarBuildsWithoutCrash
// ────────────────────────────────────────
TEST(VoiceThemeComponentTest, LiveThemeBarBuildsWithoutCrash) {
  ftxui::Component bar;
  EXPECT_NO_THROW({ bar = LiveThemeBar(); });
  EXPECT_NE(bar, nullptr);
}

// ── 15. ThemeStageBuildsWithoutCrash ─────────────────────────────────────────
TEST(VoiceThemeComponentTest, ThemeStageBuildsWithoutCrash) {
  ftxui::Component stage;
  EXPECT_NO_THROW({ stage = ThemeStage(); });
  EXPECT_NE(stage, nullptr);
}

// ── 16. ThemeShowcaseBuildsWithoutCrash
// ───────────────────────────────────────
TEST(VoiceThemeComponentTest, ThemeShowcaseBuildsWithoutCrash) {
  ftxui::Component showcase;
  EXPECT_NO_THROW({ showcase = ThemeShowcase(false); });
  EXPECT_NE(showcase, nullptr);
}

// ── 17. WithLiveThemesReturnsNonNull
// ──────────────────────────────────────────
TEST(VoiceThemeComponentTest, WithLiveThemesReturnsNonNull) {
  auto inner = ftxui::Renderer([] { return ftxui::text("hi"); });
  ftxui::Component wrapped;
  EXPECT_NO_THROW({ wrapped = WithLiveThemes(inner); });
  EXPECT_NE(wrapped, nullptr);
}

// ── 18. ReactiveThemeGlobalIsSingleton
// ────────────────────────────────────────
TEST(ReactiveThemeTest, GlobalIsSingleton) {
  EXPECT_EQ(&ReactiveTheme::Global(), &ReactiveTheme::Global());
}

// ── 19. ReactiveThemeGetAndSet
// ────────────────────────────────────────────────
TEST(ReactiveThemeTest, GetAndSet) {
  Theme matrix = Theme::Matrix();
  ReactiveTheme::Global().Set(matrix, /*animate=*/false);
  const Theme& got = ReactiveTheme::Global().Get();
  EXPECT_EQ(got.primary, matrix.primary);
}

}  // namespace
}  // namespace ftxui::ui
