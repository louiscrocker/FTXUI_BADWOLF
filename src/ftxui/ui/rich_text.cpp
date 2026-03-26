// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "ftxui/ui/rich_text.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

// ── TextStyle
// ─────────────────────────────────────────────────────────────────

TextStyle& TextStyle::Fg(ftxui::Color c) {
  fg_ = c;
  has_fg_ = true;
  return *this;
}

TextStyle& TextStyle::Bg(ftxui::Color c) {
  bg_ = c;
  has_bg_ = true;
  return *this;
}

TextStyle& TextStyle::Fg(uint8_t r, uint8_t g, uint8_t b) {
  return Fg(ftxui::Color::RGB(r, g, b));
}

TextStyle& TextStyle::Bg(uint8_t r, uint8_t g, uint8_t b) {
  return Bg(ftxui::Color::RGB(r, g, b));
}

TextStyle& TextStyle::Fg256(uint8_t index) {
  return Fg(ftxui::Color(static_cast<ftxui::Color::Palette256>(index)));
}

TextStyle& TextStyle::Bg256(uint8_t index) {
  return Bg(ftxui::Color(static_cast<ftxui::Color::Palette256>(index)));
}

// Named color shortcuts
TextStyle& TextStyle::Black() {
  return Fg(ftxui::Color::Palette16::Black);
}
TextStyle& TextStyle::Red() {
  return Fg(ftxui::Color::Palette16::Red);
}
TextStyle& TextStyle::Green() {
  return Fg(ftxui::Color::Palette16::Green);
}
TextStyle& TextStyle::Yellow() {
  return Fg(ftxui::Color::Palette16::Yellow);
}
TextStyle& TextStyle::Blue() {
  return Fg(ftxui::Color::Palette16::Blue);
}
TextStyle& TextStyle::Magenta() {
  return Fg(ftxui::Color::Palette16::Magenta);
}
TextStyle& TextStyle::Cyan() {
  return Fg(ftxui::Color::Palette16::Cyan);
}
TextStyle& TextStyle::White() {
  return Fg(ftxui::Color::Palette16::White);
}
TextStyle& TextStyle::BrightBlack() {
  return Fg(ftxui::Color::Palette16::GrayDark);
}
TextStyle& TextStyle::BrightRed() {
  return Fg(ftxui::Color::Palette16::RedLight);
}
TextStyle& TextStyle::BrightGreen() {
  return Fg(ftxui::Color::Palette16::GreenLight);
}
TextStyle& TextStyle::BrightYellow() {
  return Fg(ftxui::Color::Palette16::YellowLight);
}
TextStyle& TextStyle::BrightBlue() {
  return Fg(ftxui::Color::Palette16::BlueLight);
}
TextStyle& TextStyle::BrightMagenta() {
  return Fg(ftxui::Color::Palette16::MagentaLight);
}
TextStyle& TextStyle::BrightCyan() {
  return Fg(ftxui::Color::Palette16::CyanLight);
}
TextStyle& TextStyle::BrightWhite() {
  return Fg(ftxui::Color::Palette16::GrayLight);
}

// Attribute setters
TextStyle& TextStyle::Bold() {
  bold_ = true;
  return *this;
}
TextStyle& TextStyle::Dim() {
  dim_ = true;
  return *this;
}
TextStyle& TextStyle::Italic() {
  italic_ = true;
  return *this;
}
TextStyle& TextStyle::Underline() {
  underline_ = true;
  return *this;
}
TextStyle& TextStyle::Blink() {
  blink_ = true;
  return *this;
}
TextStyle& TextStyle::RapidBlink() {
  rapid_blink_ = true;
  return *this;
}
TextStyle& TextStyle::Reverse() {
  reverse_ = true;
  return *this;
}
TextStyle& TextStyle::Hidden() {
  hidden_ = true;
  return *this;
}
TextStyle& TextStyle::Strikethrough() {
  strikethrough_ = true;
  return *this;
}

