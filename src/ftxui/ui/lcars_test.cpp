// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/lcars.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/ui/theme.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

class LCARSTest : public ::testing::Test {
 protected:
  void SetUp() override { SetTheme(Theme::Default()); }
  void TearDown() override { SetTheme(Theme::Default()); }
};

// ── Theme presets
// ─────────────────────────────────────────────────────────────

TEST_F(LCARSTest, Theme_LCARS_NotDefault) {
  auto t = Theme::LCARS();
  EXPECT_NE(t.primary, Theme::Default().primary);
}

TEST_F(LCARSTest, Theme_Imperial_NotDefault) {
  auto t = Theme::Imperial();
  EXPECT_NE(t.primary, Theme::Default().primary);
}

TEST_F(LCARSTest, Theme_Rebel_NotDefault) {
  auto t = Theme::Rebel();
  EXPECT_NE(t.primary, Theme::Default().primary);
}

TEST_F(LCARSTest, Theme_Enterprise_NotDefault) {
  auto t = Theme::Enterprise();
  EXPECT_NE(t.primary, Theme::Default().primary);
}

TEST_F(LCARSTest, Theme_Matrix_NotDefault) {
  auto t = Theme::Matrix();
  EXPECT_NE(t.primary, Theme::Default().primary);
}

// ── LCARSPanel
// ────────────────────────────────────────────────────────────────

TEST_F(LCARSTest, LCARSPanel_BuildsOk) {
  auto panel = LCARSPanel("STATUS", text("content"));
  EXPECT_NE(panel, nullptr);
  // Render it to ensure no crash.
  auto screen = Screen::Create(Dimension::Fixed(40), Dimension::Fixed(5));
  Render(screen, panel);
}

TEST_F(LCARSTest, LCARSPanel_WithExplicitColor) {
  auto panel = LCARSPanel("NAV", text("nav data"), Color::RGB(255, 153, 0));
  EXPECT_NE(panel, nullptr);
  auto screen = Screen::Create(Dimension::Fixed(40), Dimension::Fixed(5));
  Render(screen, panel);
}

// ── LCARSBar
// ──────────────────────────────────────────────────────────────────

TEST_F(LCARSTest, LCARSBar_EmptySegments) {
  auto bar = LCARSBar({});
  EXPECT_NE(bar, nullptr);
  auto screen = Screen::Create(Dimension::Fixed(40), Dimension::Fixed(2));
  Render(screen, bar);
}

TEST_F(LCARSTest, LCARSBar_MultipleSegments) {
  auto bar = LCARSBar({{"SHIELDS", Color::RGB(255, 153, 0)},
                       {"WEAPONS", Color::RGB(153, 153, 255)}});
  EXPECT_NE(bar, nullptr);
  auto screen = Screen::Create(Dimension::Fixed(60), Dimension::Fixed(2));
  Render(screen, bar);
}

// ── LCARSReadout ─────────────────────────────────────────────────────────────

TEST_F(LCARSTest, LCARSReadout_BuildsOk) {
  auto el = LCARSReadout("STARDATE", "47634.4");
  EXPECT_NE(el, nullptr);
  auto screen = Screen::Create(Dimension::Fixed(40), Dimension::Fixed(2));
  Render(screen, el);
}

TEST_F(LCARSTest, LCARSReadout_WithColor) {
  auto el = LCARSReadout("STATUS", "NOMINAL", Color::RGB(153, 255, 153));
  EXPECT_NE(el, nullptr);
  auto screen = Screen::Create(Dimension::Fixed(40), Dimension::Fixed(2));
  Render(screen, el);
}

// ── LCARSButton ──────────────────────────────────────────────────────────────

TEST_F(LCARSTest, LCARSButton_BuildsOk) {
  bool clicked = false;
  auto btn = LCARSButton("ENGAGE", [&clicked] { clicked = true; });
  EXPECT_NE(btn, nullptr);
}

TEST_F(LCARSTest, LCARSButton_WithColor) {
  auto btn = LCARSButton("FIRE", [] {}, Color::RGB(255, 51, 51));
  EXPECT_NE(btn, nullptr);
}

// ── LCARSScreen ──────────────────────────────────────────────────────────────

TEST_F(LCARSTest, LCARSScreen_BuildsOk) {
  auto sidebar = Renderer([] { return text("sidebar"); });
  auto main = Renderer([] { return text("main"); });
  auto screen = LCARSScreen("ENTERPRISE", sidebar, main);
  EXPECT_NE(screen, nullptr);
}

}  // namespace
}  // namespace ftxui::ui
