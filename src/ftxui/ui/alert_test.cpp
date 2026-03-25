// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/alert.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/ui/theme.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// Reset alert state before each test.
class AlertTest : public ::testing::Test {
 protected:
  void SetUp() override {
    AlertSystem::Instance().AllClear();
    SetTheme(Theme::Default());
  }
  void TearDown() override { AlertSystem::Instance().AllClear(); }
};

TEST_F(AlertTest, AlertSystem_DefaultIsAllClear) {
  EXPECT_EQ(AlertSystem::Instance().Level(), AlertLevel::AllClear);
}

TEST_F(AlertTest, AlertSystem_RedChangesLevel) {
  AlertSystem::Instance().Red("TEST");
  EXPECT_EQ(AlertSystem::Instance().Level(), AlertLevel::Red);
}

TEST_F(AlertTest, AlertSystem_YellowChangesLevel) {
  AlertSystem::Instance().Yellow("TEST");
  EXPECT_EQ(AlertSystem::Instance().Level(), AlertLevel::Yellow);
}

TEST_F(AlertTest, AlertSystem_BlueChangesLevel) {
  AlertSystem::Instance().Blue("TEST");
  EXPECT_EQ(AlertSystem::Instance().Level(), AlertLevel::Blue);
}

TEST_F(AlertTest, AlertSystem_AllClearResetsLevel) {
  AlertSystem::Instance().Red("TEST");
  AlertSystem::Instance().AllClear();
  EXPECT_EQ(AlertSystem::Instance().Level(), AlertLevel::AllClear);
}

TEST_F(AlertTest, AlertSystem_MessageSetOnRed) {
  AlertSystem::Instance().Red("KLINGON ATTACK");
  EXPECT_EQ(AlertSystem::Instance().Message(), "KLINGON ATTACK");
}

TEST_F(AlertTest, AlertSystem_MessageSetOnYellow) {
  AlertSystem::Instance().Yellow("SENSOR CONTACT");
  EXPECT_EQ(AlertSystem::Instance().Message(), "SENSOR CONTACT");
}

TEST_F(AlertTest, AlertSystem_MessageClearedOnAllClear) {
  AlertSystem::Instance().Red("DANGER");
  AlertSystem::Instance().AllClear();
  EXPECT_EQ(AlertSystem::Instance().Message(), "");
}

TEST_F(AlertTest, AlertSystem_DefaultMessageRed) {
  AlertSystem::Instance().Red();
  EXPECT_EQ(AlertSystem::Instance().Message(), "RED ALERT");
}

TEST_F(AlertTest, AlertSystem_DefaultMessageYellow) {
  AlertSystem::Instance().Yellow();
  EXPECT_EQ(AlertSystem::Instance().Message(), "YELLOW ALERT");
}

TEST_F(AlertTest, AlertSystem_OnChangeFiredOnRed) {
  int fired = 0;
  AlertLevel captured = AlertLevel::AllClear;
  AlertSystem::Instance().OnChange([&](AlertLevel lv, std::string /*msg*/) {
    ++fired;
    captured = lv;
  });
  AlertSystem::Instance().Red("TEST");
  // The AllClear in TearDown fires the listener too, but we only care about
  // the Red call here.
  EXPECT_GE(fired, 1);
  EXPECT_EQ(captured, AlertLevel::Red);
}

TEST_F(AlertTest, AlertSystem_OnChangeFiredOnAllClear) {
  AlertLevel last = AlertLevel::Red;
  AlertSystem::Instance().Red("A");
  AlertSystem::Instance().OnChange([&](AlertLevel lv, std::string /*msg*/) {
    last = lv;
  });
  AlertSystem::Instance().AllClear();
  EXPECT_EQ(last, AlertLevel::AllClear);
}

TEST_F(AlertTest, WithAlertOverlay_BuildsOk) {
  auto inner = Renderer([]() { return text("hello"); });
  auto wrapped = WithAlertOverlay(inner);
  EXPECT_NE(wrapped, nullptr);
}

TEST_F(AlertTest, FreeFunction_RedAlert) {
  RedAlert("CUSTOM");
  EXPECT_EQ(AlertSystem::Instance().Level(), AlertLevel::Red);
  EXPECT_EQ(AlertSystem::Instance().Message(), "CUSTOM");
}

TEST_F(AlertTest, FreeFunction_YellowAlert) {
  YellowAlert("WARN");
  EXPECT_EQ(AlertSystem::Instance().Level(), AlertLevel::Yellow);
}

TEST_F(AlertTest, FreeFunction_AllClear) {
  RedAlert();
  AllClear();
  EXPECT_EQ(AlertSystem::Instance().Level(), AlertLevel::AllClear);
}

}  // namespace
}  // namespace ftxui::ui
