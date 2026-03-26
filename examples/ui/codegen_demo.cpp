// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file codegen_demo.cpp
/// @brief Demonstrates the BadWolf AI Code Generator — CodeGenStudio,
///        TemplateLibrary browser, syntax highlighting, and live preview.

#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/codegen.hpp"
#include "ftxui/ui/llm_bridge.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Dark());

  // ── History of last 5 generated results ─────────────────────────────────
  struct HistoryEntry {
    std::string description;
    std::string code;
  };
  auto history = std::make_shared<std::vector<HistoryEntry>>();
  int history_sel = 0;

  // ── Template library browser ─────────────────────────────────────────────
  const auto& templates = TemplateLibrary::BuiltinTemplates();

  std::vector<std::string> tmpl_names;
  for (const auto& t : templates) {
    tmpl_names.push_back(t.name);
  }

  int tmpl_sel = 0;

  // ── LLM status ───────────────────────────────────────────────────────────
  bool llm_ok = LLMAdapter::OllamaAvailable();

  // ── Currently previewed code ──────────────────────────────────────────────
  auto current_code = std::make_shared<std::string>();
  auto preview = CodePreview(*current_code);

  // ── CodeGenStudio ─────────────────────────────────────────────────────────
  auto studio = CodeGenStudio([&](const CodeGenResult& r) {
    *current_code = r.code;
    if (history->size() >= 5) {
      history->erase(history->begin());
    }
    history->push_back({r.description, r.code});
    history_sel = static_cast<int>(history->size()) - 1;
  });

  // ── Template list ─────────────────────────────────────────────────────────
  auto tmpl_list = Menu(&tmpl_names, &tmpl_sel);

  auto tmpl_panel = Renderer(tmpl_list, [&] {
    Elements rows;
    rows.push_back(text(" Template Library") | bold | color(Color::Cyan));
    rows.push_back(separator());

    for (int i = 0; i < static_cast<int>(templates.size()); ++i) {
      const auto& t = templates[i];
      bool sel = (i == tmpl_sel);
      Element row = hbox({
          text(sel ? "▶ " : "  "),
          text(t.name) | bold | size(WIDTH, EQUAL, 12),
          text(t.description) | dim | flex,
      });
      if (sel) {
        row = row | color(Color::Cyan);
      }
      rows.push_back(row);
    }

    rows.push_back(separator());
    rows.push_back(text(" Press Enter to preview") | dim);

    return vbox(std::move(rows)) | border;
  });

  // Load template preview on selection
  auto tmpl_panel_interactive = CatchEvent(tmpl_panel, [&](Event ev) {
    if (ev == Event::Return && tmpl_sel >= 0 &&
        tmpl_sel < static_cast<int>(templates.size())) {
      const auto& t = templates[tmpl_sel];
      std::map<std::string, std::string> vars = {
          {"APP_NAME", t.name},
          {"TIMESTAMP", "2024-01-01"},
      };
      *current_code = TemplateLibrary::Fill(t, vars);
    }
    return false;
  });

  // ── History panel ─────────────────────────────────────────────────────────
  auto history_panel = Renderer([&] {
    Elements rows;
    rows.push_back(text(" Recent History") | bold | color(Color::Cyan));
    rows.push_back(separator());

    if (history->empty()) {
      rows.push_back(text(" (none yet)") | dim);
    } else {
      for (int i = static_cast<int>(history->size()) - 1; i >= 0; --i) {
        bool sel = (i == history_sel);
        rows.push_back(hbox({
                           text(sel ? "▶ " : "  "),
                           text((*history)[i].description) | flex,
                       }) |
                       (sel ? color(Color::Cyan) : color(Color::White)));
      }
    }

    return vbox(std::move(rows)) | border;
  });

  // ── LLM status bar ────────────────────────────────────────────────────────
  auto status_bar = Renderer([&, llm_ok] {
    return hbox({
               text(" LLM: "),
               llm_ok ? (text("● Ollama Connected") | color(Color::Green))
                      : (text("● Offline (using templates)") |
                         color(Color::Yellow)),
               filler(),
               text(" Q/Esc: quit "),
           }) |
           color(Color::GrayDark);
  });

  // ── Preview panel (syntax-highlighted code) ───────────────────────────────
  // We re-render the preview dynamically based on current_code
  auto live_preview = Renderer([&] {
    if (current_code->empty()) {
      return vbox({
                 text(" Code Preview") | bold | color(Color::Cyan),
                 separator(),
                 text("  (generate or select a template to see code here)") |
                     dim,
             }) |
             border;
    }

    auto lines = SyntaxHighlighter::Highlight(*current_code);
    int total = static_cast<int>(lines.size());
    Elements visible;
    visible.reserve(std::min(total, 25));
    for (int i = 0; i < std::min(total, 25); ++i) {
      std::string num = std::to_string(i + 1);
      while (num.size() < 4) {
        num = " " + num;
      }
      visible.push_back(hbox({
          text(num) | dim | size(WIDTH, EQUAL, 4),
          text(" │ ") | dim,
          lines[i] | flex,
      }));
    }
    if (total > 25) {
      visible.push_back(
          text("    … " + std::to_string(total - 25) + " more lines …") | dim);
    }

    return vbox({
               hbox({
                   text(" Code Preview (" + std::to_string(total) + " lines)") |
                       bold | color(Color::Cyan),
                   filler(),
               }),
               separator(),
               vbox(std::move(visible)),
           }) |
           border | flex;
  });

  // ── Main layout ───────────────────────────────────────────────────────────
  auto left_panel = Container::Vertical({
      tmpl_panel_interactive,
      history_panel,
  });

  auto left_renderer = Renderer(left_panel, [&] {
    return vbox({
        tmpl_panel_interactive->Render() | flex,
        history_panel->Render(),
    });
  });

  auto right_panel = Container::Vertical({
      studio,
  });

  auto right_renderer = Renderer(right_panel, [&] {
    return vbox({
        studio->Render() | flex,
        live_preview->Render(),
    });
  });

  auto split = ResizableSplitLeft(left_renderer | size(WIDTH, EQUAL, 28),
                                  right_renderer | flex, nullptr);

  auto root = Container::Vertical({split});
  auto main_renderer = Renderer(root, [&] {
    return vbox({
        split->Render() | flex,
        status_bar->Render(),
    });
  });

  auto screen = ScreenInteractive::Fullscreen();
  main_renderer |= CatchEvent([&](Event ev) {
    if (ev == Event::Character('q') || ev == Event::Escape) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });

  screen.Loop(main_renderer);
  return 0;
}
