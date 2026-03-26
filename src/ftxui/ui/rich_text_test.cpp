// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "ftxui/ui/rich_text.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── TextStyle
// ─────────────────────────────────────────────────────────────────

TEST(TextStyleTest, DefaultNoAttributes) {
  TextStyle s;
  EXPECT_FALSE(s.has_fg());
  EXPECT_FALSE(s.has_bg());
  EXPECT_FALSE(s.is_bold());
  EXPECT_FALSE(s.is_dim());
  EXPECT_FALSE(s.is_italic());
  EXPECT_FALSE(s.is_underline());
  EXPECT_FALSE(s.is_blink());
  EXPECT_FALSE(s.is_rapid_blink());
  EXPECT_FALSE(s.is_reverse());
  EXPECT_FALSE(s.is_hidden());
  EXPECT_FALSE(s.is_strikethrough());
}

TEST(TextStyleTest, BoldSetsFlag) {
  TextStyle s;
  s.Bold();
  EXPECT_TRUE(s.is_bold());
  EXPECT_FALSE(s.is_dim());
}

TEST(TextStyleTest, FgColor) {
  TextStyle s;
  s.Fg(Color::Palette16::Red);
  EXPECT_TRUE(s.has_fg());
  EXPECT_FALSE(s.has_bg());
}

TEST(TextStyleTest, BgColor) {
  TextStyle s;
  s.Bg(Color::Palette16::Blue);
  EXPECT_FALSE(s.has_fg());
  EXPECT_TRUE(s.has_bg());
}

TEST(TextStyleTest, FgTrueColor) {
  TextStyle s;
  s.Fg(255, 128, 0);
  EXPECT_TRUE(s.has_fg());
}

TEST(TextStyleTest, ReverseSetsFlag) {
  TextStyle s;
  s.Reverse();
  EXPECT_TRUE(s.is_reverse());
  EXPECT_FALSE(s.is_bold());
}

TEST(TextStyleTest, ApplyReturnsElement) {
  TextStyle s;
  s.Bold().Red();
  auto el = ftxui::text("hello");
  auto result = s.Apply(el);
  EXPECT_NE(result, nullptr);
}

TEST(TextStyleTest, CallOperatorStringReturnsElement) {
  TextStyle s;
  s.Dim().Cyan();
  auto el = s("world");
  EXPECT_NE(el, nullptr);
}

TEST(TextStyleTest, Chaining) {
  TextStyle s;
  s.Bold().Red().Underline();
  EXPECT_TRUE(s.is_bold());
  EXPECT_TRUE(s.has_fg());
  EXPECT_TRUE(s.is_underline());
}

// ── RichText::Parse
// ───────────────────────────────────────────────────────────

TEST(RichTextParseTest, EmptyString) {
  auto spans = RichText::Parse("");
  EXPECT_TRUE(spans.empty());
}

TEST(RichTextParseTest, PlainText) {
  auto spans = RichText::Parse("hello");
  ASSERT_EQ(spans.size(), 1u);
  EXPECT_EQ(spans[0].text, "hello");
  EXPECT_FALSE(spans[0].style.is_bold());
}

TEST(RichTextParseTest, BoldTag) {
  auto spans = RichText::Parse("[bold]text[/bold]");
  ASSERT_EQ(spans.size(), 1u);
  EXPECT_EQ(spans[0].text, "text");
  EXPECT_TRUE(spans[0].style.is_bold());
}

TEST(RichTextParseTest, RedTag) {
  auto spans = RichText::Parse("[red]text[/red]");
  ASSERT_EQ(spans.size(), 1u);
  EXPECT_EQ(spans[0].text, "text");
  EXPECT_TRUE(spans[0].style.has_fg());
}

TEST(RichTextParseTest, NestedBoldRed) {
  auto spans = RichText::Parse("[bold][red]text[/][/]");
  ASSERT_EQ(spans.size(), 1u);
  EXPECT_EQ(spans[0].text, "text");
  EXPECT_TRUE(spans[0].style.is_bold());
  EXPECT_TRUE(spans[0].style.has_fg());
}

