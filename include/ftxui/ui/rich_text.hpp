// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_RICH_TEXT_HPP
#define FTXUI_UI_RICH_TEXT_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

// ── TextStyle ─────────────────────────────────────────────────────────────────
// Fluent builder for all terminal text attributes.

class TextStyle {
 public:
  TextStyle() = default;

  // Foreground / background
  TextStyle& Fg(ftxui::Color c);
  TextStyle& Bg(ftxui::Color c);
  TextStyle& Fg(uint8_t r, uint8_t g, uint8_t b);  // TrueColor fg
  TextStyle& Bg(uint8_t r, uint8_t g, uint8_t b);  // TrueColor bg
  TextStyle& Fg256(uint8_t index);                  // 256-color fg
  TextStyle& Bg256(uint8_t index);                  // 256-color bg

  // Named color shortcuts (set foreground)
  TextStyle& Black();
  TextStyle& Red();
  TextStyle& Green();
  TextStyle& Yellow();
  TextStyle& Blue();
  TextStyle& Magenta();
  TextStyle& Cyan();
  TextStyle& White();
  TextStyle& BrightBlack();
  TextStyle& BrightRed();
  TextStyle& BrightGreen();
  TextStyle& BrightYellow();
  TextStyle& BrightBlue();
  TextStyle& BrightMagenta();
  TextStyle& BrightCyan();
  TextStyle& BrightWhite();

  // Attributes
  TextStyle& Bold();
  TextStyle& Dim();
  TextStyle& Italic();
  TextStyle& Underline();
  TextStyle& Blink();
  TextStyle& RapidBlink();
  TextStyle& Reverse();
  TextStyle& Hidden();
  TextStyle& Strikethrough();

  // Apply style to an existing Element
  ftxui::Element Apply(ftxui::Element el) const;

  // Create a styled text element directly
  ftxui::Element operator()(const std::string& text) const;
  ftxui::Element operator()(ftxui::Element el) const;

  // Getters
  ftxui::Color fg_color() const { return fg_; }
  ftxui::Color bg_color() const { return bg_; }
  bool has_fg() const { return has_fg_; }
  bool has_bg() const { return has_bg_; }
  bool is_bold() const { return bold_; }
  bool is_dim() const { return dim_; }
  bool is_italic() const { return italic_; }
  bool is_underline() const { return underline_; }
  bool is_blink() const { return blink_; }
  bool is_rapid_blink() const { return rapid_blink_; }
  bool is_reverse() const { return reverse_; }
  bool is_hidden() const { return hidden_; }
  bool is_strikethrough() const { return strikethrough_; }

 private:
  ftxui::Color fg_{ftxui::Color::Default};
  ftxui::Color bg_{ftxui::Color::Default};
  bool has_fg_ = false, has_bg_ = false;
  bool bold_ = false, dim_ = false, italic_ = false;
  bool underline_ = false, blink_ = false, rapid_blink_ = false;
  bool reverse_ = false, hidden_ = false, strikethrough_ = false;
};

// ── Span ──────────────────────────────────────────────────────────────────────
// A styled text span: text + style.
struct Span {
  std::string text;
  TextStyle style;
};

// ── RichText ──────────────────────────────────────────────────────────────────
// Parse and render inline markup: [bold]text[/bold], [red]text[/red], etc.
// Supported tags: bold, dim, italic, underline, blink, reverse, strikethrough
//                 black, red, green, yellow, blue, magenta, cyan, white
//                 bright_black, bright_red, bright_green, etc.
//                 bg_red, bg_green, etc.
//                 rgb(r,g,b), bg_rgb(r,g,b)
//                 [/] closes all open tags

class RichText {
 public:
  // Parse markup string → vector of Spans
  static std::vector<Span> Parse(const std::string& markup);

  // Parse markup → single Element (hbox of styled texts)
  static ftxui::Element Element(const std::string& markup);

  // Render a pre-parsed span list
  static ftxui::Element Render(const std::vector<Span>& spans);

  // Wrap markup in a Component
  static ftxui::Component Component(const std::string& markup);
};

// ── AnsiParser ────────────────────────────────────────────────────────────────
// Convert ANSI escape sequences to styled Elements.

class AnsiParser {
 public:
  // Parse ANSI-escaped string → vector of Spans
  static std::vector<Span> Parse(const std::string& ansi_text);

  // Parse ANSI-escaped string → Element
  static ftxui::Element Element(const std::string& ansi_text);

  // Strip all ANSI escapes → plain string
  static std::string Strip(const std::string& ansi_text);
};

// ── ColorSwatch ───────────────────────────────────────────────────────────────
// Display utilities for colors.

// A visual 2-char wide color swatch element
ftxui::Element ColorSwatch(ftxui::Color c);

// Full 256-color palette grid element (16×16)
ftxui::Element ColorPalette256();

// All 16 named colors as a row
ftxui::Element ColorPalette16();

// TrueColor gradient (horizontal, start→end color)
ftxui::Element ColorGradient(ftxui::Color from, ftxui::Color to,
                              int width = 40);

// ── Convenience free functions ────────────────────────────────────────────────
ftxui::Element StyledText(const std::string& text, TextStyle style);
ftxui::Element BoldText(const std::string& text,
                        ftxui::Color fg = ftxui::Color::Default);
ftxui::Element DimText(const std::string& text,
                       ftxui::Color fg = ftxui::Color::Default);
ftxui::Element BlinkText(const std::string& text,
                         ftxui::Color fg = ftxui::Color::Default);
ftxui::Element ColorText(const std::string& text, ftxui::Color fg,
                         ftxui::Color bg = ftxui::Color::Default);
ftxui::Element RgbText(const std::string& text, uint8_t r, uint8_t g,
                       uint8_t b);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_RICH_TEXT_HPP
