// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file typewriter_demo.cpp
///
/// Demonstrates the FTXUI console typewriter engine:
///   Tab 1: Typewriter  — character-by-character animation with replay/speed
///   Tab 2: Sequence    — TypewriterSequence with Matrix green styling
///   Tab 3: Console     — Styled console with Info/Warn/Error/Success + Clear
///   Tab 4: REPL        — ConsolePrompt interactive REPL

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/typewriter.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Tab 1: TypewriterText ──────────────────────────────────────────────────

static const std::string kIntroText =
    "FTXUI BadWolf — The C++ Terminal UI Framework.\n"
    "Animate text character by character with zero boilerplate.\n"
    "Driven by AnimationLoop at 60 fps.";

Component MakeTypewriterTab() {
  struct State {
    int cps = 30;
    Component tw = TypewriterText(kIntroText);
  };
  auto st = std::make_shared<State>();

  auto rebuild = [st]() {
    TypewriterConfig cfg;
    cfg.chars_per_second = st->cps;
    st->tw = TypewriterText(kIntroText, cfg);
  };

  auto btn_replay = Button(" Replay (30 cps) ", [st, rebuild]() mutable {
    st->cps = 30;
    rebuild();
  });
  auto btn_fast = Button(" Fast (200 cps) ", [st, rebuild]() mutable {
    st->cps = 200;
    rebuild();
  });
  auto btn_slow = Button(" Slow (5 cps) ", [st, rebuild]() mutable {
    st->cps = 5;
    rebuild();
  });

  auto buttons = Container::Horizontal({btn_replay, btn_fast, btn_slow});

  return Renderer(buttons, [=]() -> Element {
    return vbox({
               text(" Typewriter Demo ") | bold | hcenter,
               separatorLight(),
               vbox({st->tw->Render()}) | flex | border,
               separatorEmpty(),
               hbox({
                   btn_replay->Render() | flex,
                   btn_fast->Render() | flex,
                   btn_slow->Render() | flex,
               }),
           }) |
           flex;
  });
}

// ── Tab 2: TypewriterSequence ──────────────────────────────────────────────

Component MakeSequenceTab() {
  auto seq_comp = std::make_shared<Component>(nullptr);

  auto build_seq = [seq_comp]() {
    TypewriterConfig matrix;
    matrix.chars_per_second = 20;
    matrix.show_cursor = true;

    TypewriterConfig fast;
    fast.chars_per_second = 50;
    fast.show_cursor = false;

    *seq_comp = TypewriterSequence()
                    .Then("INITIATING BADWOLF SYSTEMS...", matrix)
                    .Pause(std::chrono::milliseconds(600))
                    .Then("\nLOADING MODULES...", matrix)
                    .Pause(std::chrono::milliseconds(400))
                    .Then("\nALL SYSTEMS NOMINAL.", fast)
                    .Build();
  };
  build_seq();

  auto btn_restart = Button(" Restart ", [build_seq]() { build_seq(); });

  return Renderer(btn_restart, [=]() -> Element {
    Element seq_elem = (*seq_comp) ? (*seq_comp)->Render() : text("");
    return vbox({
               text(" TypewriterSequence Demo ") | bold | hcenter,
               separatorLight(),
               vbox({seq_elem | color(Color::Green)}) | flex | border,
               separatorEmpty(),
               btn_restart->Render() | hcenter,
           }) |
           flex;
  });
}

// ── Tab 3: Console ─────────────────────────────────────────────────────────