ftxui::Element TextStyle::Apply(ftxui::Element el) const {
  if (has_fg_) {
    el = ftxui::color(fg_, el);
  }
  if (has_bg_) {
    el = ftxui::bgcolor(bg_, el);
  }
  if (bold_) {
    el = ftxui::bold(el);
  }
  if (dim_) {
    el = ftxui::dim(el);
  }
  if (italic_) {
    el = ftxui::italic(el);
  }
  if (underline_) {
    el = ftxui::underlined(el);
  }
  if (blink_ || rapid_blink_) {
    el = ftxui::blink(el);
  }
  if (reverse_) {
    el = ftxui::inverted(el);
  }
  if (strikethrough_) {
    el = ftxui::strikethrough(el);
  }
  return el;
}

ftxui::Element TextStyle::operator()(const std::string& t) const {
  return Apply(ftxui::text(t));
}

ftxui::Element TextStyle::operator()(ftxui::Element el) const {
  return Apply(std::move(el));
}

// ── RichText
// ──────────────────────────────────────────────────────────────────

namespace {

// Lowercase helper
std::string ToLower(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

// Apply a tag name to a TextStyle
void ApplyTag(const std::string& tag, TextStyle& style) {
  const std::string t = ToLower(tag);

  // Attributes
  if (t == "bold") {
    style.Bold();
    return;
  }
  if (t == "dim") {
    style.Dim();
    return;
  }
  if (t == "italic") {
    style.Italic();
    return;
  }
  if (t == "underline") {
    style.Underline();
    return;
  }
  if (t == "blink") {
    style.Blink();
    return;
  }
  if (t == "reverse") {
    style.Reverse();
    return;
  }
  if (t == "strikethrough") {
    style.Strikethrough();
    return;
  }

  // Named fg colors
  if (t == "black") {
    style.Black();
    return;
  }
  if (t == "red") {
    style.Red();
    return;
  }
  if (t == "green") {
    style.Green();
    return;
  }
  if (t == "yellow") {
    style.Yellow();
    return;
  }
  if (t == "blue") {
    style.Blue();
    return;
  }
  if (t == "magenta") {
    style.Magenta();
    return;
  }
  if (t == "cyan") {
    style.Cyan();
    return;
  }
  if (t == "white") {
    style.White();
    return;
  }
  if (t == "bright_black") {
    style.BrightBlack();
    return;
  }
  if (t == "bright_red") {
    style.BrightRed();
    return;
  }
  if (t == "bright_green") {
    style.BrightGreen();
    return;
  }
  if (t == "bright_yellow") {
    style.BrightYellow();
    return;
  }
  if (t == "bright_blue") {
    style.BrightBlue();
    return;
  }
  if (t == "bright_magenta") {
    style.BrightMagenta();
    return;
  }
  if (t == "bright_cyan") {
    style.BrightCyan();
    return;
  }
  if (t == "bright_white") {
    style.BrightWhite();
    return;
  }

  // Named bg colors
  if (t == "bg_black") {
    style.Bg(ftxui::Color::Palette16::Black);
    return;
  }
  if (t == "bg_red") {
    style.Bg(ftxui::Color::Palette16::Red);
    return;
  }
  if (t == "bg_green") {
    style.Bg(ftxui::Color::Palette16::Green);
    return;
  }
  if (t == "bg_yellow") {
    style.Bg(ftxui::Color::Palette16::Yellow);
    return;
  }
  if (t == "bg_blue") {
    style.Bg(ftxui::Color::Palette16::Blue);
    return;
  }
  if (t == "bg_magenta") {
    style.Bg(ftxui::Color::Palette16::Magenta);
    return;
  }
  if (t == "bg_cyan") {
    style.Bg(ftxui::Color::Palette16::Cyan);
    return;
  }
  if (t == "bg_white") {
    style.Bg(ftxui::Color::Palette16::White);
    return;
  }

  // TrueColor: rgb(r,g,b)
  if (t.rfind("rgb(", 0) == 0) {
    int r = 0, g = 0, b = 0;
    if (std::sscanf(t.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) == 3) {
      style.Fg(static_cast<uint8_t>(r), static_cast<uint8_t>(g),
               static_cast<uint8_t>(b));
    }
    return;
  }
  // bg_rgb(r,g,b)
  if (t.rfind("bg_rgb(", 0) == 0) {
    int r = 0, g = 0, b = 0;
    if (std::sscanf(t.c_str(), "bg_rgb(%d,%d,%d)", &r, &g, &b) == 3) {
      style.Bg(static_cast<uint8_t>(r), static_cast<uint8_t>(g),
               static_cast<uint8_t>(b));
    }
    return;
  }
}

}  // namespace

// static
std::vector<Span> RichText::Parse(const std::string& markup) {
  std::vector<Span> result;
  // Stack of accumulated styles
  std::vector<TextStyle> stack;
  stack.push_back(TextStyle{});  // base style

  std::string text;
  size_t i = 0;
  const size_t n = markup.size();

  auto flush = [&]() {
    if (!text.empty()) {
      result.push_back({text, stack.back()});
      text.clear();
    }
  };

  while (i < n) {
    if (markup[i] == '[') {
      // Find closing ']'
      size_t j = markup.find(']', i + 1);
      if (j == std::string::npos) {
        text += markup[i];
        ++i;
        continue;
      }
      const std::string tag = markup.substr(i + 1, j - i - 1);
      flush();

      if (tag == "/" || tag == "reset") {
        // Close all open tags — reset to base style
        stack.resize(1);
      } else if (!tag.empty() && tag[0] == '/') {
        // Close matching tag — pop top style if stack depth > 1
        if (stack.size() > 1) {
          stack.pop_back();
        }
      } else {
        // Open tag — push a new style based on current
        TextStyle new_style = stack.back();
        ApplyTag(tag, new_style);
        stack.push_back(new_style);
      }
      i = j + 1;
    } else {
      text += markup[i];
      ++i;
    }
  }
  flush();
  return result;
}

// static
ftxui::Element RichText::Render(const std::vector<Span>& spans) {
  ftxui::Elements elements;
  elements.reserve(spans.size());
  for (const auto& span : spans) {
    elements.push_back(span.style(span.text));
  }
  if (elements.empty()) {
    return ftxui::text("");
  }
  return ftxui::hbox(std::move(elements));
}

// static
ftxui::Element RichText::Element(const std::string& markup) {
  return Render(Parse(markup));
}

// static
ftxui::Component RichText::Component(const std::string& markup) {
  return ftxui::Renderer([markup] { return RichText::Element(markup); });
}

// ── AnsiParser
// ────────────────────────────────────────────────────────────────

namespace {

// Map ANSI color index 30-37 / 90-97 to Palette16
ftxui::Color AnsiIndexToColor(int base, int offset) {
  // base 30 → standard fg, base 90 → bright fg
  // base 40 → standard bg, base 100 → bright bg
  static const ftxui::Color::Palette16 standard[8] = {
      ftxui::Color::Palette16::Black, ftxui::Color::Palette16::Red,
      ftxui::Color::Palette16::Green, ftxui::Color::Palette16::Yellow,
      ftxui::Color::Palette16::Blue,  ftxui::Color::Palette16::Magenta,
      ftxui::Color::Palette16::Cyan,  ftxui::Color::Palette16::GrayLight,
  };
  static const ftxui::Color::Palette16 bright[8] = {
      ftxui::Color::Palette16::GrayDark,
      ftxui::Color::Palette16::RedLight,
      ftxui::Color::Palette16::GreenLight,
      ftxui::Color::Palette16::YellowLight,
      ftxui::Color::Palette16::BlueLight,
      ftxui::Color::Palette16::MagentaLight,
      ftxui::Color::Palette16::CyanLight,
      ftxui::Color::Palette16::White,
  };
  (void)base;
  if (offset >= 0 && offset < 8) {
    return ftxui::Color(standard[offset]);
  }
  if (offset >= 60 && offset < 68) {
    return ftxui::Color(bright[offset - 60]);
  }
  return ftxui::Color::Default;
}

// Process a list of SGR parameter codes and update style
void ApplySGRCodes(const std::vector<int>& codes, TextStyle& style) {
  size_t i = 0;
  while (i < codes.size()) {
    const int code = codes[i];
    switch (code) {
      case 0:  // Reset
        style = TextStyle{};
        break;
      case 1:
        style.Bold();
        break;
      case 2:
        style.Dim();
        break;
      case 3:
        style.Italic();
        break;
      case 4:
        style.Underline();
        break;
      case 5:
        style.Blink();
        break;
      case 6:
        style.RapidBlink();
        break;
      case 7:
        style.Reverse();
        break;
      case 8:
        style.Hidden();
        break;
      case 9:
        style.Strikethrough();
        break;
      case 21:
        style.Underline();
        break;  // doubly underlined → underline
      case 22:  /* normal intensity — clear bold/dim, not modeled */
        break;
      case 23: /* not italic */
        break;
      case 24: /* not underline */
        break;
      case 25: /* not blink */
        break;
      // Standard fg colors 30-37
      case 30:
      case 31:
      case 32:
      case 33:
      case 34:
      case 35:
      case 36:
      case 37:
        style.Fg(AnsiIndexToColor(30, code - 30));
        break;
      // Extended fg color
      case 38:
        if (i + 1 < codes.size() && codes[i + 1] == 2 && i + 4 < codes.size()) {
          style.Fg(static_cast<uint8_t>(codes[i + 2]),
                   static_cast<uint8_t>(codes[i + 3]),
                   static_cast<uint8_t>(codes[i + 4]));
          i += 4;
        } else if (i + 1 < codes.size() && codes[i + 1] == 5 &&
                   i + 2 < codes.size()) {
          style.Fg256(static_cast<uint8_t>(codes[i + 2]));
          i += 2;
        }
        break;
      case 39:  // default fg
        style.Fg(ftxui::Color::Default);
        break;
      // Standard bg colors 40-47
      case 40:
      case 41:
      case 42:
      case 43:
      case 44:
      case 45:
      case 46:
      case 47:
        style.Bg(AnsiIndexToColor(40, code - 40));
        break;
      // Extended bg color
      case 48:
        if (i + 1 < codes.size() && codes[i + 1] == 2 && i + 4 < codes.size()) {
          style.Bg(static_cast<uint8_t>(codes[i + 2]),
                   static_cast<uint8_t>(codes[i + 3]),
                   static_cast<uint8_t>(codes[i + 4]));
          i += 4;
        } else if (i + 1 < codes.size() && codes[i + 1] == 5 &&
                   i + 2 < codes.size()) {
          style.Bg256(static_cast<uint8_t>(codes[i + 2]));
          i += 2;
        }
        break;
      case 49:  // default bg
        style.Bg(ftxui::Color::Default);
        break;
      // Bright fg 90-97
      case 90:
      case 91:
      case 92:
      case 93:
      case 94:
      case 95:
      case 96:
      case 97:
        style.Fg(AnsiIndexToColor(90, (code - 90) + 60));
        break;
      // Bright bg 100-107
      case 100:
      case 101:
      case 102:
      case 103:
      case 104:
      case 105:
      case 106:
      case 107:
        style.Bg(AnsiIndexToColor(100, (code - 100) + 60));
        break;
      default:
        break;
    }
    ++i;
  }
}

}  // namespace

// static
std::vector<Span> AnsiParser::Parse(const std::string& ansi_text) {
  std::vector<Span> result;
  TextStyle current_style;
  std::string text;
  size_t i = 0;
  const size_t n = ansi_text.size();

  auto flush = [&]() {
    if (!text.empty()) {
      result.push_back({text, current_style});
      text.clear();
    }
  };

  while (i < n) {
    if (ansi_text[i] == '\033' && i + 1 < n && ansi_text[i + 1] == '[') {
      flush();
      // Read CSI sequence: \033[ ... <letter>
      i += 2;
      std::string param;
      while (i < n && ansi_text[i] != 'm' &&
             (std::isdigit(static_cast<unsigned char>(ansi_text[i])) ||
              ansi_text[i] == ';')) {
        param += ansi_text[i];
        ++i;
      }
      if (i < n && ansi_text[i] == 'm') {
        ++i;
        // Parse semicolon-separated codes
        std::vector<int> codes;
        if (param.empty()) {
          codes.push_back(0);  // ESC[m == reset
        } else {
          std::istringstream ss(param);
          std::string token;
          while (std::getline(ss, token, ';')) {
            if (token.empty()) {
              codes.push_back(0);
            } else {
              codes.push_back(std::stoi(token));
            }
          }
        }
        ApplySGRCodes(codes, current_style);
      }
      // Skip non-m CSI sequences (e.g., cursor movement)
      // Already advanced past the terminator above
    } else {
      text += ansi_text[i];
      ++i;
    }
  }
  flush();
  return result;
}

// static
ftxui::Element AnsiParser::Element(const std::string& ansi_text) {
  return RichText::Render(Parse(ansi_text));
}

// static
std::string AnsiParser::Strip(const std::string& ansi_text) {
  std::string result;
  size_t i = 0;
  const size_t n = ansi_text.size();
  while (i < n) {
    if (ansi_text[i] == '\033' && i + 1 < n && ansi_text[i + 1] == '[') {
      i += 2;
      // Skip until final byte (0x40–0x7E)
      while (i < n && !(ansi_text[i] >= 0x40 && ansi_text[i] <= 0x7E)) {
        ++i;
      }
      if (i < n) {
        ++i;  // skip the final byte
      }
    } else {
      result += ansi_text[i];
      ++i;
    }
  }
  return result;
}

// ── ColorSwatch
// ───────────────────────────────────────────────────────────────

ftxui::Element ColorSwatch(ftxui::Color c) {
  return ftxui::bgcolor(c, ftxui::text("  "));
}

ftxui::Element ColorPalette256() {
  ftxui::Elements rows;
  for (int row = 0; row < 16; ++row) {
    ftxui::Elements cells;
    for (int col = 0; col < 16; ++col) {
      const int index = row * 16 + col;
      cells.push_back(ColorSwatch(
          ftxui::Color(static_cast<ftxui::Color::Palette256>(index))));
    }
    rows.push_back(ftxui::hbox(std::move(cells)));
  }
  return ftxui::vbox(std::move(rows));
}

ftxui::Element ColorPalette16() {
  static const ftxui::Color::Palette16 colors[16] = {
      ftxui::Color::Palette16::Black,
      ftxui::Color::Palette16::Red,
      ftxui::Color::Palette16::Green,
      ftxui::Color::Palette16::Yellow,
      ftxui::Color::Palette16::Blue,
      ftxui::Color::Palette16::Magenta,
      ftxui::Color::Palette16::Cyan,
      ftxui::Color::Palette16::GrayLight,
      ftxui::Color::Palette16::GrayDark,
      ftxui::Color::Palette16::RedLight,
      ftxui::Color::Palette16::GreenLight,
      ftxui::Color::Palette16::YellowLight,
      ftxui::Color::Palette16::BlueLight,
      ftxui::Color::Palette16::MagentaLight,
      ftxui::Color::Palette16::CyanLight,
      ftxui::Color::Palette16::White,
  };
  ftxui::Elements cells;
  for (const auto& c : colors) {
    cells.push_back(ColorSwatch(ftxui::Color(c)));
  }
  return ftxui::hbox(std::move(cells));
}

ftxui::Element ColorGradient(ftxui::Color from, ftxui::Color to, int width) {
  if (width <= 0) {
    return ftxui::text("");
  }
  ftxui::Elements cells;
  cells.reserve(static_cast<size_t>(width));
  for (int i = 0; i < width; ++i) {
    const float t = (width == 1)
                        ? 0.0F
                        : static_cast<float>(i) / static_cast<float>(width - 1);
    const ftxui::Color c = ftxui::Color::Interpolate(t, from, to);
    cells.push_back(ftxui::bgcolor(c, ftxui::text("█")));
  }
  return ftxui::hbox(std::move(cells));
}

// ── Convenience free functions
// ────────────────────────────────────────────────

ftxui::Element StyledText(const std::string& t, TextStyle style) {
  return style(t);
}

ftxui::Element BoldText(const std::string& t, ftxui::Color fg) {
  TextStyle s;
  s.Bold();
  if (fg != ftxui::Color::Default) {
    s.Fg(fg);
  }
  return s(t);
}

ftxui::Element DimText(const std::string& t, ftxui::Color fg) {
  TextStyle s;
  s.Dim();
  if (fg != ftxui::Color::Default) {
    s.Fg(fg);
  }
  return s(t);
}

ftxui::Element BlinkText(const std::string& t, ftxui::Color fg) {
  TextStyle s;
  s.Blink();
  if (fg != ftxui::Color::Default) {
    s.Fg(fg);
  }
  return s(t);
}

ftxui::Element ColorText(const std::string& t,
                         ftxui::Color fg,
                         ftxui::Color bg) {
  TextStyle s;
  s.Fg(fg);
  if (bg != ftxui::Color::Default) {
    s.Bg(bg);
  }
  return s(t);
}

ftxui::Element RgbText(const std::string& t, uint8_t r, uint8_t g, uint8_t b) {
  return TextStyle{}.Fg(r, g, b)(t);
}

}  // namespace ftxui::ui
