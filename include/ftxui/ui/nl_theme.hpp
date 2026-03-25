// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_NL_THEME_HPP
#define FTXUI_UI_NL_THEME_HPP

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/ui/theme.hpp"

namespace ftxui::ui {

// ── ThemeDescription ─────────────────────────────────────────────────────────
/// @brief Parsed attributes extracted from a natural-language theme
/// description.
/// @ingroup ui
struct ThemeDescription {
  std::string raw;

  enum class Mood { Dark, Light, Neutral } mood = Mood::Dark;
  enum class Style {
    Cyberpunk,
    Minimal,
    Corporate,
    Hacker,
    Retro,
    Space,
    Nature,
    Warm
  } style = Style::Minimal;

  struct ColorSpec {
    uint8_t r, g, b;
    std::string name;
  };
  std::optional<ColorSpec> primary_color;
  std::optional<ColorSpec> accent_color;
  std::optional<ColorSpec> bg_color;

  bool high_contrast = false;
  bool neon = false;        ///< Bright saturated colors.
  bool muted = false;       ///< Desaturated colors.
  bool monochrome = false;  ///< Single-hue palette.
};

// ── NLThemeParser
// ─────────────────────────────────────────────────────────────
/// @brief Rule-based theme parser — no LLM required.
///
/// Converts free-form English theme descriptions into structured
/// ThemeDescription objects using keyword matching, then maps those
/// descriptions to concrete Theme values.
///
/// @ingroup ui
class NLThemeParser {
 public:
  /// Parse a natural-language description into a ThemeDescription.
  static ThemeDescription Parse(const std::string& description);

  /// Map a ThemeDescription to a concrete Theme.
  static Theme ToTheme(const ThemeDescription& desc);

  /// One-shot: parse and convert a description to a Theme.
  static Theme FromDescription(const std::string& description);
};

// ── LLMThemeGenerator
// ─────────────────────────────────────────────────────────
/// @brief Optional LLM-powered theme generator.
///
/// When an Ollama server is available the generator sends a structured prompt
/// and parses the JSON response.  Falls back transparently to NLThemeParser
/// when no LLM is reachable.
///
/// @ingroup ui
class LLMThemeGenerator {
 public:
  /// Returns true if an LLM backend (Ollama or OpenAI) is reachable.
  static bool IsAvailable();

  /// Generate a theme from a description, upgrading to LLM if available.
  static Theme Generate(const std::string& description,
                        const std::string& model = "llama3");

  /// Generate multiple theme variants for the same description.
  static std::vector<Theme> GenerateVariants(const std::string& description,
                                             int count = 3);

  /// Returns the system prompt used when querying the LLM.
  static std::string SystemPrompt();
};

// ── ThemePicker
// ───────────────────────────────────────────────────────────────
/// @brief Interactive component: type a description, see a live theme preview.
///
/// Layout:
///   - Text input: "Describe your theme…"
///   - Color swatch grid showing generated palette
///   - "Apply" button that calls on_theme_selected with the current theme
///   - Status line: "LLM: Active" or "Rule-based"
///
/// @ingroup ui
ftxui::Component ThemePicker(std::function<void(Theme)> on_theme_selected = {});

// ── ThemeTransition
// ───────────────────────────────────────────────────────────
/// @brief Smoothly interpolates between two themes over a configurable
/// duration using the AnimationLoop.
/// @ingroup ui
class ThemeTransition {
 public:
  ThemeTransition(
      Theme from,
      Theme to,
      std::chrono::milliseconds duration = std::chrono::milliseconds(500));
  ~ThemeTransition();

  /// Returns the interpolated theme at the current point in the animation.
  Theme Current() const;

  /// Returns true once the full duration has elapsed.
  bool Complete() const;

  /// Normalized progress: 0.0 → 1.0.
  float Progress() const;

  /// Begin the animation from the current time.
  void Start();

  /// Reset to the initial state (not started).
  void Reset();

 private:
  Theme from_;
  Theme to_;
  std::chrono::milliseconds duration_;

  using Clock = std::chrono::steady_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  bool started_ = false;
  TimePoint start_time_;
  int frame_callback_id_ = -1;
};

// ── Theme interpolation
// ───────────────────────────────────────────────────────
/// @brief Linearly interpolate between two themes.
/// @param t  Blend factor — 0.0 returns @p a, 1.0 returns @p b.
/// @ingroup ui
Theme LerpTheme(const Theme& a, const Theme& b, float t);

// ── Free functions — main API
// ─────────────────────────────────────────────────
/// @brief Build a Theme from a plain-English description.
///
/// Uses the rule-based NLThemeParser (zero dependencies).  Upgrade to LLM
/// is transparent: if an Ollama/OpenAI backend is available the result will
/// use precise generated RGB values instead of predefined palette entries.
///
/// @code
/// auto t = ftxui::ui::ThemeFromDescription("dark blue cyberpunk");
/// ftxui::ui::SetTheme(t);
/// @endcode
///
/// @ingroup ui
Theme ThemeFromDescription(const std::string& description);

/// @brief Asynchronous variant — invokes @p callback when the theme is ready.
///
/// Falls back to the synchronous rule-based path if no LLM is reachable,
/// calling the callback immediately on the calling thread in that case.
///
/// @ingroup ui
void ThemeFromDescriptionAsync(const std::string& description,
                               std::function<void(Theme)> callback);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_NL_THEME_HPP
