// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file voice_demo.cpp
/// @brief Voice Control demo — microphone → whisper.cpp → NLParser → action.
///
/// Press V to toggle voice recording, or K to enter a keyboard-simulated
/// voice command (when whisper.cpp is not available).
///
/// Recognised commands (case-insensitive):
///   "increment counter" / "add one"    → counter++
///   "add todo <item>"                  → adds item to the todo list
///   "apply <name> theme"               → changes the theme
///   "go to <tab>"                      → switches tabs
///   "help"                             → shows command list

#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/voice.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Shared app state
// ──────────────────────────────────────────────────────────
struct AppState {
  int counter = 0;
  std::vector<std::string> todos;
  int tab_index = 0;
  std::deque<std::string> log;  // last 5 transcriptions
  bool show_help = false;
  std::string sim_input;  // keyboard simulation mode input

  void AddLog(const std::string& msg) {
    log.push_front(msg);
    if (log.size() > 5) {
      log.pop_back();
    }
  }
};

// ── Counter tab
// ───────────────────────────────────────────────────────────────
Component CounterTab(std::shared_ptr<AppState> state) {
  return Renderer([state] {
    return vbox({
               text("Counter") | bold | color(Color::Cyan),
               separator(),
               text("Value: " + std::to_string(state->counter)) | bold |
                   color(Color::Yellow),
               text("Say \"increment counter\" or \"add one\" to count up.") |
                   color(Color::GrayLight),
           }) |
           border | flex;
  });
}

// ── Todo tab ─────────────────────────────────────────────────────────────────
Component TodoTab(std::shared_ptr<AppState> state) {
  return Renderer([state] {
    Elements items;
    items.push_back(text("Todo List") | bold | color(Color::Cyan));
    items.push_back(separator());
    if (state->todos.empty()) {
      items.push_back(text("(empty — say \"add todo buy milk\")") |
                      color(Color::GrayDark));
    } else {
      for (auto& t : state->todos) {
        items.push_back(text("• " + t) | color(Color::White));
      }
    }
    return vbox(std::move(items)) | border | flex;
  });
}

// ── Theme tab ────────────────────────────────────────────────────────────────
Component ThemeTab(std::shared_ptr<AppState> state) {
  return Renderer([state] {
    (void)state;
    return vbox({
               text("Theme Picker") | bold | color(Color::Magenta),
               separator(),
               text("Say \"apply dark theme\" or \"apply blue theme\".") |
                   color(Color::GrayLight),
               text("Uses ThemeFromDescription under the hood.") |
                   color(Color::GrayDark),
           }) |
           border | flex;
  });
}

// ── Help overlay ─────────────────────────────────────────────────────────────
Element HelpOverlay() {
  return vbox({
             text("Voice Commands") | bold | color(Color::Cyan),
             separator(),
             text("\"increment counter\"") | color(Color::Yellow),
             text("\"add todo <item>\"") | color(Color::Yellow),
             text("\"apply <name> theme\"") | color(Color::Yellow),
             text("\"go to counter / todo / theme\"") | color(Color::Yellow),
             text("\"help\" — toggle this overlay") | color(Color::Yellow),
             separator(),
             text("Keys:") | bold,
             text("  V   — start/stop voice recording") |
                 color(Color::GrayLight),
             text("  K   — keyboard simulation mode") | color(Color::GrayLight),
             text("  Q   — quit") | color(Color::GrayLight),
         }) |
         border | color(Color::GrayLight);
}

// ═════════════════════════════════════════════════════════════════════════════

