// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/nl_theme.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/animation.hpp"
#include "ftxui/ui/llm_bridge.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

namespace {

// ── String helpers
// ────────────────────────────────────────────────────────────

std::string ToLower(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

// ── Color helpers
// ─────────────────────────────────────────────────────────────

/// Clamp a float to [0, 255] and return as uint8_t.
uint8_t ClampU8(float v) {
  return static_cast<uint8_t>(std::min(255.f, std::max(0.f, std::round(v))));
}

/// Linearly interpolate a single uint8_t channel.
uint8_t LerpU8(uint8_t a, uint8_t b, float t) {
  return ClampU8(static_cast<float>(a) * (1.f - t) + static_cast<float>(b) * t);
}

Color LerpColor(Color a, Color b, float t) {
  // Color layout: [type(1), red(1), green(1), blue(1), alpha(1)] = 5 bytes
  // ColorType::TrueColor == 3
  uint8_t ba[sizeof(Color)], bb[sizeof(Color)];
  std::memcpy(ba, &a, sizeof(Color));
  std::memcpy(bb, &b, sizeof(Color));
  const uint8_t kTrueColor = 3;  // ColorType::TrueColor
  if (ba[0] == kTrueColor && bb[0] == kTrueColor) {
    uint8_t r = LerpU8(ba[1], bb[1], t);
    uint8_t g = LerpU8(ba[2], bb[2], t);
    uint8_t bv = LerpU8(ba[3], bb[3], t);
    return Color::RGB(r, g, bv);
  }
  // Snap at midpoint for palette colors.
  return (t < 0.5f) ? a : b;
}

// ── Named color table
// ─────────────────────────────────────────────────────────

struct NamedColor {
  const char* name;
  uint8_t r, g, b;
};

constexpr NamedColor kNamedColors[] = {
    {"red", 220, 50, 50},      {"blue", 50, 100, 220},
    {"green", 50, 200, 100},   {"purple", 139, 92, 246},
    {"orange", 255, 140, 0},   {"cyan", 0, 210, 210},
    {"yellow", 255, 215, 0},   {"pink", 255, 100, 150},
    {"white", 240, 240, 240},  {"gold", 212, 175, 55},
    {"teal", 0, 180, 160},     {"violet", 148, 0, 211},
    {"magenta", 255, 0, 255},  {"crimson", 220, 20, 60},
    {"indigo", 75, 0, 130},    {"navy", 15, 30, 90},
    {"amber", 255, 175, 0},    {"lime", 140, 220, 0},
    {"silver", 192, 192, 192}, {"black", 15, 15, 20},
};

std::optional<ThemeDescription::ColorSpec> ExtractColor(
    const std::string& lower) {
  for (const auto& nc : kNamedColors) {
    if (Contains(lower, nc.name)) {
      return ThemeDescription::ColorSpec{nc.r, nc.g, nc.b, nc.name};
    }
  }
  return std::nullopt;
}

// ── Saturation helpers
// ────────────────────────────────────────────────────────

/// Boost or reduce saturation of an RGB triplet.
/// factor > 1.0 boosts, factor < 1.0 reduces.
Color AdjustSaturation(Color c, float factor) {
  // Color layout: [type(1), red(1), green(1), blue(1), alpha(1)] = 5 bytes
  uint8_t ba[sizeof(Color)];
  std::memcpy(ba, &c, sizeof(Color));
  const uint8_t kTrueColor = 3;
  if (ba[0] != kTrueColor) {
    return c;
  }
  float r = ba[1] / 255.f;
  float g = ba[2] / 255.f;
  float b = ba[3] / 255.f;
  float gray = 0.2126f * r + 0.7152f * g + 0.0722f * b;
  r = gray + (r - gray) * factor;
  g = gray + (g - gray) * factor;
  b = gray + (b - gray) * factor;
  return Color::RGB(ClampU8(r * 255.f), ClampU8(g * 255.f), ClampU8(b * 255.f));
}

Color Neon(Color c) {
  return AdjustSaturation(c, 1.4f);
}
Color Muted(Color c) {
  return AdjustSaturation(c, 0.7f);
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// NLThemeParser
// ─────────────────────────────────────────────────────────────────────────────

ThemeDescription NLThemeParser::Parse(const std::string& description) {
  ThemeDescription d;
  d.raw = description;
  const std::string lower = ToLower(description);

  // ── Mood detection ──────────────────────────────────────────────────
  if (Contains(lower, "dark") || Contains(lower, "night") ||
      Contains(lower, "midnight") || Contains(lower, "black")) {
    d.mood = ThemeDescription::Mood::Dark;
  } else if (Contains(lower, "light") || Contains(lower, "bright") ||
             Contains(lower, "white") || Contains(lower, "day")) {
    d.mood = ThemeDescription::Mood::Light;
  } else {
    d.mood = ThemeDescription::Mood::Dark;  // default
  }

  // ── Style detection ──────────────────────────────────────────────────
  if (Contains(lower, "cyberpunk") || Contains(lower, "cyber") ||
      Contains(lower, "synthwave") || Contains(lower, "neon punk")) {
    d.style = ThemeDescription::Style::Cyberpunk;
  } else if (Contains(lower, "hacker") || Contains(lower, "matrix") ||
             Contains(lower, "terminal")) {
    d.style = ThemeDescription::Style::Hacker;
  } else if (Contains(lower, "retro") || Contains(lower, "vintage") ||
             Contains(lower, "80s") || Contains(lower, "vaporwave")) {
    d.style = ThemeDescription::Style::Retro;
  } else if (Contains(lower, "space") || Contains(lower, "galaxy") ||
             Contains(lower, "cosmic") || Contains(lower, "star")) {
    d.style = ThemeDescription::Style::Space;
  } else if (Contains(lower, "nature") || Contains(lower, "forest") ||
             Contains(lower, "earth")) {
    d.style = ThemeDescription::Style::Nature;
  } else if (Contains(lower, "warm") || Contains(lower, "sunset") ||
             Contains(lower, "fire")) {
    d.style = ThemeDescription::Style::Warm;
  } else if (Contains(lower, "corporate") || Contains(lower, "professional") ||
             Contains(lower, "business")) {
    d.style = ThemeDescription::Style::Corporate;
  } else if (Contains(lower, "minimal") || Contains(lower, "clean") ||
             Contains(lower, "simple") || Contains(lower, "flat")) {
    d.style = ThemeDescription::Style::Minimal;
  } else {
    d.style = ThemeDescription::Style::Minimal;  // default
  }

  // ── Modifier detection ───────────────────────────────────────────────
  if (Contains(lower, "neon") || Contains(lower, "vibrant") ||
      Contains(lower, "vivid")) {
    d.neon = true;
  }
  if (Contains(lower, "bright") && !Contains(lower, "dark")) {
    d.neon = true;
  }
  if (Contains(lower, "muted") || Contains(lower, "pastel") ||
      Contains(lower, "soft") || Contains(lower, "subtle")) {
    d.muted = true;
  }
  if (Contains(lower, "high contrast") || Contains(lower, "high-contrast")) {
    d.high_contrast = true;
  }
  if (Contains(lower, "monochrome") || Contains(lower, "mono")) {
    d.monochrome = true;
  }

  // ── Color extraction ─────────────────────────────────────────────────
  // We try to find a primary and accent color by scanning for two color words.
  // First match → primary, second match → accent (if different).
  bool found_primary = false;
  for (const auto& nc : kNamedColors) {
    if (Contains(lower, nc.name)) {
      if (!found_primary) {
        d.primary_color =
            ThemeDescription::ColorSpec{nc.r, nc.g, nc.b, nc.name};
        found_primary = true;
      } else {
        // Second distinct color becomes accent.
        if (nc.name != d.primary_color->name) {
          d.accent_color =
              ThemeDescription::ColorSpec{nc.r, nc.g, nc.b, nc.name};
          break;
        }
      }
    }
  }

  // "green" also triggers Hacker style when no other style was set.
  if (Contains(lower, "green") && d.style == ThemeDescription::Style::Minimal) {
    d.style = ThemeDescription::Style::Hacker;
  }
  // "orange" triggers Warm when no other style.
  if (Contains(lower, "orange") &&
      d.style == ThemeDescription::Style::Minimal) {
    d.style = ThemeDescription::Style::Warm;
  }

  return d;
}

// ─────────────────────────────────────────────────────────────────────────────

Theme NLThemeParser::ToTheme(const ThemeDescription& desc) {
  Theme t;
  t.animations_enabled = true;
  t.border_style = ROUNDED;

  // ── Base palette from style ───────────────────────────────────────────
  switch (desc.style) {
    case ThemeDescription::Style::Cyberpunk:
      t.primary = Color::RGB(139, 92, 246);   // purple
      t.secondary = Color::RGB(0, 210, 210);  // cyan
      t.accent = Color::RGB(0, 255, 210);     // bright cyan
      t.border_color = Color::RGB(80, 0, 160);
      t.button_bg_active = Color::RGB(139, 92, 246);
      t.button_fg_active = Color::Black;
      t.button_bg_normal = Color::Default;
      t.button_fg_normal = Color::RGB(180, 130, 255);
      t.text = Color::RGB(220, 200, 255);
      t.text_muted = Color::RGB(120, 100, 160);
      t.error_color = Color::RGB(255, 50, 100);
      t.warning_color = Color::RGB(255, 200, 0);
      t.success_color = Color::RGB(0, 255, 160);
      break;

    case ThemeDescription::Style::Hacker:
      t.primary = Color::RGB(0, 200, 60);
      t.secondary = Color::RGB(0, 255, 80);
      t.accent = Color::RGB(50, 255, 100);
      t.border_color = Color::RGB(0, 120, 30);
      t.button_bg_active = Color::RGB(0, 180, 50);
      t.button_fg_active = Color::Black;
      t.button_bg_normal = Color::Default;
      t.button_fg_normal = Color::RGB(0, 200, 60);
      t.text = Color::RGB(0, 230, 70);
      t.text_muted = Color::RGB(0, 130, 40);
      t.error_color = Color::RGB(255, 60, 60);
      t.warning_color = Color::RGB(220, 220, 0);
      t.success_color = Color::RGB(0, 255, 80);
      break;

    case ThemeDescription::Style::Retro:
      t.primary = Color::RGB(212, 160, 60);   // amber
      t.secondary = Color::RGB(255, 140, 0);  // orange
      t.accent = Color::RGB(255, 200, 80);
      t.border_color = Color::RGB(120, 80, 20);
      t.button_bg_active = Color::RGB(200, 140, 50);
      t.button_fg_active = Color::Black;
      t.button_bg_normal = Color::Default;
      t.button_fg_normal = Color::RGB(212, 160, 60);
      t.text = Color::RGB(235, 200, 130);
      t.text_muted = Color::RGB(140, 110, 60);
      t.error_color = Color::RGB(200, 50, 50);
      t.warning_color = Color::RGB(255, 190, 0);
      t.success_color = Color::RGB(100, 200, 80);
      break;

    case ThemeDescription::Style::Space:
      t.primary = Color::RGB(50, 100, 220);  // deep blue
      t.secondary = Color::RGB(30, 60, 160);
      t.accent = Color::RGB(212, 175, 55);  // gold
      t.border_color = Color::RGB(30, 50, 120);
      t.button_bg_active = Color::RGB(50, 100, 200);
      t.button_fg_active = Color::White;
      t.button_bg_normal = Color::Default;
      t.button_fg_normal = Color::RGB(100, 150, 255);
      t.text = Color::RGB(200, 210, 240);
      t.text_muted = Color::RGB(100, 120, 180);
      t.error_color = Color::RGB(220, 50, 80);
      t.warning_color = Color::RGB(212, 175, 55);
      t.success_color = Color::RGB(50, 200, 130);
      break;

    case ThemeDescription::Style::Nature:
      t.primary = Color::RGB(50, 160, 80);
      t.secondary = Color::RGB(30, 120, 60);
      t.accent = Color::RGB(140, 220, 80);
      t.border_color = Color::RGB(40, 100, 50);
      t.button_bg_active = Color::RGB(50, 150, 70);
      t.button_fg_active = Color::White;
      t.button_bg_normal = Color::Default;
      t.button_fg_normal = Color::RGB(80, 180, 100);
      t.text = Color::RGB(200, 230, 200);
      t.text_muted = Color::RGB(100, 150, 110);
      t.error_color = Color::RGB(200, 60, 60);
      t.warning_color = Color::RGB(220, 180, 0);
      t.success_color = Color::RGB(60, 210, 90);
      break;

    case ThemeDescription::Style::Warm:
      t.primary = Color::RGB(220, 80, 40);  // orange-red
      t.secondary = Color::RGB(200, 120, 30);
      t.accent = Color::RGB(255, 160, 50);
      t.border_color = Color::RGB(140, 60, 20);
      t.button_bg_active = Color::RGB(210, 80, 40);
      t.button_fg_active = Color::White;
      t.button_bg_normal = Color::Default;
      t.button_fg_normal = Color::RGB(240, 130, 60);
      t.text = Color::RGB(240, 210, 190);
      t.text_muted = Color::RGB(160, 120, 100);
      t.error_color = Color::RGB(200, 40, 40);
      t.warning_color = Color::RGB(255, 180, 0);
      t.success_color = Color::RGB(80, 200, 80);
      break;

    case ThemeDescription::Style::Corporate:
      t.primary = Color::RGB(0, 90, 180);
      t.secondary = Color::RGB(0, 60, 130);
      t.accent = Color::RGB(0, 140, 220);
      t.border_color = Color::RGB(80, 100, 130);
      t.button_bg_active = Color::RGB(0, 90, 180);
      t.button_fg_active = Color::White;
      t.button_bg_normal = Color::Default;
      t.button_fg_normal = Color::RGB(0, 120, 200);
      t.text = Color::RGB(220, 225, 235);
      t.text_muted = Color::RGB(140, 155, 175);
      t.error_color = Color::RGB(200, 40, 40);
      t.warning_color = Color::RGB(220, 160, 0);
      t.success_color = Color::RGB(0, 180, 80);
      break;

    case ThemeDescription::Style::Minimal:
    default:
      t.primary = Color::RGB(100, 150, 220);
      t.secondary = Color::RGB(80, 120, 180);
      t.accent = Color::RGB(130, 180, 250);
      t.border_color = Color::RGB(80, 90, 110);
      t.button_bg_active = Color::RGB(100, 140, 210);
      t.button_fg_active = Color::White;
      t.button_bg_normal = Color::Default;
      t.button_fg_normal = Color::RGB(140, 170, 220);
      t.text = Color::RGB(220, 225, 235);
      t.text_muted = Color::RGB(140, 150, 170);
      t.error_color = Color::RGB(200, 60, 60);
      t.warning_color = Color::RGB(210, 170, 0);
      t.success_color = Color::RGB(60, 190, 80);
      break;
  }

  // ── Override with explicit colors from description ────────────────────
  if (desc.primary_color) {
    const auto& c = *desc.primary_color;
    t.primary = Color::RGB(c.r, c.g, c.b);
    // Derive button active from primary.
    t.button_bg_active = t.primary;
  }
  if (desc.accent_color) {
    const auto& c = *desc.accent_color;
    t.accent = Color::RGB(c.r, c.g, c.b);
    t.secondary = t.accent;
  }

  // ── Mood-based overrides ──────────────────────────────────────────────
  switch (desc.mood) {
    case ThemeDescription::Mood::Dark:
      // Dark backgrounds are implied by the style-specific colors above.
      // Ensure text is light.
      if (!desc.primary_color) {
        // Already set by style.
      }
      break;
    case ThemeDescription::Mood::Light:
      t.text = Color::RGB(20, 20, 30);
      t.text_muted = Color::RGB(90, 100, 120);
      t.border_color = Color::RGB(180, 190, 210);
      t.button_fg_normal = Color::RGB(40, 50, 80);
      t.border_style = LIGHT;
      break;
    case ThemeDescription::Mood::Neutral:
      break;
  }

  // ── Modifier application ──────────────────────────────────────────────
  if (desc.neon) {
    t.primary = Neon(t.primary);
    t.accent = Neon(t.accent);
    t.secondary = Neon(t.secondary);
  }
  if (desc.muted) {
    t.primary = Muted(t.primary);
    t.accent = Muted(t.accent);
    t.secondary = Muted(t.secondary);
    t.text_muted = Muted(t.text_muted);
  }
  if (desc.high_contrast) {
    t.text = Color::RGB(255, 255, 255);
    t.border_color = t.primary;
  }

  return t;
}

Theme NLThemeParser::FromDescription(const std::string& description) {
  return ToTheme(Parse(description));
}

// ─────────────────────────────────────────────────────────────────────────────
// LerpTheme
// ─────────────────────────────────────────────────────────────────────────────

Theme LerpTheme(const Theme& a, const Theme& b, float t) {
  t = std::max(0.f, std::min(1.f, t));
  Theme result;
  result.primary = LerpColor(a.primary, b.primary, t);
  result.secondary = LerpColor(a.secondary, b.secondary, t);
  result.accent = LerpColor(a.accent, b.accent, t);
  result.error_color = LerpColor(a.error_color, b.error_color, t);
  result.warning_color = LerpColor(a.warning_color, b.warning_color, t);
  result.success_color = LerpColor(a.success_color, b.success_color, t);
  result.text = LerpColor(a.text, b.text, t);
  result.text_muted = LerpColor(a.text_muted, b.text_muted, t);
  result.border_color = LerpColor(a.border_color, b.border_color, t);
  result.button_bg_normal =
      LerpColor(a.button_bg_normal, b.button_bg_normal, t);
  result.button_fg_normal =
      LerpColor(a.button_fg_normal, b.button_fg_normal, t);
  result.button_bg_active =
      LerpColor(a.button_bg_active, b.button_bg_active, t);
  result.button_fg_active =
      LerpColor(a.button_fg_active, b.button_fg_active, t);
  result.border_style = (t < 0.5f) ? a.border_style : b.border_style;
  result.animations_enabled =
      (t < 0.5f) ? a.animations_enabled : b.animations_enabled;
  return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// ThemeTransition
// ─────────────────────────────────────────────────────────────────────────────

ThemeTransition::ThemeTransition(Theme from,
                                 Theme to,
                                 std::chrono::milliseconds duration)
    : from_(std::move(from)), to_(std::move(to)), duration_(duration) {}

ThemeTransition::~ThemeTransition() {
  if (frame_callback_id_ >= 0) {
    AnimationLoop::Instance().RemoveCallback(frame_callback_id_);
  }
}

void ThemeTransition::Start() {
  started_ = true;
  start_time_ = Clock::now();
  if (frame_callback_id_ >= 0) {
    AnimationLoop::Instance().RemoveCallback(frame_callback_id_);
  }
  // Register a frame callback that applies the interpolated theme each frame.
  frame_callback_id_ = AnimationLoop::Instance().OnFrame([this](float /*dt*/) {
    if (!started_ || Complete()) {
      return;
    }
    SetTheme(Current());
  });
}

void ThemeTransition::Reset() {
  started_ = false;
  if (frame_callback_id_ >= 0) {
    AnimationLoop::Instance().RemoveCallback(frame_callback_id_);
    frame_callback_id_ = -1;
  }
}

float ThemeTransition::Progress() const {
  if (!started_) {
    return 0.f;
  }
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      Clock::now() - start_time_);
  if (duration_.count() <= 0) {
    return 1.f;
  }
  float p = static_cast<float>(elapsed.count()) /
            static_cast<float>(duration_.count());
  return std::max(0.f, std::min(1.f, p));
}

bool ThemeTransition::Complete() const {
  return started_ && Progress() >= 1.f;
}

Theme ThemeTransition::Current() const {
  return LerpTheme(from_, to_, Progress());
}

// ─────────────────────────────────────────────────────────────────────────────
// LLMThemeGenerator
// ─────────────────────────────────────────────────────────────────────────────

bool LLMThemeGenerator::IsAvailable() {
  return LLMAdapter::OllamaAvailable() || LLMAdapter::OpenAIAvailable();
}

std::string LLMThemeGenerator::SystemPrompt() {
  return R"(You are a terminal UI theme generator. Given a description, respond ONLY with valid JSON like:
{"primary":"#5C2D91","accent":"#00D4AA","secondary":"#3B1F6E","text":"#E0D0FF","text_muted":"#7B6FAA","border":"#500090","button_active_bg":"#5C2D91","button_active_fg":"#000000","error":"#FF3264","warning":"#FFD700","success":"#00FF80","style":"cyberpunk"}
No explanation, no markdown, just JSON.)";
}

namespace {

/// Parse a hex color string like "#RRGGBB" or "RRGGBB".
std::optional<Color> ParseHexColor(const std::string& hex) {
  std::string h = hex;
  if (!h.empty() && h[0] == '#') {
    h = h.substr(1);
  }
  if (h.size() != 6) {
    return std::nullopt;
  }
  try {
    uint8_t r = static_cast<uint8_t>(std::stoul(h.substr(0, 2), nullptr, 16));
    uint8_t g = static_cast<uint8_t>(std::stoul(h.substr(2, 2), nullptr, 16));
    uint8_t b = static_cast<uint8_t>(std::stoul(h.substr(4, 2), nullptr, 16));
    return Color::RGB(r, g, b);
  } catch (...) {
    return std::nullopt;
  }
}

/// Extract a value for a JSON key like "primary":"#..." from a raw string.
std::string ExtractJsonString(const std::string& json, const std::string& key) {
  // Find "key":"value"
  std::string search = "\"" + key + "\":\"";
  auto pos = json.find(search);
  if (pos == std::string::npos) {
    return {};
  }
  pos += search.size();
  auto end = json.find('"', pos);
  if (end == std::string::npos) {
    return {};
  }
  return json.substr(pos, end - pos);
}

Theme ThemeFromLLMJson(const std::string& json,
                       const std::string& fallback_description) {
  Theme t = NLThemeParser::FromDescription(fallback_description);

  auto apply = [&](const std::string& key, Color& field) {
    auto val = ExtractJsonString(json, key);
    if (!val.empty()) {
      if (auto c = ParseHexColor(val)) {
        field = *c;
      }
    }
  };

  apply("primary", t.primary);
  apply("accent", t.accent);
  apply("secondary", t.secondary);
  apply("text", t.text);
  apply("text_muted", t.text_muted);
  apply("border", t.border_color);
  apply("button_active_bg", t.button_bg_active);
  apply("button_active_fg", t.button_fg_active);
  apply("error", t.error_color);
  apply("warning", t.warning_color);
  apply("success", t.success_color);
  return t;
}

}  // namespace

Theme LLMThemeGenerator::Generate(const std::string& description,
                                  const std::string& model) {
  if (!IsAvailable()) {
    return NLThemeParser::FromDescription(description);
  }
  // Build a minimal intent that carries our theme prompt.
  // LLMAdapter::QueryLLM returns a UIIntent, but we only need the raw
  // JSON text.  We re-use the "raw_input" round-trip trick: pass the system
  // prompt embedded in the user prompt and capture the response via the
  // options map.
  //
  // Since LLMAdapter was designed for UIIntent, we do a lightweight HTTP
  // call ourselves here using the same socket approach.  As a simpler
  // alternative we fall back to NLThemeParser and patch colors from any
  // JSON found in the LLM response.
  UIIntent intent = LLMAdapter::QueryLLM(
      SystemPrompt() + "\n\nTheme description: " + description, model);

  // The LLM response may be embedded in the raw_input field or in options.
  // Try to extract JSON from anywhere in the raw_input.
  const std::string& raw = intent.raw_input;
  auto jstart = raw.find('{');
  auto jend = raw.rfind('}');
  if (jstart != std::string::npos && jend != std::string::npos &&
      jend > jstart) {
    std::string json = raw.substr(jstart, jend - jstart + 1);
    return ThemeFromLLMJson(json, description);
  }
  return NLThemeParser::FromDescription(description);
}

std::vector<Theme> LLMThemeGenerator::GenerateVariants(
    const std::string& description,
    int count) {
  std::vector<Theme> result;
  result.reserve(static_cast<size_t>(count));

  Theme base = Generate(description);
  result.push_back(base);

  // Produce variants by adjusting saturation / mood.
  if (count > 1) {
    Theme lighter = base;
    lighter.text = Color::RGB(240, 240, 240);
    lighter.border_color = AdjustSaturation(base.primary, 0.6f);
    result.push_back(lighter);
  }
  if (count > 2) {
    Theme saturated = base;
    saturated.primary = Neon(base.primary);
    saturated.accent = Neon(base.accent);
    result.push_back(saturated);
  }
  // Fill remaining slots with the base theme.
  while (static_cast<int>(result.size()) < count) {
    result.push_back(base);
  }
  return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// ThemePicker
// ─────────────────────────────────────────────────────────────────────────────

ftxui::Component ThemePicker(std::function<void(Theme)> on_theme_selected) {
  struct State {
    std::string input;
    Theme preview = Theme::Dark();
    bool applied = false;
    // Debounce: track last change time.
    std::chrono::steady_clock::time_point last_change =
        std::chrono::steady_clock::now() - std::chrono::seconds(10);
  };
  auto state = std::make_shared<State>();

  auto input_component = ftxui::Input(&state->input, "Describe your theme…");

  auto apply_btn = ftxui::Button("Apply", [state, on_theme_selected] {
    if (on_theme_selected) {
      on_theme_selected(state->preview);
    }
    state->applied = true;
  });

  auto container = ftxui::Container::Vertical({
      input_component,
      apply_btn,
  });

  auto renderer = ftxui::Renderer(
      container, [state, input_component, apply_btn]() -> ftxui::Element {
        // Debounced theme generation (500 ms).
        auto now = std::chrono::steady_clock::now();
        if (now - state->last_change > std::chrono::milliseconds(500) &&
            !state->input.empty()) {
          state->preview = NLThemeParser::FromDescription(state->input);
          state->last_change =
              std::chrono::steady_clock::now() + std::chrono::seconds(100);
        }

        const Theme& t = state->preview;

        // Build color swatch grid (3×3).
        auto swatch = [](Color c, const std::string& label) -> ftxui::Element {
          return ftxui::hbox({
              ftxui::text("  ") | ftxui::bgcolor(c),
              ftxui::text(" " + label) | ftxui::color(c),
          });
        };

        auto swatches = ftxui::vbox({
            ftxui::hbox({
                swatch(t.primary, "primary   "),
                ftxui::text("  "),
                swatch(t.accent, "accent    "),
                ftxui::text("  "),
                swatch(t.secondary, "secondary "),
            }),
            ftxui::hbox({
                swatch(t.success_color, "success   "),
                ftxui::text("  "),
                swatch(t.warning_color, "warning   "),
                ftxui::text("  "),
                swatch(t.error_color, "error     "),
            }),
            ftxui::hbox({
                swatch(t.text, "text      "),
                ftxui::text("  "),
                swatch(t.text_muted, "muted     "),
                ftxui::text("  "),
                swatch(t.border_color, "border    "),
            }),
        });

        std::string status =
            LLMThemeGenerator::IsAvailable() ? "● LLM: Active" : "○ Rule-based";

        return ftxui::vbox({
                   ftxui::text("Theme Description") | ftxui::bold |
                       ftxui::color(t.primary),
                   ftxui::separator(),
                   input_component->Render(),
                   ftxui::separator(),
                   ftxui::text("Preview") | ftxui::bold,
                   swatches,
                   ftxui::separator(),
                   apply_btn->Render(),
                   ftxui::text(status) | ftxui::color(t.text_muted),
               }) |
               ftxui::border;
      });

  // Hook onChange to reset debounce timer.
  input_component->OnEvent(ftxui::Event::Character('a'));  // warm-up

  // Wrap so that input changes reset the debounce.
  auto wrapped = ftxui::CatchEvent(renderer, [state](ftxui::Event /*e*/) {
    state->last_change = std::chrono::steady_clock::now();
    return false;  // let the event propagate
  });

  return wrapped;
}

// ─────────────────────────────────────────────────────────────────────────────
// Free functions
// ─────────────────────────────────────────────────────────────────────────────

Theme ThemeFromDescription(const std::string& description) {
  return LLMThemeGenerator::Generate(description);
}

void ThemeFromDescriptionAsync(const std::string& description,
                               std::function<void(Theme)> callback) {
  if (!LLMThemeGenerator::IsAvailable()) {
    // Synchronous fallback.
    callback(NLThemeParser::FromDescription(description));
    return;
  }
  // Run LLM query on a background thread.
  std::thread([description, callback = std::move(callback)] {
    Theme t = LLMThemeGenerator::Generate(description);
    callback(std::move(t));
  }).detach();
}

}  // namespace ftxui::ui
