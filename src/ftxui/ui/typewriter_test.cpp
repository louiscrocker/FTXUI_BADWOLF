// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file typewriter_test.cpp
/// Unit tests for ftxui::ui::TypewriterText, TypewriterSequence, Console,
/// ConsolePrompt, and WithTypewriter.

#include "ftxui/ui/typewriter.hpp"

#include <chrono>
#include <memory>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "gtest/gtest.h"

using namespace ftxui;
using namespace ftxui::ui;

// ============================================================================
// TypewriterConfig defaults
// ============================================================================

TEST(TypewriterConfigTest, Defaults) {
  TypewriterConfig cfg;
  EXPECT_EQ(cfg.chars_per_second, 30);
  EXPECT_TRUE(cfg.show_cursor);
  EXPECT_EQ(cfg.cursor_char, '_');
  EXPECT_TRUE(cfg.cursor_blink);
  EXPECT_EQ(cfg.blink_hz, 2);
  EXPECT_FALSE(cfg.on_complete);
  EXPECT_FALSE(cfg.on_char);
}

// ============================================================================
// TypewriterText
// ============================================================================

TEST(TypewriterTextTest, CreatesWithoutCrash) {
  auto comp = TypewriterText("Hello, World!");
  ASSERT_NE(comp, nullptr);
}

TEST(TypewriterTextTest, RendersNonNullElement) {
  auto comp = TypewriterText("Hello");
  ASSERT_NE(comp, nullptr);
  auto elem = comp->Render();
  ASSERT_NE(elem, nullptr);
}

TEST(TypewriterTextTest, StartsWithPartialOrEmptyDisplay) {
  // At 30 cps, no frames have fired yet — displayed_count should be 0.
  TypewriterConfig cfg;
  cfg.chars_per_second = 30;
  auto comp = TypewriterText("Hello World", cfg);
  ASSERT_NE(comp, nullptr);
  // Just verify it renders without crash; content starts empty.
  auto elem = comp->Render();
  ASSERT_NE(elem, nullptr);
}

TEST(TypewriterTextTest, InstantDisplayWhenCpsZero) {
  TypewriterConfig cfg;
  cfg.chars_per_second = 0;
  bool complete_called = false;
  cfg.on_complete = [&] { complete_called = true; };
  auto comp = TypewriterText("Test", cfg);
  ASSERT_NE(comp, nullptr);
  // on_complete should fire immediately for cps <= 0.
  EXPECT_TRUE(complete_called);
}

// ============================================================================
// Console::Create
// ============================================================================

TEST(ConsoleTest, CreateFactory) {
  auto con = Console::Create();
  ASSERT_NE(con, nullptr);
}

TEST(ConsoleTest, CreateWithCustomMaxLines) {
  auto con = Console::Create(100);
  ASSERT_NE(con, nullptr);
  EXPECT_EQ(con->LineCount(), 0);
}

// ============================================================================
// Console::PrintLine
// ============================================================================

TEST(ConsoleTest, PrintLineAddsLine) {
  auto con = Console::Create();
  con->PrintLine("first line");
  EXPECT_EQ(con->LineCount(), 1);
}

// ============================================================================
// Console::LineCount
// ============================================================================

TEST(ConsoleTest, LineCountIncrements) {
  auto con = Console::Create();
  EXPECT_EQ(con->LineCount(), 0);
  con->PrintLine("a");
  EXPECT_EQ(con->LineCount(), 1);
  con->PrintLine("b");
  EXPECT_EQ(con->LineCount(), 2);
  con->PrintLine("c");
  EXPECT_EQ(con->LineCount(), 3);
}

// ============================================================================
// Console::Clear
// ============================================================================

TEST(ConsoleTest, ClearResetsToZero) {
  auto con = Console::Create();
  con->PrintLine("one");
  con->PrintLine("two");
  con->PrintLine("three");
  EXPECT_EQ(con->LineCount(), 3);
  con->Clear();
  EXPECT_EQ(con->LineCount(), 0);
}