int main() {
  auto state = std::make_shared<AppState>();
  state->todos.push_back("buy milk");

  // ── Engine setup ───────────────────────────────────────────────────────────
  auto engine = std::make_shared<VoiceCommandEngine>();

  engine->OnTranscription(
      [state](const std::string& text) { state->AddLog("🎙 " + text); });

  engine->OnCommand("increment",
                    [state](const std::string&) { ++state->counter; });
  engine->OnCommand("add", [state](const std::string& entity) {
    if (!entity.empty()) {
      state->todos.push_back(entity);
    }
  });
  engine->OnCommand("theme", [](const std::string& entity) {
    if (!entity.empty()) {
      SetTheme(ThemeFromDescription(entity));
    }
  });
  engine->OnCommand("go", [state](const std::string& entity) {
    std::string lower;
    for (char c : entity) {
      lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (lower.find("counter") != std::string::npos) {
      state->tab_index = 0;
    } else if (lower.find("todo") != std::string::npos) {
      state->tab_index = 1;
    } else if (lower.find("theme") != std::string::npos) {
      state->tab_index = 2;
    }
  });
  engine->OnCommand("help", [state](const std::string&) {
    state->show_help = !state->show_help;
  });
  engine->OnCommand("unknown", [state](const std::string& text) {
    state->AddLog("? unrecognised: " + text);
  });

  // ── Whisper availability ───────────────────────────────────────────────────
  bool whisper_ok = WhisperTranscriber::WhisperAvailable();
  std::string whisper_status = whisper_ok
                                   ? "whisper: available ✓"
                                   : "whisper: not found (press K to simulate)";

  // ── Tabs ───────────────────────────────────────────────────────────────────
  std::vector<std::string> tab_labels = {"Counter", "Todo", "Theme"};
  Component tabs = Container::Tab(
      {
          CounterTab(state),
          TodoTab(state),
          ThemeTab(state),
      },
      &state->tab_index);

  // ── Voice bar ─────────────────────────────────────────────────────────────
  Component voice_bar = VoiceInputBar(engine);

  // ── Simulation input ──────────────────────────────────────────────────────
  Component sim_input_component =
      Input(&state->sim_input, "type voice command…");

  // ── Main layout ───────────────────────────────────────────────────────────
  Component tab_switcher = Container::Horizontal({});
  // (rendered manually below)

  Component layout = Container::Vertical({
      tabs,
      voice_bar,
      sim_input_component,
  });

  Component renderer =
      Renderer(layout, [&, state, whisper_ok, whisper_status]() -> Element {
        // Tab header
        Elements tab_elems;
        for (int i = 0; i < static_cast<int>(tab_labels.size()); ++i) {
          std::string label = " " + tab_labels[i] + " ";
          Element e = text(label);
          if (i == state->tab_index) {
            e = e | bold | color(Color::Cyan) | inverted;
          } else {
            e = e | color(Color::GrayLight);
          }
          tab_elems.push_back(e);
          if (i + 1 < static_cast<int>(tab_labels.size())) {
            tab_elems.push_back(text("|") | color(Color::GrayDark));
          }
        }

        Element tab_header = hbox(std::move(tab_elems)) | border;

        // Transcription log
        Elements log_elems;
        log_elems.push_back(text("Recent transcriptions:") | bold |
                            color(Color::GrayLight));
        for (auto& entry : state->log) {
          log_elems.push_back(text("  " + entry) | color(Color::Cyan));
        }
        if (state->log.empty()) {
          log_elems.push_back(text("  (none yet)") | color(Color::GrayDark));
        }
        Element log_panel = vbox(std::move(log_elems)) | border;

        // Status bar
        Color ws_color = whisper_ok ? Color::Green : Color::Yellow;
        Element status_bar = hbox({
                                 text(" " + whisper_status) | color(ws_color),
                                 text("  |  ") | color(Color::GrayDark),
                                 text("V=voice  K=simulate  H=help  Q=quit") |
                                     color(Color::GrayDark),
                                 text(" "),
                             }) |
                             border;

        Element main_content = vbox({
            tab_header,
            tabs->Render() | flex,
            log_panel,
            voice_bar->Render(),
            hbox({
                text(" ⌨  ") | color(Color::GrayLight),
                sim_input_component->Render() | flex,
            }) | border,
            status_bar,
        });

        if (state->show_help) {
          return dbox({
              main_content,
              HelpOverlay() | center,
          });
        }
        return main_content;
      });

  // ── Event handling ────────────────────────────────────────────────────────
  Component app = CatchEvent(renderer, [&, state, engine](Event event) -> bool {
    if (event == Event::Character('q') || event == Event::Character('Q')) {
      engine->StopListening();
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    if (event == Event::Character('h') || event == Event::Character('H')) {
      state->show_help = !state->show_help;
      return true;
    }
    if (event == Event::Character('v') || event == Event::Character('V')) {
      if (engine->IsListening()) {
        engine->StopListening();
      } else {
        engine->StartListening();
      }
      return true;
    }
    // K = keyboard simulation: take current sim_input as "voice command"
    if (event == Event::Character('k') || event == Event::Character('K')) {
      if (!state->sim_input.empty()) {
        std::string cmd = state->sim_input;
        state->sim_input.clear();
        state->AddLog("⌨  " + cmd);
        // Dispatch directly through the engine's handlers
        // by routing through a fake transcription
        engine->OnTranscription([state, cmd](const std::string&) {});  // reset
        // Re-use OnTranscription for echo only; dispatch manually
        state->AddLog("→ dispatching: " + cmd);
        // Trigger via ListenOnce simulation: use a temp engine command
        // For simplicity, do keyword dispatch inline here:
        std::string lower;
        for (char c : cmd) {
          lower +=
              static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (lower.find("increment") != std::string::npos ||
            lower.find("add one") != std::string::npos) {
          ++state->counter;
        } else if (lower.find("add todo") != std::string::npos) {
          size_t pos = lower.find("add todo");
          std::string item = cmd.substr(pos + 8);
          while (!item.empty() && item.front() == ' ') {
            item = item.substr(1);
          }
          if (!item.empty()) {
            state->todos.push_back(item);
          }
        } else if (lower.find("theme") != std::string::npos) {
          SetTheme(ThemeFromDescription(cmd));
        } else if (lower.find("go to") != std::string::npos) {
          if (lower.find("counter") != std::string::npos) {
            state->tab_index = 0;
          } else if (lower.find("todo") != std::string::npos) {
            state->tab_index = 1;
          } else if (lower.find("theme") != std::string::npos) {
            state->tab_index = 2;
          }
        } else if (lower.find("help") != std::string::npos) {
          state->show_help = !state->show_help;
        }
      }
      return true;
    }
    return false;
  });

  App::Fullscreen().Loop(app);
  engine->StopListening();
  return 0;
}
