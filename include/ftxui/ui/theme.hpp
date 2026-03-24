// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_THEME_HPP
#define FTXUI_UI_THEME_HPP

#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

/// @brief A centralized style configuration for all ui:: components.
///
/// Set a theme globally with `ftxui::ui::SetTheme(theme)` before building
/// components. All ui:: factory functions read from the active theme.
///
/// @ingroup ui
struct Theme {
  // ── Color palette ─────────────────────────────────────────────────────────
  Color primary = Color::Cyan;           ///< Accent/focus color.
  Color secondary = Color::Blue;         ///< Secondary highlight.
  Color accent = Color::GreenLight;      ///< Success/positive.
  Color error_color = Color::Red;        ///< Error/danger.
  Color warning_color = Color::Yellow;   ///< Warning/caution.
  Color success_color = Color::Green;    ///< Success indicator.
  Color text = Color::Default;           ///< Normal text.
  Color text_muted = Color::GrayLight;   ///< Dim/secondary text.
  Color border_color = Color::GrayDark;  ///< Border lines.

  // ── Button colors ─────────────────────────────────────────────────────────
  Color button_bg_normal = Color::Default;  ///< Button background (normal).
  Color button_fg_normal = Color::White;    ///< Button foreground (normal).
  Color button_bg_active = Color::Cyan;     ///< Button background (focused).
  Color button_fg_active = Color::Black;    ///< Button foreground (focused).

  // ── Style ─────────────────────────────────────────────────────────────────
  BorderStyle border_style = ROUNDED;  ///< Border character style.
  bool animations_enabled = true;      ///< Enable animated transitions.

  // ── Preset themes ─────────────────────────────────────────────────────────
  static Theme Default();
  static Theme Dark();
  static Theme Light();
  static Theme Nord();
  static Theme Dracula();
  static Theme Monokai();

  // ── Fluent setters ────────────────────────────────────────────────────────
  Theme& WithPrimary(Color c) {
    primary = c;
    return *this;
  }
  Theme& WithSecondary(Color c) {
    secondary = c;
    return *this;
  }
  Theme& WithAccent(Color c) {
    accent = c;
    return *this;
  }
  Theme& WithError(Color c) {
    error_color = c;
    return *this;
  }
  Theme& WithBorderStyle(BorderStyle s) {
    border_style = s;
    return *this;
  }
  Theme& WithBorderColor(Color c) {
    border_color = c;
    return *this;
  }
  Theme& WithAnimations(bool v) {
    animations_enabled = v;
    return *this;
  }

  // ── Style factories (used internally by ui:: components) ──────────────────
  ButtonOption MakeButtonStyle() const;
  InputOption MakeInputStyle() const;
  Decorator MakeBorderDecorator() const;
  Decorator PrimaryTextDecorator() const;
  Decorator MutedTextDecorator() const;
};

/// @brief Replace the active theme used by all ui:: components.
void SetTheme(const Theme& theme);

/// @brief Return the currently active theme.
const Theme& GetTheme();

/// @brief Save the active theme to a file (key=value format).
/// Returns true on success.
bool SaveTheme(std::string_view path);

/// @brief Load and apply a theme from a saved file.
/// Returns true on success; leaves the theme unchanged on failure.
bool LoadTheme(std::string_view path);

/// @brief RAII guard that restores the previous theme on destruction.
class ScopedTheme {
 public:
  explicit ScopedTheme(const Theme& theme);
  ~ScopedTheme();

 private:
  Theme previous_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_THEME_HPP
