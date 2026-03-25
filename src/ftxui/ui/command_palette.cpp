// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/command_palette.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Helpers
// ───────────────────────────────────────────────────────────────────

namespace {

std::string ToLower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return out;
}

// Parse a simplified key string into an Event.
// Handles "Ctrl+X" (a-z, case-insensitive) and single printable characters.
Event ParseBindKey(std::string_view key) {
  std::string lower(key);
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (lower.size() == 6 && lower.substr(0, 5) == "ctrl+") {
    switch (lower[5]) {
      case 'a':
        return Event::CtrlA;
      case 'b':
        return Event::CtrlB;
      case 'c':
        return Event::CtrlC;
      case 'd':
        return Event::CtrlD;
      case 'e':
        return Event::CtrlE;
      case 'f':
        return Event::CtrlF;
      case 'g':
        return Event::CtrlG;
      case 'h':
        return Event::CtrlH;
      case 'i':
        return Event::CtrlI;
      case 'j':
        return Event::CtrlJ;
      case 'k':
        return Event::CtrlK;
      case 'l':
        return Event::CtrlL;
      case 'm':
        return Event::CtrlM;
      case 'n':
        return Event::CtrlN;
      case 'o':
        return Event::CtrlO;
      case 'p':
        return Event::CtrlP;
      case 'q':
        return Event::CtrlQ;
      case 'r':
        return Event::CtrlR;
      case 's':
        return Event::CtrlS;
      case 't':
        return Event::CtrlT;
      case 'u':
        return Event::CtrlU;
      case 'v':
        return Event::CtrlV;
      case 'w':
        return Event::CtrlW;
      case 'x':
        return Event::CtrlX;
      case 'y':
        return Event::CtrlY;
      case 'z':
        return Event::CtrlZ;
      default:
        break;
    }
  }

  if (key.size() == 1) {
    return Event::Character(std::string(key));
  }

  return Event::Custom;  // Unrecognized; will never match.
}

}  // namespace

// ── CommandPalette
// ────────────────────────────────────────────────────────────

CommandPalette& CommandPalette::Add(std::string_view name,
                                    std::function<void()> action,
                                    std::string_view description,
                                    std::string_view category) {
  commands_.push_back(Command{
      .name = std::string(name),
      .description = std::string(description),
      .category = std::string(category),
      .action = std::move(action),
  });
  return *this;
}

CommandPalette& CommandPalette::Bind(std::string_view key) {
  bind_key_ = std::string(key);
  return *this;
}

ftxui::Component CommandPalette::Wrap(ftxui::Component parent) const {
  auto commands = commands_;  // Value copy so the component is self-contained.

  struct PaletteState {
    bool show = false;
    std::string search;
    int selected = 0;
  };
  auto state = std::make_shared<PaletteState>();

  // Compute up to 8 filtered commands from the current search string.
  auto compute_filtered = [commands, state]() -> std::vector<const Command*> {
    std::string lower_search = ToLower(state->search);
    std::vector<const Command*> result;
    for (const auto& cmd : commands) {
      if (lower_search.empty() ||
          ToLower(cmd.name).find(lower_search) != std::string::npos ||
          ToLower(cmd.description).find(lower_search) != std::string::npos) {
        result.push_back(&cmd);
        if (result.size() >= 8) {
          break;
        }
      }
    }
    return result;
  };

  // Input for the search bar (StringRef keeps a stable pointer into state).
  InputOption input_opt;
  input_opt.placeholder = "Type to search...";
  auto search_input = Input(&state->search, input_opt);

  // CatchEvent wrapping the input to intercept palette navigation keys.
  auto palette_content =
      CatchEvent(search_input, [state, compute_filtered](Event e) -> bool {
        auto filtered = compute_filtered();
        int n = static_cast<int>(filtered.size());

        if (e == Event::ArrowDown) {
          if (state->selected + 1 < n) {
            state->selected++;
          }
          return true;
        }
        if (e == Event::ArrowUp) {
          if (state->selected > 0) {
            state->selected--;
          }
          return true;
        }
        if (e == Event::Return) {
          if (n > 0 && state->selected >= 0 && state->selected < n) {
            auto action =
                filtered[static_cast<size_t>(state->selected)]->action;
            state->show = false;
            state->search.clear();
            state->selected = 0;
            if (action) {
              action();
            }
          }
          return true;
        }
        if (e == Event::Escape) {
          state->show = false;
          state->search.clear();
          state->selected = 0;
          return true;
        }
        return false;
      });

  // Renderer draws the full palette overlay.
  auto palette_renderer = Renderer(
      palette_content, [state, compute_filtered, palette_content]() -> Element {
        auto filtered = compute_filtered();
        int n = static_cast<int>(filtered.size());

        // Clamp selection to current filtered range.
        if (n > 0) {
          state->selected = std::clamp(state->selected, 0, n - 1);
        } else {
          state->selected = 0;
        }

        const Theme& t = GetTheme();

        // Search row: ❯ <input>.
        Element search_row = hbox({text(" ❯ ") | bold | color(t.primary),
                                   palette_content->Render() | xflex});

        // Result rows.
        Elements result_rows;
        if (filtered.empty()) {
          result_rows.push_back(text("  No results") | color(t.text_muted));
        } else {
          std::string last_cat;
          for (int i = 0; i < n; ++i) {
            const Command* cmd = filtered[static_cast<size_t>(i)];

            // Category header when category changes.
            if (!cmd->category.empty() && cmd->category != last_cat) {
              last_cat = cmd->category;
              result_rows.push_back(text("  " + cmd->category) | bold |
                                    color(t.text_muted));
            }

            bool sel = (i == state->selected);
            Element name_el = text(cmd->name) | xflex;
            Element desc_el =
                text("  " + cmd->description) | color(t.text_muted);
            Element row = hbox({text(sel ? " ▶ " : "   "), name_el, desc_el});
            if (sel) {
              row = row | bgcolor(t.primary) | color(Color::Black);
            }
            result_rows.push_back(row);
          }
        }

        Element body = vbox({
            search_row,
            separator(),
            vbox(std::move(result_rows)) | size(HEIGHT, LESS_THAN, 16),
        });

        return window(text(" Command Palette "), body) |
               size(WIDTH, EQUAL, 54) | center;
      });

  // Attach the modal to the parent.
  parent = Modal(parent, palette_renderer, &state->show);

  // Outer CatchEvent: open the palette on the configured bind key.
  Event bind_event = ParseBindKey(bind_key_);
  parent = CatchEvent(parent, [state, bind_event](Event e) -> bool {
    if (e == bind_event) {
      state->show = !state->show;
      if (state->show) {
        state->search.clear();
        state->selected = 0;
      }
      return true;
    }
    return false;
  });

  return parent;
}

ftxui::ComponentDecorator CommandPalette::AsDecorator() const {
  return [palette = *this](Component comp) mutable -> Component {
    return palette.Wrap(std::move(comp));
  };
}

}  // namespace ftxui::ui
