// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/keymap.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// Helper: wrap a trivial Renderer with a Keymap and send an event.
// Returns true if the callback for that binding fired.
bool KeyFires(const std::string& key_str, ftxui::Event event_to_send) {
  bool fired = false;
  auto base = ftxui::Renderer([] { return ftxui::text(""); });
  Keymap km;
  km.Bind(key_str, [&] { fired = true; }, "test");
  auto wrapped = km.Wrap(base);
  wrapped->OnEvent(event_to_send);
  return fired;
}

// ── Single character bindings ──────────────────────────────────────────────────

TEST(KeymapTest, SingleCharQ) {
  EXPECT_TRUE(KeyFires("q", ftxui::Event::Character('q')));
}

TEST(KeymapTest, SingleCharSlash) {
  EXPECT_TRUE(KeyFires("/", ftxui::Event::Character('/')));
}

TEST(KeymapTest, SingleCharDoesNotFireOnWrongChar) {
  EXPECT_FALSE(KeyFires("q", ftxui::Event::Character('w')));
}

// ── Named keys ─────────────────────────────────────────────────────────────────

TEST(KeymapTest, EscapeKey) {
  EXPECT_TRUE(KeyFires("Escape", ftxui::Event::Escape));
}

TEST(KeymapTest, EnterKey) {
  EXPECT_TRUE(KeyFires("Enter", ftxui::Event::Return));
}

TEST(KeymapTest, ReturnAlias) {
  EXPECT_TRUE(KeyFires("Return", ftxui::Event::Return));
}

TEST(KeymapTest, TabKey) {
  EXPECT_TRUE(KeyFires("Tab", ftxui::Event::Tab));
}

TEST(KeymapTest, BackspaceKey) {
  EXPECT_TRUE(KeyFires("Backspace", ftxui::Event::Backspace));
}

TEST(KeymapTest, DeleteKey) {
  EXPECT_TRUE(KeyFires("Delete", ftxui::Event::Delete));
}

TEST(KeymapTest, UpKey) {
  EXPECT_TRUE(KeyFires("Up", ftxui::Event::ArrowUp));
}

TEST(KeymapTest, DownKey) {
  EXPECT_TRUE(KeyFires("Down", ftxui::Event::ArrowDown));
}

TEST(KeymapTest, LeftKey) {
  EXPECT_TRUE(KeyFires("Left", ftxui::Event::ArrowLeft));
}

TEST(KeymapTest, RightKey) {
  EXPECT_TRUE(KeyFires("Right", ftxui::Event::ArrowRight));
}

// ── Function keys ──────────────────────────────────────────────────────────────

TEST(KeymapTest, F1Key) {
  EXPECT_TRUE(KeyFires("F1", ftxui::Event::F1));
}

TEST(KeymapTest, F12Key) {
  EXPECT_TRUE(KeyFires("F12", ftxui::Event::F12));
}

// ── Ctrl combos ────────────────────────────────────────────────────────────────

TEST(KeymapTest, CtrlC) {
  EXPECT_TRUE(KeyFires("Ctrl+c", ftxui::Event::CtrlC));
}

TEST(KeymapTest, CtrlCUpperCase) {
  EXPECT_TRUE(KeyFires("Ctrl+C", ftxui::Event::CtrlC));
}

TEST(KeymapTest, CtrlS) {
  EXPECT_TRUE(KeyFires("Ctrl+s", ftxui::Event::CtrlS));
}

TEST(KeymapTest, CtrlZ) {
  EXPECT_TRUE(KeyFires("Ctrl+z", ftxui::Event::CtrlZ));
}

// ── Unknown key ────────────────────────────────────────────────────────────────

TEST(KeymapTest, UnknownKeyDoesNotCrash) {
  // An unknown key string should produce no match but not crash.
  EXPECT_FALSE(KeyFires("NotAKey", ftxui::Event::Character('x')));
}

TEST(KeymapTest, UnknownKeyDoesNotFireOnCustomEvent) {
  // The sentinel event (Event::Custom) should not match anything normal.
  EXPECT_FALSE(KeyFires("NotAKey", ftxui::Event::Return));
}

// ── Multiple bindings ──────────────────────────────────────────────────────────

TEST(KeymapTest, MultipleBindingsIndependent) {
  int q_count = 0;
  int esc_count = 0;
  auto base = ftxui::Renderer([] { return ftxui::text(""); });
  Keymap km;
  km.Bind("q", [&] { q_count++; });
  km.Bind("Escape", [&] { esc_count++; });
  auto wrapped = km.Wrap(base);

  wrapped->OnEvent(ftxui::Event::Character('q'));
  EXPECT_EQ(q_count, 1);
  EXPECT_EQ(esc_count, 0);

  wrapped->OnEvent(ftxui::Event::Escape);
  EXPECT_EQ(q_count, 1);
  EXPECT_EQ(esc_count, 1);
}

// ── AsDecorator ────────────────────────────────────────────────────────────────

TEST(KeymapTest, AsDecoratorAppliesBindings) {
  bool fired = false;
  auto base = ftxui::Renderer([] { return ftxui::text(""); });
  Keymap km;
  km.Bind("q", [&] { fired = true; });
  auto wrapped = base | km.AsDecorator();
  wrapped->OnEvent(ftxui::Event::Character('q'));
  EXPECT_TRUE(fired);
}

// ── HelpElement ────────────────────────────────────────────────────────────────

TEST(KeymapTest, HelpElementDoesNotCrash) {
  Keymap km;
  km.Bind("q", [] {}, "Quit");
  km.Bind("Ctrl+s", [] {}, "Save");
  auto elem = km.HelpElement();
  ASSERT_NE(elem, nullptr);
}

TEST(KeymapTest, EmptyKeymapHelpElementDoesNotCrash) {
  Keymap km;
  auto elem = km.HelpElement();
  ASSERT_NE(elem, nullptr);
}

}  // namespace
}  // namespace ftxui::ui