TEST(RichTextParseTest, CloseAllTags) {
  auto spans = RichText::Parse("[bold][red]text[/]after");
  // "text" is bold+red, "after" is unstyled
  ASSERT_GE(spans.size(), 2u);
  // Last span should be plain
  EXPECT_EQ(spans.back().text, "after");
  EXPECT_FALSE(spans.back().style.is_bold());
}

// ── RichText::Element
// ─────────────────────────────────────────────────────────

TEST(RichTextElementTest, PlainTextNonNull) {
  auto el = RichText::Element("hello world");
  EXPECT_NE(el, nullptr);
}

TEST(RichTextElementTest, MarkupNonNull) {
  auto el = RichText::Element("[bold]hello[/bold] [red]world[/red]");
  EXPECT_NE(el, nullptr);
}

// ── AnsiParser::Strip
// ─────────────────────────────────────────────────────────

TEST(AnsiParserTest, StripRemovesEscapes) {
  const std::string input = "\033[1mhello\033[0m";
  const std::string result = AnsiParser::Strip(input);
  EXPECT_EQ(result, "hello");
}

TEST(AnsiParserTest, StripLeavesPlainText) {
  const std::string input = "plain text";
  EXPECT_EQ(AnsiParser::Strip(input), input);
}

// ── AnsiParser::Parse
// ─────────────────────────────────────────────────────────

TEST(AnsiParserParseTest, EmptyString) {
  auto spans = AnsiParser::Parse("");
  EXPECT_TRUE(spans.empty());
}

TEST(AnsiParserParseTest, PlainText) {
  auto spans = AnsiParser::Parse("hello");
  ASSERT_EQ(spans.size(), 1u);
  EXPECT_EQ(spans[0].text, "hello");
  EXPECT_FALSE(spans[0].style.is_bold());
}

TEST(AnsiParserParseTest, BoldCode) {
  auto spans = AnsiParser::Parse("\033[1mhello\033[0m");
  ASSERT_GE(spans.size(), 1u);
  // First span "hello" should be bold
  EXPECT_EQ(spans[0].text, "hello");
  EXPECT_TRUE(spans[0].style.is_bold());
}

TEST(AnsiParserParseTest, RedFgCode) {
  auto spans = AnsiParser::Parse("\033[31mred\033[0m");
  ASSERT_GE(spans.size(), 1u);
  EXPECT_EQ(spans[0].text, "red");
  EXPECT_TRUE(spans[0].style.has_fg());
}

TEST(AnsiParserParseTest, TrueColorFg) {
  auto spans = AnsiParser::Parse("\033[38;2;255;0;0mred\033[0m");
  ASSERT_GE(spans.size(), 1u);
  EXPECT_EQ(spans[0].text, "red");
  EXPECT_TRUE(spans[0].style.has_fg());
}

TEST(AnsiParserParseTest, ResetClearsStyle) {
  auto spans = AnsiParser::Parse("\033[1m\033[0mplain");
  ASSERT_GE(spans.size(), 1u);
  EXPECT_EQ(spans.back().text, "plain");
  EXPECT_FALSE(spans.back().style.is_bold());
}

// ── ColorSwatch / Palette / Gradient ─────────────────────────────────────────

TEST(ColorSwatchTest, NonNull) {
  auto el = ColorSwatch(Color::Palette16::Red);
  EXPECT_NE(el, nullptr);
}

TEST(ColorPaletteTest, Palette16NonNull) {
  auto el = ColorPalette16();
  EXPECT_NE(el, nullptr);
}

TEST(ColorPaletteTest, Palette256NonNull) {
  auto el = ColorPalette256();
  EXPECT_NE(el, nullptr);
}

TEST(ColorGradientTest, NonNull) {
  auto el = ColorGradient(Color::Palette16::Red, Color::Palette16::Blue, 20);
  EXPECT_NE(el, nullptr);
}

// ── StyledText convenience
// ────────────────────────────────────────────────────

TEST(ConvenienceFunctionsTest, StyledText) {
  TextStyle s;
  s.Bold().Green();
  auto el = StyledText("hi", s);
  EXPECT_NE(el, nullptr);
}