Component MakeConsoleTab() {
  auto con = Console::Create(200);
  con->Info("Console demo started");
  con->Warn("Low memory warning");
  con->Error("Connection refused");
  con->Success("Reconnected successfully");
  con->PrintBold("=== FTXUI BadWolf Console ===");

  auto console_comp = con->Build(" Console Output ");

  auto btn_info = Button(" Info ", [con]() { con->Info("Server OK"); });
  auto btn_warn = Button(" Warn ", [con]() { con->Warn("Queue: 9800/10000"); });
  auto btn_error = Button(" Error ", [con]() { con->Error("Worker crash"); });
  auto btn_ok = Button(" Success ", [con]() { con->Success("Task done"); });
  auto btn_type = Button(" TypePrint ", [con]() {
    TypewriterConfig cfg;
    cfg.chars_per_second = 40;
    con->TypePrint(
        "Streaming: analyzing 50,000 rows... anomaly at index 42137.", cfg,
        Color::GreenLight);
  });
  auto btn_clear = Button(" Clear ", [con]() { con->Clear(); });

  auto btns = Container::Horizontal(
      {btn_info, btn_warn, btn_error, btn_ok, btn_type, btn_clear});
  auto layout = Container::Vertical({console_comp, btns});

  return Renderer(layout, [=]() -> Element {
    return vbox({
               text(" Console Demo ") | bold | hcenter,
               separatorLight(),
               console_comp->Render() | flex,
               separatorLight(),
               hbox({
                   btn_info->Render(),
                   btn_warn->Render(),
                   btn_error->Render(),
                   btn_ok->Render(),
                   btn_type->Render() | flex,
                   btn_clear->Render(),
               }),
           }) |
           flex;
  });
}

// ── Tab 4: REPL ────────────────────────────────────────────────────────────

Component MakeReplTab() {
  auto con = Console::Create(500);
  con->PrintBold("FTXUI BadWolf REPL v1.0");
  con->Info("Type 'help' for available commands.");

  ConsolePromptConfig cfg;
  cfg.prompt_str = "badwolf> ";
  cfg.prompt_color = Color::GreenLight;
  cfg.placeholder = "enter command...";
  cfg.typewriter_output = true;
  cfg.typewriter_cfg.chars_per_second = 60;

  cfg.handler = [](const std::string& cmd) -> std::string {
    if (cmd == "help") {
      return "Commands: help  status  clear  version  echo <text>";
    }
    if (cmd == "status") {
      return "ALL SYSTEMS NOMINAL | CPU: 12% | MEM: 245MB | Uptime: 3d 14h";
    }
    if (cmd == "clear") {
      return "";
    }
    if (cmd == "version") {
      return "FTXUI BadWolf v1.0.0 — C++20 Terminal UI Framework";
    }
    if (cmd.size() > 5 && cmd.substr(0, 4) == "echo") {
      return cmd.substr(5);
    }
    return "[ERROR] Unknown command: '" + cmd + "'. Try 'help'.";
  };

  auto prompt = ConsolePrompt(con, cfg);

  return Renderer(prompt, [=]() -> Element {
    return vbox({
               text(" Interactive REPL Demo ") | bold | hcenter,
               separatorLight(),
               prompt->Render() | flex,
           }) |
           flex;
  });
}

// ── Main ───────────────────────────────────────────────────────────────────

int main() {
  AnimationLoop::Instance().SetFPS(60);
  SetTheme(Theme::Dark());

  std::vector<std::string> tab_names = {"Typewriter", "Sequence", "Console",
                                        "REPL"};
  int selected_tab = 0;

  auto tab_toggle = Toggle(&tab_names, &selected_tab);
  auto tab1 = MakeTypewriterTab();
  auto tab2 = MakeSequenceTab();
  auto tab3 = MakeConsoleTab();
  auto tab4 = MakeReplTab();
  auto tab_content = Container::Tab({tab1, tab2, tab3, tab4}, &selected_tab);
  auto main_layout = Container::Vertical({tab_toggle, tab_content});

  auto root = Renderer(main_layout, [&]() -> Element {
    return vbox({
               text(" ◈ FTXUI BadWolf — Console Typewriter Engine ◈ ") | bold |
                   hcenter | color(Color::Cyan),
               separatorLight(),
               tab_toggle->Render() | hcenter,
               separatorLight(),
               tab_content->Render() | flex,
           }) |
           flex;
  });

  root |= CatchEvent([](Event e) {
    if (e == Event::Character('q')) {
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    return false;
  });

  App::Fullscreen().Loop(root);
  return 0;
}
