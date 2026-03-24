// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/theme.hpp"

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace ftxui::ui {

namespace {
Theme g_theme;
}  // namespace

void SetTheme(const Theme& theme) {
  g_theme = theme;
}

const Theme& GetTheme() {
  return g_theme;
}

ScopedTheme::ScopedTheme(const Theme& theme) : previous_(GetTheme()) {
  SetTheme(theme);
}

ScopedTheme::~ScopedTheme() {
  SetTheme(previous_);
}

// ── Preset factories ──────────────────────────────────────────────────────────

Theme Theme::Default() {
  return {};
}

Theme Theme::Dark() {
  Theme t;
  t.primary = Color::Cyan;
  t.secondary = Color::Blue;
  t.accent = Color::GreenLight;
  t.error_color = Color::Red;
  t.warning_color = Color::Yellow;
  t.success_color = Color::Green;
  t.text = Color::White;
  t.text_muted = Color::GrayLight;
  t.border_color = Color::GrayDark;
  t.button_bg_normal = Color::Default;
  t.button_fg_normal = Color::White;
  t.button_bg_active = Color::CyanLight;
  t.button_fg_active = Color::Black;
  t.border_style = ROUNDED;
  t.animations_enabled = true;
  return t;
}

Theme Theme::Light() {
  Theme t;
  t.primary = Color::Blue;
  t.secondary = Color::CyanLight;
  t.accent = Color::Green;
  t.error_color = Color::Red;
  t.warning_color = Color::Yellow;
  t.success_color = Color::GreenLight;
  t.text = Color::Default;
  t.text_muted = Color::GrayDark;
  t.border_color = Color::GrayDark;
  t.button_bg_normal = Color::Default;
  t.button_fg_normal = Color::Default;
  t.button_bg_active = Color::Blue;
  t.button_fg_active = Color::White;
  t.border_style = LIGHT;
  t.animations_enabled = true;
  return t;
}

Theme Theme::Nord() {
  Theme t;
  t.primary = Color::RGB(136, 192, 208);       // Nord8  – frost blue
  t.secondary = Color::RGB(129, 161, 193);     // Nord9
  t.accent = Color::RGB(163, 190, 140);        // Nord14 – aurora green
  t.error_color = Color::RGB(191, 97, 106);    // Nord11 – red
  t.warning_color = Color::RGB(235, 203, 139); // Nord13 – yellow
  t.success_color = Color::RGB(163, 190, 140); // Nord14
  t.text = Color::RGB(216, 222, 233);          // Nord4
  t.text_muted = Color::RGB(76, 86, 106);      // Nord2
  t.border_color = Color::RGB(59, 66, 82);     // Nord1
  t.button_bg_normal = Color::Default;
  t.button_fg_normal = Color::RGB(216, 222, 233);
  t.button_bg_active = Color::RGB(136, 192, 208);
  t.button_fg_active = Color::RGB(46, 52, 64); // Nord0
  t.border_style = ROUNDED;
  t.animations_enabled = true;
  return t;
}

Theme Theme::Dracula() {
  Theme t;
  t.primary = Color::RGB(189, 147, 249);       // purple
  t.secondary = Color::RGB(139, 233, 253);     // cyan
  t.accent = Color::RGB(80, 250, 123);         // green
  t.error_color = Color::RGB(255, 85, 85);     // red
  t.warning_color = Color::RGB(241, 250, 140); // yellow
  t.success_color = Color::RGB(80, 250, 123);  // green
  t.text = Color::RGB(248, 248, 242);          // foreground
  t.text_muted = Color::RGB(98, 114, 164);     // comment
  t.border_color = Color::RGB(68, 71, 90);     // current-line
  t.button_bg_normal = Color::Default;
  t.button_fg_normal = Color::RGB(248, 248, 242);
  t.button_bg_active = Color::RGB(189, 147, 249);
  t.button_fg_active = Color::RGB(40, 42, 54); // background
  t.border_style = ROUNDED;
  t.animations_enabled = true;
  return t;
}

Theme Theme::Monokai() {
  Theme t;
  t.primary = Color::RGB(102, 217, 239);       // cyan
  t.secondary = Color::RGB(166, 226, 46);      // green
  t.accent = Color::RGB(249, 38, 114);         // pink
  t.error_color = Color::RGB(249, 38, 114);    // pink
  t.warning_color = Color::RGB(230, 219, 116); // yellow
  t.success_color = Color::RGB(166, 226, 46);  // green
  t.text = Color::RGB(248, 248, 242);          // foreground
  t.text_muted = Color::RGB(117, 113, 94);     // comment
  t.border_color = Color::RGB(117, 113, 94);
  t.button_bg_normal = Color::Default;
  t.button_fg_normal = Color::RGB(248, 248, 242);
  t.button_bg_active = Color::RGB(102, 217, 239);
  t.button_fg_active = Color::RGB(39, 40, 34);
  t.border_style = ROUNDED;
  t.animations_enabled = true;
  return t;
}

// ── Style factories ───────────────────────────────────────────────────────────

ButtonOption Theme::MakeButtonStyle() const {
  if (animations_enabled) {
    return ButtonOption::Animated(button_bg_normal, button_fg_normal,
                                  button_bg_active, button_fg_active);
  }
  auto opt = ButtonOption::Simple();
  return opt;
}

InputOption Theme::MakeInputStyle() const {
  InputOption opt;
  Color fg = primary;
  opt.transform = [fg](InputState state) {
    if (state.focused) {
      state.element = state.element | color(fg);
    }
    if (state.is_placeholder) {
      state.element = state.element | dim;
    }
    return state.element;
  };
  return opt;
}

Decorator Theme::MakeBorderDecorator() const {
  return borderStyled(border_style, border_color);
}

Decorator Theme::PrimaryTextDecorator() const {
  return color(primary);
}

Decorator Theme::MutedTextDecorator() const {
  return color(text_muted);
}

// ── Theme persistence ─────────────────────────────────────────────────────────

namespace {

// Serialize Color as its raw 32-bit memory value. Color is a 4-byte struct
// (ColorType uint8 + r,g,b uint8) — stable across platforms for this use case.
std::string ColorToString(Color c) {
  uint32_t raw = 0;
  std::memcpy(&raw, &c, sizeof(Color));
  return std::to_string(raw);
}

Color ColorFromString(const std::string& s) {
  if (s.empty()) return Color::Default;
  try {
    uint32_t raw = static_cast<uint32_t>(std::stoul(s));
    Color c;
    std::memcpy(&c, &raw, sizeof(Color));
    return c;
  } catch (...) {
    return Color::Default;
  }
}

}  // namespace

bool SaveTheme(std::string_view path) {
  std::string path_str{path};
  std::ofstream f{path_str};
  if (!f) return false;
  const Theme& t = GetTheme();
#define W(key, val) f << (key) << "=" << ColorToString(val) << "\n"
  W("primary",          t.primary);
  W("secondary",        t.secondary);
  W("accent",           t.accent);
  W("error_color",      t.error_color);
  W("warning_color",    t.warning_color);
  W("success_color",    t.success_color);
  W("text",             t.text);
  W("text_muted",       t.text_muted);
  W("border_color",     t.border_color);
  W("button_bg_normal", t.button_bg_normal);
  W("button_fg_normal", t.button_fg_normal);
  W("button_bg_active", t.button_bg_active);
  W("button_fg_active", t.button_fg_active);
#undef W
  f << "border_style=" << static_cast<int>(t.border_style) << "\n";
  f << "animations=" << (t.animations_enabled ? "1" : "0") << "\n";
  return f.good();
}

bool LoadTheme(std::string_view path) {
  std::string path_str{path};
  std::ifstream f{path_str};
  if (!f) return false;

  std::unordered_map<std::string, std::string> kv;
  std::string line;
  while (std::getline(f, line)) {
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    kv[line.substr(0, eq)] = line.substr(eq + 1);
  }

  Theme t = GetTheme();
#define R(key, field) if (kv.count(key)) t.field = ColorFromString(kv.at(key))
  R("primary",          primary);
  R("secondary",        secondary);
  R("accent",           accent);
  R("error_color",      error_color);
  R("warning_color",    warning_color);
  R("success_color",    success_color);
  R("text",             text);
  R("text_muted",       text_muted);
  R("border_color",     border_color);
  R("button_bg_normal", button_bg_normal);
  R("button_fg_normal", button_fg_normal);
  R("button_bg_active", button_bg_active);
  R("button_fg_active", button_fg_active);
#undef R
  if (kv.count("border_style"))
    t.border_style = static_cast<BorderStyle>(std::stoi(kv.at("border_style")));
  if (kv.count("animations"))
    t.animations_enabled = (kv.at("animations") == "1");

  SetTheme(t);
  return true;
}

}  // namespace ftxui::ui

