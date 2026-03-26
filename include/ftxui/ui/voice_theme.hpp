// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_VOICE_THEME_HPP
#define FTXUI_UI_VOICE_THEME_HPP

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/ui/nl_theme.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/ui/voice.hpp"

namespace ftxui::ui {

// ── VoiceThemeConfig
// ──────────────────────────────────────────────────────────
struct VoiceThemeConfig {
  VoiceConfig voice;
  std::chrono::milliseconds transition_duration{800};
  bool announce_theme = true;
  bool continuous_listening = false;
  std::vector<std::string> wake_words = {
      "theme", "apply", "activate", "switch to", "change to", "use", "set"};
};

// ── VoiceThemeCommand
// ─────────────────────────────────────────────────────────
struct VoiceThemeCommand {
  std::string raw_text;
  std::string theme_name;
  bool is_theme_command = false;
  float confidence = 0.0f;
};

// ── VoiceThemeController
// ──────────────────────────────────────────────────────
class VoiceThemeController {
 public:
  explicit VoiceThemeController(VoiceThemeConfig cfg = {});
  ~VoiceThemeController();

  void Start();
  void Stop();
  bool IsListening() const;

  void ApplyTheme(const Theme& theme);
  void ApplyThemeByName(const std::string& name);
  void ApplyThemeFromDescription(const std::string& desc);

  VoiceStatus voice_status() const;
  const Theme& current_theme() const;
  std::string last_command() const;
  float transition_progress() const;

  int OnThemeChanged(std::function<void(const Theme&, const std::string&)> fn);
  int OnCommand(std::function<void(const VoiceThemeCommand&)> fn);
  void RemoveCallback(int id);

  static VoiceThemeCommand ParseCommand(const std::string& text);
  static Theme NamedTheme(const std::string& name);
  static std::vector<std::string> AvailableThemeNames();

 private:
  VoiceThemeConfig cfg_;
  std::unique_ptr<VoiceCommandEngine> engine_;
  Theme current_theme_;
  Theme target_theme_;
  std::unique_ptr<ThemeTransition> transition_;
  std::string last_command_;
  std::map<int, std::function<void(const Theme&, const std::string&)>>
      theme_callbacks_;
  std::map<int, std::function<void(const VoiceThemeCommand&)>> cmd_callbacks_;
  int next_cb_id_{0};
  int frame_cb_id_{-1};

  void OnTranscription(const std::string& text);
  void StartTransition(const Theme& target, const std::string& desc);
};

// ── LiveThemeBar
// ──────────────────────────────────────────────────────────────
ftxui::Component LiveThemeBar(
    std::shared_ptr<VoiceThemeController> ctrl = nullptr,
    VoiceThemeConfig cfg = {});

// ── ThemeStage
// ────────────────────────────────────────────────────────────────
ftxui::Component ThemeStage();

// ── WithLiveThemes
// ────────────────────────────────────────────────────────────
ftxui::Component WithLiveThemes(
    ftxui::Component app,
    std::shared_ptr<VoiceThemeController> ctrl = nullptr,
    VoiceThemeConfig cfg = {});

// ── ThemeShowcase
// ─────────────────────────────────────────────────────────────
ftxui::Component ThemeShowcase(
    bool auto_cycle = false,
    std::chrono::milliseconds cycle_interval = std::chrono::seconds(5));

// ── ReactiveTheme
// ─────────────────────────────────────────────────────────────
class ReactiveTheme {
 public:
  static ReactiveTheme& Global();

  const Theme& Get() const;
  void Set(const Theme& t, bool animate = true);

  int OnChange(std::function<void(const Theme&)> fn);
  void RemoveOnChange(int id);

  ftxui::Component Bind(ftxui::Component c);

 private:
  ReactiveTheme() = default;

  Theme current_;
  std::map<int, std::function<void(const Theme&)>> listeners_;
  int next_id_{0};
  std::unique_ptr<ThemeTransition> transition_;
  int frame_cb_id_{-1};
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_VOICE_THEME_HPP
