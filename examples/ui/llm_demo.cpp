// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file llm_demo.cpp
/// @brief LLM Bridge demo — type English, get a live BadWolf UI component.
///
/// Shows the LLMRepl with a sidebar of example prompts.  Click any example
/// to run it instantly, or type your own prompt in the input field.
///
/// NLP backend priority:
///   1. Ollama (localhost:11434) — if running
///   2. Rule-based NLP           — always available

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/llm_bridge.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Dark());

  // ── Example prompts sidebar ──────────────────────────────────────────────
  const std::vector<std::string> examples = {
      "show me a table of users",
      "display a chart of server metrics",
      "create a form for user registration",
      "show world map",
      "monitor server logs",
      "show loading progress",
      "dashboard overview",
      "display a list of files",
      "plot histogram",
      "edit servers",
  };

  // Track which example is "active" (button pressed)
  auto active_intent = std::make_shared<UIIntent>();
  auto active_component = std::make_shared<ftxui::Component>(
      UIGenerator::GenerateFromText(examples[0]));
  *active_intent = NLParser::Parse(examples[0]);

  // ── Build example buttons ────────────────────────────────────────────────
  std::vector<ftxui::Component> btn_list;
  for (const auto& ex : examples) {
    auto btn =
        Button(ex, [ex, active_component, active_intent] {
          *active_intent = NLParser::Parse(ex);
          *active_component = UIGenerator::Generate(*active_intent);
        },
               ButtonOption::Ascii());
    btn_list.push_back(btn);
  }
  auto example_menu = Container::Vertical(btn_list);

  // ── Determine active backend label ───────────────────────────────────────
  std::string backend;
  if (LLMAdapter::OllamaAvailable()) {
    backend = "Ollama: llama3";
  } else {
    backend = "Rule-based NLP";
  }

  // ── Main layout ──────────────────────────────────────────────────────────
  // Left: example prompts sidebar
  // Right: LLMRepl (full interactive REPL)
  auto repl = LLMRepl();
  auto all = Container::Horizontal({example_menu, repl});

  auto root = Renderer(all, [&, active_component]() -> Element {
    const Theme& t = GetTheme();

    // Sidebar
    auto sidebar = window(
        text(" Example Prompts ") | bold | color(t.primary),
        vbox({
            text("Click to run instantly") | color(t.text_muted) | dim,
            separatorEmpty(),
            example_menu->Render() | vscroll_indicator | yflex,
            separatorEmpty(),
            separatorEmpty(),
            text("Backend: " + backend) | bold | color(t.secondary),
        }) | ystretch) | size(WIDTH, EQUAL, 34);

    // Main REPL panel
    auto main_panel = repl->Render() | flex;

    return hbox({sidebar, main_panel}) | flex;
  });

  auto app = App::TerminalOutput();
  app.Loop(root);
  return 0;
}
