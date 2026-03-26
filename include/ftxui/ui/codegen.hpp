// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_CODEGEN_HPP
#define FTXUI_UI_CODEGEN_HPP

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

// ── CodeGenRequest
// ────────────────────────────────────────────────────────────

/// @brief Parameters for a code generation request.
/// @ingroup ui
struct CodeGenRequest {
  std::string description;            ///< e.g. "kubernetes pod monitor"
  std::string style = "modern";       ///< modern, minimal, matrix, lcars
  std::vector<std::string> features;  ///< ["charts", "tables", "real-time"]
  std::string target_file;            ///< output .cpp path (empty = stdout)
  std::string model = "llama3";       ///< LLM model to use
  bool include_cmake = true;          ///< also generate CMakeLists snippet
  bool include_comments = true;       ///< generate well-commented code
  int max_tokens = 4000;
};

// ── CodeGenResult
// ─────────────────────────────────────────────────────────────

/// @brief Result of a code generation request.
/// @ingroup ui
struct CodeGenResult {
  bool success = false;
  std::string code;           ///< generated C++ code
  std::string cmake_snippet;  ///< CMakeLists.txt addition
  std::string description;    ///< what was generated
  std::string model_used;
  std::chrono::milliseconds elapsed{0};
  std::string error;

  /// Write code to a file. Returns true on success.
  bool WriteToFile(const std::string& path) const;

  /// Validate: does it at least include necessary headers?
  bool LooksValid() const;
};

// ── CodeTemplate
// ──────────────────────────────────────────────────────────────

/// @brief Built-in template that works without LLM.
/// @ingroup ui
struct CodeTemplate {
  std::string name;
  std::string description;
  std::vector<std::string> keywords;  ///< triggers: "monitor", "dashboard", …
  std::string code;                   ///< template code with {PLACEHOLDERS}
  std::string cmake_snippet;
};

// ── TemplateLibrary
// ───────────────────────────────────────────────────────────

/// @brief Library of built-in code templates — no LLM required.
///
/// Provides 8 built-in templates:
///   "dashboard", "monitor", "crud", "chat", "explorer",
///   "wizard", "game", "map"
///
/// @ingroup ui
class TemplateLibrary {
 public:
  /// Return all built-in templates.
  static const std::vector<CodeTemplate>& BuiltinTemplates();

  /// Find best matching template for a description (keyword scoring).
  /// Returns nullptr if no template scores ≥ 1.
  static const CodeTemplate* FindBest(const std::string& description);

  /// Fill template placeholders.  {KEY} → vars["KEY"].
  /// Unknown placeholders are left as-is.
  static std::string Fill(const CodeTemplate& tmpl,
                          const std::map<std::string, std::string>& vars);
};

// ── CodeGenerator
// ─────────────────────────────────────────────────────────────

/// @brief Generates complete, compilable FTXUI BadWolf applications.
///
/// Uses Ollama (or OpenAI) when available; falls back to TemplateLibrary.
///
/// @ingroup ui
class CodeGenerator {
 public:
  explicit CodeGenerator(std::string model = "llama3");

  /// Synchronous generation (blocks until LLM responds or falls back).
  CodeGenResult Generate(const CodeGenRequest& req);

  /// Async generation with streaming token callback.
  void GenerateAsync(const CodeGenRequest& req,
                     std::function<void(const std::string& token)> on_token,
                     std::function<void(CodeGenResult)> on_complete);

  /// Template-only generation — never contacts LLM.
  CodeGenResult GenerateFromTemplate(const CodeGenRequest& req);

  /// Returns true if an Ollama server is reachable.
  bool LLMAvailable() const;

  /// Return the configured model name.
  std::string ModelName() const;

  /// Build the system prompt sent to the LLM.
  static std::string BuildSystemPrompt();

  /// Build the user prompt for a specific request.
  static std::string BuildUserPrompt(const CodeGenRequest& req);

 private:
  std::string model_;

  /// Extract the first ```cpp … ``` block from an LLM response.
  void ExtractCode(const std::string& llm_response, CodeGenResult& result);

  /// Send a raw prompt to Ollama and return the raw text response.
  std::string QueryOllama(const std::string& system_prompt,
                          const std::string& user_prompt);
};

// ── CodeGenStudio
// ─────────────────────────────────────────────────────────────

/// @brief Interactive in-app code generation component.
///
/// Layout:
///   - Description input, style/feature toggles
///   - [Generate →] and [From Template] buttons
///   - Syntax-highlighted code preview
///   - [Copy] / [Save As…] actions
///
/// @param on_generated  Optional callback invoked after successful generation.
/// @ingroup ui
ftxui::Component CodeGenStudio(
    std::function<void(const CodeGenResult&)> on_generated = {});

// ── CodePreview
// ───────────────────────────────────────────────────────────────

/// @brief Scrollable, syntax-highlighted code view component.
/// @param code      Source code text.
/// @param language  Language hint ("cpp" is the only value currently used).
/// @ingroup ui
ftxui::Component CodePreview(const std::string& code,
                             const std::string& language = "cpp");

// ── SyntaxHighlighter
// ─────────────────────────────────────────────────────────

/// @brief Minimal C++ syntax highlighter for terminal display.
/// @ingroup ui
class SyntaxHighlighter {
 public:
  enum class TokenType {
    Keyword,
    String,
    Comment,
    Number,
    Preprocessor,
    Operator,
    Normal,
  };

  struct Token {
    std::string text;
    TokenType type;
  };

  /// Tokenize a single line of C++ source.
  static std::vector<Token> Tokenize(const std::string& line);

  /// Return a colored ftxui::Element for a single line.
  static ftxui::Element HighlightLine(const std::string& line,
                                      const std::string& language = "cpp");

  /// Highlight a full source file; return one Element per line.
  static std::vector<ftxui::Element> Highlight(
      const std::string& code,
      const std::string& language = "cpp");
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_CODEGEN_HPP