// ============================================================================
// Log-level color helpers
// ============================================================================

TEST(ConsoleTest, InfoUsesCyanColor) {
  auto con = Console::Create();
  con->Info("test message");
  auto lines = con->Lines();
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0].fg, ftxui::Color::Cyan);
}

TEST(ConsoleTest, WarnUsesYellowColor) {
  auto con = Console::Create();
  con->Warn("test warning");
  auto lines = con->Lines();
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0].fg, ftxui::Color::Yellow);
}

TEST(ConsoleTest, ErrorUsesRedColor) {
  auto con = Console::Create();
  con->Error("test error");
  auto lines = con->Lines();
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0].fg, ftxui::Color::Red);
}

TEST(ConsoleTest, SuccessUsesGreenColor) {
  auto con = Console::Create();
  con->Success("test success");
  auto lines = con->Lines();
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0].fg, ftxui::Color::Green);
}

// ============================================================================
// Console::Build
// ============================================================================

TEST(ConsoleTest, BuildReturnsNonNull) {
  auto con = Console::Create();
  con->PrintLine("hello");
  auto comp = con->Build("Test Console");
  ASSERT_NE(comp, nullptr);
  auto elem = comp->Render();
  ASSERT_NE(elem, nullptr);
}

// ============================================================================
// ConsolePrompt
// ============================================================================

TEST(ConsolePromptTest, BuildsWithoutCrash) {
  auto con = Console::Create();
  ConsolePromptConfig cfg;
  cfg.handler = [](const std::string& cmd) -> std::string {
    return "echo: " + cmd;
  };
  auto prompt = ConsolePrompt(con, cfg);
  ASSERT_NE(prompt, nullptr);
  auto elem = prompt->Render();
  ASSERT_NE(elem, nullptr);
}

// ============================================================================
// TypewriterSequence
// ============================================================================

TEST(TypewriterSequenceTest, BuildsWithoutCrashEmptySequence) {
  TypewriterSequence seq;
  auto comp = seq.Build();
  ASSERT_NE(comp, nullptr);
}

TEST(TypewriterSequenceTest, ThenAddsStep) {
  TypewriterSequence seq;
  seq.Then("Hello");
  seq.Then("World");
  auto comp = seq.Build();
  ASSERT_NE(comp, nullptr);
  auto elem = comp->Render();
  ASSERT_NE(elem, nullptr);
}

TEST(TypewriterSequenceTest, PauseAddsPauseStep) {
  TypewriterSequence seq;
  seq.Then("Start");
  seq.Pause(std::chrono::milliseconds(500));
  seq.Then("End");
  auto comp = seq.Build();
  ASSERT_NE(comp, nullptr);
}

TEST(TypewriterSequenceTest, OnCompleteStoresCallback) {
  bool called = false;
  TypewriterSequence seq;
  seq.OnComplete([&] { called = true; });
  // Empty sequence — just verify it builds.
  auto comp = seq.Build();
  ASSERT_NE(comp, nullptr);
  // Callback has been stored (not yet fired since no animation ticks).
  EXPECT_FALSE(called);
}

TEST(TypewriterSequenceTest, BuildReturnsNonNull) {
  TypewriterSequence seq;
  seq.Then("INITIATING...");
  seq.Pause(std::chrono::milliseconds(200));
  seq.Then("DONE.");
  auto comp = seq.Build();
  ASSERT_NE(comp, nullptr);
  auto elem = comp->Render();
  ASSERT_NE(elem, nullptr);
}

// ============================================================================
// WithTypewriter
// ============================================================================

TEST(WithTypewriterTest, WrapsComponentWithoutCrash) {
  auto inner = Renderer([] { return ftxui::text("inner content"); });
  TypewriterConfig cfg;
  cfg.chars_per_second = 0;  // instant display so we can test transition
  auto wrapped = WithTypewriter(inner, "intro text", cfg);
  ASSERT_NE(wrapped, nullptr);
  auto elem = wrapped->Render();
  ASSERT_NE(elem, nullptr);
}
