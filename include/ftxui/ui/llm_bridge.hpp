// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_LLM_BRIDGE_HPP
#define FTXUI_UI_LLM_BRIDGE_HPP

#include <map>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

// ── Intent types
// ──────────────────────────────────────────────────────────────

/// @brief Recognized UI intent categories.
/// @ingroup ui
enum class IntentType {
  TABLE,
  CHART,
  FORM,
  DASHBOARD,
  MAP,
  LOG,
  PROGRESS,
  UNKNOWN,
};

/// @brief Structured representation of a parsed natural-language UI request.
/// @ingroup ui
struct UIIntent {
  IntentType type = IntentType::UNKNOWN;
  std::string entity;                          ///< "users", "metrics", …
  std::vector<std::string> fields;             ///< from "with columns …"
  std::map<std::string, std::string> options;  ///< "color"→"blue", …
  std::string raw_input;
};

// ── NLParser
// ──────────────────────────────────────────────────────────────────

/// @brief Rule-based natural-language parser — no external dependencies.
///
/// Parses free-form English descriptions into structured UIIntent objects
/// using keyword matching, positional heuristics, and simple extraction.
///
/// @ingroup ui
class NLParser {
 public:
  /// Parse a single natural-language string into a UIIntent.
  static UIIntent Parse(const std::string& natural_language);

  /// Parse a compound prompt such as "show X and display Y" into multiple
  /// intents (split on "and", "also", "plus", "then").
  static std::vector<UIIntent> ParseMulti(const std::string& nl);
};

// ── UIGenerator
// ───────────────────────────────────────────────────────────────

/// @brief Generates live FTXUI components from UIIntent objects.
///
/// Built-in sample datasets for entities: users, servers, metrics, files, logs.
///
/// @ingroup ui
class UIGenerator {
 public:
  /// Generate an interactive Component for the given intent.
  static ftxui::Component Generate(const UIIntent& intent);

  /// One-shot convenience: parse @p nl then generate a Component.
  static ftxui::Component GenerateFromText(const std::string& nl);

  /// Lightweight Element preview (no interactivity) for the given intent.
  static ftxui::Element PreviewElement(const UIIntent& intent);
};

// ── LLMAdapter
// ────────────────────────────────────────────────────────────────

/// @brief Optional LLM adapter — falls back to NLParser when unavailable.
///
/// Supports Ollama (localhost:11434) and OpenAI (via OPENAI_API_KEY env var).
/// HTTP communication uses raw POSIX sockets — no external libraries required.
///
/// @ingroup ui
class LLMAdapter {
 public:
  /// @brief Returns true if an Ollama server is listening on localhost:11434.
  static bool OllamaAvailable();

  /// @brief Returns true if the OPENAI_API_KEY environment variable is set.
  static bool OpenAIAvailable();

  /// @brief Send @p prompt to the LLM and parse the JSON response into a
  ///        UIIntent.  Falls back to NLParser::Parse if no LLM is available.
  /// @param prompt  Natural-language description.
  /// @param model   Ollama model name (default: "llama3").
  static UIIntent QueryLLM(const std::string& prompt,
                           const std::string& model = "llama3");

  /// @brief Returns the system prompt that instructs the LLM to output a
  ///        structured JSON intent.
  static std::string SystemPrompt();
};

// ── High-level components
// ─────────────────────────────────────────────────────

/// @brief Interactive REPL component — type English, get a live UI preview.
///
/// Layout:
///   - Top 80 %: generated component preview (updates on Enter)
///   - Left sidebar: last 10 command history entries
///   - Bottom: text input "Describe a UI component…"
///   - Status bar: active NLP backend
///   - Ctrl+H: toggle help overlay with example prompts
///
/// @ingroup ui
ftxui::Component LLMRepl();

/// @brief Decorator: attach a "?" help button to any component.
///
/// Pressing the button (or '?') opens an overlay that describes the component
/// using UIGenerator::PreviewElement on an UNKNOWN intent with the given name.
///
/// @param inner           The component to decorate.
/// @param component_name  Human-readable name shown in the help overlay.
/// @ingroup ui
ftxui::Component WithLLMHelp(ftxui::Component inner,
                             const std::string& component_name);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_LLM_BRIDGE_HPP
