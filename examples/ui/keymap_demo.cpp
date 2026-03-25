// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file keymap_demo.cpp
/// Demonstrates ftxui::ui::Keymap and the WithConfirm / WithAlert dialog
/// helpers.

#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Monokai());

  // ── State ──────────────────────────────────────────────────────────────────
  std::string last_key;
  int action_count = 0;
  bool show_confirm = false;
  bool show_alert = false;
  bool show_help = false;

  // ── Main content ───────────────────────────────────────────────────────────
  auto content = Renderer([&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               text("  Keymap + Dialog Demo  ") | bold | hcenter |
                   color(t.primary),
               separatorLight(),
               hbox({
                   text("Last key    : ") | dim,
                   text(last_key.empty() ? "(none)" : last_key) | bold |
                       color(t.accent),
               }) | hcenter,
               hbox({
                   text("Action count: ") | dim,
                   text(std::to_string(action_count)) | bold |
                       color(t.secondary),
               }) | hcenter,
               separatorEmpty(),
               vbox({
                   text("Keybindings:") | dim | hcenter,
                   separatorEmpty(),
                   text("  s  →  trigger action (count++)") |
                       color(t.text_muted),
                   text("  d  →  open confirm dialog") | color(t.text_muted),
                   text("  e  →  open alert dialog") | color(t.text_muted),
                   text("  ?  →  keyboard help overlay") | color(t.text_muted),
                   text("  q  →  quit") | color(t.text_muted),
               }),
           }) |
           borderStyled(t.border_style, t.border_color) |
           size(WIDTH, EQUAL, 46) | center;
  });

  // ── Keybindings ────────────────────────────────────────────────────────────
  content |= Keymap()
                 .Bind(
                     "s",
                     [&] {
                       last_key = "s";
                       ++action_count;
                     },
                     "Trigger action")
                 .Bind(
                     "d",
                     [&] {
                       last_key = "d";
                       show_confirm = true;
                     },
                     "Open confirm dialog")
                 .Bind(
                     "e",
                     [&] {
                       last_key = "e";
                       show_alert = true;
                     },
                     "Open alert dialog")
                 .Bind(
                     "?",
                     [&] {
                       last_key = "?";
                       show_help = true;
                     },
                     "Keybinding help")
                 .Bind(
                     "q",
                     [] {
                       if (App* a = App::Active()) {
                         a->Exit();
                       }
                     },
                     "Quit")
                 .AsDecorator();

  // ── Dialogs ────────────────────────────────────────────────────────────────
  content |= WithConfirm(
      "Really trigger the action?",
      [&] {
        ++action_count;
        show_confirm = false;
        last_key = "confirmed";
      },
      [&] {
        show_confirm = false;
        last_key = "cancelled";
      },
      &show_confirm);

  content |= WithAlert("Notice",
                       "This is an informational alert.\n"
                       "Press OK to dismiss it.",
                       &show_alert, [&] { show_alert = false; });

  const std::vector<std::pair<std::string, std::string>> bindings = {
      {"s", "Trigger action"}, {"d", "Confirm dialog"}, {"e", "Alert dialog"},
      {"?", "This help"},      {"q", "Quit"},
  };
  content |= WithHelp("Keyboard Shortcuts", bindings, &show_help,
                      [&] { show_help = false; });

  App::FitComponent().Loop(content);
  return 0;
}
