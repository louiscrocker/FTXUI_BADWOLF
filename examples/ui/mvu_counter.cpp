// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file mvu_counter.cpp
/// Minimal MVU (Model-View-Update) demo — the "Hello World" of Elm/Bubbletea
/// architecture.
///
/// Pattern:
///   keyboard event → CatchEvent dispatches a typed Msg
///   → Update(model, msg) returns a new model
///   → View(model) re-renders purely from model state
///
/// Nothing inside View() creates component objects.  All DOM, all pure.

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Model ─────────────────────────────────────────────────────────────────────

struct Model {
  int count = 0;
};

// ── Messages ──────────────────────────────────────────────────────────────────

enum class Msg { Increment, Decrement, Reset, Quit };

// ── Update (pure reducer) ─────────────────────────────────────────────────────

Model Update(Model m, Msg msg) {
  switch (msg) {
    case Msg::Increment: m.count++;  break;
    case Msg::Decrement: m.count--;  break;
    case Msg::Reset:     m.count = 0; break;
    case Msg::Quit:
      if (App* app = App::Active()) app->Exit();
      break;
  }
  return m;
}

// ── View (pure DOM — no component objects constructed here) ───────────────────

Element View(const Model& m) {
  const Theme& t = GetTheme();

  return vbox({
             text("  MVU Counter Demo  ") | bold | hcenter | color(t.primary),
             separatorLight(),
             separatorEmpty(),
             text(std::to_string(m.count)) | bold | hcenter |
                 size(WIDTH, EQUAL, 10) | color(t.accent),
             separatorEmpty(),
             hbox({
                 text("  [−] decrement") | color(t.secondary),
                 filler(),
                 text("[+] increment  ") | color(t.accent),
             }),
             hbox({
                 text("  [r] reset    ") | color(t.text_muted),
                 filler(),
                 text("[q] quit       ") | color(t.error_color),
             }),
         }) |
         borderStyled(t.border_style, t.border_color) |
         size(WIDTH, EQUAL, 36) | center;
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
  SetTheme(Theme::Dracula());

  // The model lives in a shared_ptr so both the Renderer and CatchEvent
  // lambdas can access (and mutate) it without capturing by reference into
  // temporaries.
  auto model = std::make_shared<Model>();

  auto dispatch = [model](Msg msg) {
    *model = Update(*model, msg);
    if (App* app = App::Active())
      app->PostEvent(Event::Custom);  // trigger redraw
  };

  // Renderer: calls View() on every frame.
  auto comp = Renderer([model]() -> Element { return View(*model); });

  // CatchEvent: translates key presses into typed messages.
  comp |= CatchEvent([dispatch](Event event) -> bool {
    if (!event.is_character()) return false;
    const std::string& ch = event.character();
    if (ch == "+") { dispatch(Msg::Increment); return true; }
    if (ch == "-") { dispatch(Msg::Decrement); return true; }
    if (ch == "r") { dispatch(Msg::Reset);     return true; }
    if (ch == "q") { dispatch(Msg::Quit);      return true; }
    return false;
  });

  App::FitComponent().Loop(comp);
  return 0;
}
