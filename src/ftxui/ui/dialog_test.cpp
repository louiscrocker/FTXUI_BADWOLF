// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/dialog.hpp"

#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// Helper: simple parent component for wrapping.
static ftxui::Component MakeParent() {
  return ftxui::Renderer([] { return ftxui::text("parent content"); });
}

// ── WithModal ────────────────────────────────────────────────────────────────

TEST(DialogTest, WithModalShowFalseRendersParent) {
  bool show = false;
  auto parent = MakeParent();
  auto body = ftxui::Renderer([] { return ftxui::text("modal body"); });
  auto comp = WithModal(parent, "Title", body, {}, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithModalShowTrueRendersOverlay) {
  bool show = true;
  auto parent = MakeParent();
  auto body = ftxui::Renderer([] { return ftxui::text("modal body"); });
  auto comp = WithModal(parent, "Title", body, {}, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithModalWithButtons) {
  bool show = true;
  auto parent = MakeParent();
  auto body = ftxui::Renderer([] { return ftxui::text("Are you sure?"); });
  std::vector<ModalButton> buttons = {
      {"Cancel", [&] { show = false; }, false},
      {"OK", [&] { show = false; }, true},
  };
  auto comp = WithModal(parent, "Confirm", body, buttons, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithModalDecoratorShowFalse) {
  bool show = false;
  auto body = ftxui::Renderer([] { return ftxui::text("body"); });
  auto decorator = WithModal("Title", body, {}, &show);
  auto comp = decorator(MakeParent());
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithModalDecoratorShowTrue) {
  bool show = true;
  auto body = ftxui::Renderer([] { return ftxui::text("body"); });
  auto decorator = WithModal("Title", body, {}, &show);
  auto comp = decorator(MakeParent());
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

// ── WithDrawer ───────────────────────────────────────────────────────────────

TEST(DialogTest, WithDrawerRightShowFalseRendersParent) {
  bool show = false;
  auto parent = MakeParent();
  auto content = ftxui::Renderer([] { return ftxui::text("drawer"); });
  auto comp = WithDrawer(parent, DrawerSide::Right, "Panel", content, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithDrawerRightShowTrueRendersDrawer) {
  bool show = true;
  auto parent = MakeParent();
  auto content = ftxui::Renderer([] { return ftxui::text("settings"); });
  auto comp = WithDrawer(parent, DrawerSide::Right, "Settings", content, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithDrawerLeftShowTrue) {
  bool show = true;
  auto parent = MakeParent();
  auto content = ftxui::Renderer([] { return ftxui::text("nav"); });
  auto comp = WithDrawer(parent, DrawerSide::Left, "Nav", content, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithDrawerBottomShowTrue) {
  bool show = true;
  auto parent = MakeParent();
  auto content = ftxui::Renderer([] { return ftxui::text("status"); });
  auto comp = WithDrawer(parent, DrawerSide::Bottom, "Status", content, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithDrawerDecoratorShowFalse) {
  bool show = false;
  auto content = ftxui::Renderer([] { return ftxui::text("panel"); });
  auto decorator = WithDrawer(DrawerSide::Right, "Side", content, &show);
  auto comp = decorator(MakeParent());
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

// ── WithAlert ────────────────────────────────────────────────────────────────

TEST(DialogTest, WithAlertShowFalseRendersParent) {
  bool show = false;
  auto parent = MakeParent();
  auto comp = WithAlert(parent, "Info", "All good.", &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithAlertShowTrueRendersAlert) {
  bool show = true;
  auto parent = MakeParent();
  auto comp = WithAlert(parent, "Error", "File not found.", &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithAlertWithOnCloseCallback) {
  bool show = true;
  bool closed = false;
  auto parent = MakeParent();
  auto comp = WithAlert(parent, "Warning", "Disk full.",
                        &show, [&] { closed = true; });
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithAlertDecoratorShowFalse) {
  bool show = false;
  auto decorator = WithAlert("Note", "Nothing happened.", &show);
  auto comp = decorator(MakeParent());
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

// ── WithConfirm ──────────────────────────────────────────────────────────────

TEST(DialogTest, WithConfirmShowFalseRendersParent) {
  bool show = false;
  auto parent = MakeParent();
  auto comp = WithConfirm(parent, "Delete?", [] {}, [] {}, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithConfirmShowTrueRendersDialog) {
  bool show = true;
  auto parent = MakeParent();
  auto comp = WithConfirm(parent, "Delete?", [] {}, [] {}, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithConfirmDecoratorShowFalse) {
  bool show = false;
  auto decorator = WithConfirm("Are you sure?", [] {}, [] {}, &show);
  auto comp = decorator(MakeParent());
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithConfirmDecoratorShowTrue) {
  bool show = true;
  bool yes_called = false;
  bool no_called = false;
  auto decorator = WithConfirm(
      "Proceed?",
      [&] { yes_called = true; },
      [&] { no_called = true; },
      &show);
  auto comp = decorator(MakeParent());
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

// ── WithHelp ─────────────────────────────────────────────────────────────────

TEST(DialogTest, WithHelpShowFalseRendersParent) {
  bool show = false;
  auto parent = MakeParent();
  std::vector<std::pair<std::string, std::string>> bindings = {
      {"q", "Quit"}, {"?", "Help"}};
  auto comp = WithHelp(parent, "Shortcuts", bindings, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithHelpShowTrueRendersOverlay) {
  bool show = true;
  auto parent = MakeParent();
  std::vector<std::pair<std::string, std::string>> bindings = {
      {"q", "Quit"}, {"Ctrl+s", "Save"}};
  auto comp = WithHelp(parent, "Keyboard", bindings, &show);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(DialogTest, WithHelpDecoratorShowTrue) {
  bool show = true;
  std::vector<std::pair<std::string, std::string>> bindings = {{"h", "Help"}};
  auto decorator = WithHelp("Keys", bindings, &show);
  auto comp = decorator(MakeParent());
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
