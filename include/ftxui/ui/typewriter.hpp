// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_TYPEWRITER_HPP
#define FTXUI_UI_TYPEWRITER_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

// ── TypewriterText
// ──────────────────────────────────────────────────────────── Animates text
// character-by-character. Integrates with AnimationLoop.

struct TypewriterConfig {
  int chars_per_second = 30;          ///< Typing speed in chars/sec.
  bool show_cursor = true;            ///< Show blinking cursor while typing.
  char cursor_char = '_';             ///< Cursor glyph.
  bool cursor_blink = true;           ///< Blink the cursor.
  int blink_hz = 2;                   ///< Cursor blink frequency in Hz.
  std::function<void()> on_complete;  ///< Fires when last char displayed.
  std::function<void(char)> on_char;  ///< Fires for each character typed.
};

/// @brief Returns a Component that types out `text` character by character.
/// Rendering is driven by AnimationLoop — no manual timer needed.
/// Gracefully degrades to instant display when chars_per_second <= 0.
/// @ingroup ui
ftxui::Component TypewriterText(std::string text, TypewriterConfig cfg = {});

// ── TypewriterSequence
// ──────────────────────────────────────────────────────── Chains multiple
// typewriter animations in order.

/// @brief Chains multiple typewriter animations in sequence.
/// @ingroup ui
class TypewriterSequence {
 public:
  struct Step {
    std::string text;
    TypewriterConfig cfg;
    bool is_pause = false;
    bool is_clear = false;
    std::chrono::milliseconds pause_dur{};
  };

  TypewriterSequence& Then(std::string text, TypewriterConfig cfg = {});
  TypewriterSequence& Pause(std::chrono::milliseconds duration);
  TypewriterSequence& Clear();
  TypewriterSequence& OnComplete(std::function<void()> fn);

  /// @brief Returns a Component that plays the full sequence.
  ftxui::Component Build();

 private:
  std::vector<Step> steps_;
  std::function<void()> on_complete_;
};

// ── Console
// ─────────────────────────────────────────────────────────────────── A
// scrollable output console with history, typewriter effect, and log levels.

struct ConsoleLine {
  std::string text;
  ftxui::Color fg = ftxui::Color::Default;
  ftxui::Color bg = ftxui::Color::Default;
  bool bold = false;
  bool dim = false;
  bool italic = false;
  bool blink = false;
};

/// @brief A scrollable output console with typewriter support.
///
/// Example:
/// @code
/// auto con = ftxui::ui::Console::Create();
/// con->Info("Server started");
/// con->TypePrint("Loading modules...");
/// ftxui::ui::Run(con->Build("Output"));
/// @endcode
///
/// @ingroup ui
class Console {
 public:
  static std::shared_ptr<Console> Create(int max_lines = 500);

  void Print(const std::string& text,
             ftxui::Color fg = ftxui::Color::Default,
             ftxui::Color bg = ftxui::Color::Default);
  void PrintLine(const std::string& text,
                 ftxui::Color fg = ftxui::Color::Default,
                 ftxui::Color bg = ftxui::Color::Default);
  void PrintBold(const std::string& text,
                 ftxui::Color fg = ftxui::Color::White);
  void PrintDim(const std::string& text,
                ftxui::Color fg = ftxui::Color::GrayDark);

  void Info(const std::string& msg);
  void Warn(const std::string& msg);
  void Error(const std::string& msg);
  void Success(const std::string& msg);

  /// @brief Animate text character by character into the console.
  void TypePrint(const std::string& text,
                 TypewriterConfig cfg = {},
                 ftxui::Color fg = ftxui::Color::Default);

  void Clear();
  int LineCount() const;

  /// @brief Snapshot of current lines for testing / inspection.
  std::vector<ConsoleLine> Lines() const;

  /// @brief Returns a Component that renders the console with auto-scroll.
  ftxui::Component Build(std::string_view title = "") const;

 private:
  Console() = default;
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

// ── ConsolePrompt
// ───────────────────────────────────────────────────────────── Interactive
// REPL-style console: input field + scrollable output.

struct ConsolePromptConfig {
  std::string prompt_str = "> ";
  ftxui::Color prompt_color = ftxui::Color::Green;
  std::string placeholder = "type a command...";
  int history_size = 100;  ///< Up/down arrow command history size.
  bool typewriter_output = true;
  TypewriterConfig typewriter_cfg{};
  /// @brief Command handler: receives the command, returns response string.
  std::function<std::string(const std::string&)> handler;
};

/// @brief Interactive REPL-style console component.
/// @ingroup ui
ftxui::Component ConsolePrompt(std::shared_ptr<Console> console,
                               ConsolePromptConfig cfg = {});

// ── Helpers
// ───────────────────────────────────────────────────────────────────

/// @brief Wraps an existing Component with a typewriter intro animation.
/// Displays the intro text first, then transitions to the inner component.
/// @ingroup ui
ftxui::Component WithTypewriter(ftxui::Component inner,
                                std::string intro_text,
                                TypewriterConfig cfg = {});

}  // namespace ftxui::ui

#endif  // FTXUI_UI_TYPEWRITER_HPP
