// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file model_binding_demo.cpp
/// Demonstrates BindModel / MakeLens:
///
///  - UserProfile struct with multiple fields
///  - Form with BindField, BindPassword, BindCheckbox
///  - Live preview panel showing current model state
///  - Submit/log to a LogPanel
///  - Press 'q' or Escape to quit.

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

struct UserProfile {
  std::string name;
  std::string email;
  std::string bio;
  bool        active = true;
};

int main() {
  SetTheme(Theme::Nord());
  const Theme& t = GetTheme();

  auto model = MakeBind<UserProfile>(UserProfile{"Alice", "alice@example.com",
                                                  "Software engineer.", true});

  auto log = LogPanel::Create(20);

  // Submit button action (created outside render lambda).
  auto btn_submit = Button("Submit", [&] {
    auto d = model.Get();
    log->Log("[submit] name=" + d.name + " email=" + d.email +
             " active=" + (d.active ? "yes" : "no"));
  });

  auto btn_reset = Button("Reset", [&] {
    model.Set(UserProfile{});
    log->Log("[reset]");
  });

  auto form = Form()
                  .Title("User Profile")
                  .BindField("Name",  model, &UserProfile::name)
                  .BindField("Email", model, &UserProfile::email)
                  .BindField("Bio",   model, &UserProfile::bio)
                  .BindCheckbox("Active", model, &UserProfile::active)
                  .Build();

  auto buttons = Container::Horizontal({btn_submit, btn_reset});
  auto log_comp = log->Build();

  auto left = Container::Vertical({form, buttons});
  auto root = Container::Horizontal({left, log_comp});

  root |= CatchEvent([&](Event event) -> bool {
    if (event == Event::Character('q') || event == Event::Escape) {
      if (App* app = App::Active()) app->Exit();
      return true;
    }
    return false;
  });

  auto ui = Renderer(root, [&]() -> Element {
    auto d = model.Get();

    // Live preview.
    Element preview = vbox({
        text(" Live Preview ") | bold | color(t.primary) | hcenter,
        separatorLight(),
        hbox({text("  Name   : ") | dim, text(d.name)   | color(t.accent)}),
        hbox({text("  Email  : ") | dim, text(d.email)  | color(t.accent)}),
        hbox({text("  Bio    : ") | dim, text(d.bio)    | color(t.text_muted)}),
        hbox({text("  Active : ") | dim,
              text(d.active ? "yes" : "no") |
                  color(d.active ? t.success_color : t.error_color)}),
    }) | border | size(WIDTH, EQUAL, 38);

    return vbox({
               text("  BindModel Demo  ") | bold | color(t.primary) | hcenter,
               separatorLight(),
               hbox({
                   vbox({
                       form->Render() | flex,
                       separatorLight(),
                       hbox({btn_submit->Render(), text(" "),
                             btn_reset->Render()}) | hcenter,
                   }) | size(WIDTH, EQUAL, 36),
                   separator(),
                   vbox({
                       preview,
                       separatorLight(),
                       text(" Log ") | bold | color(t.secondary) | hcenter,
                       log_comp->Render() | flex,
                   }) | flex,
               }) | flex,
               separatorLight(),
               text(" q/Esc quit ") | color(t.text_muted) | hcenter,
           }) |
           border;
  });

  RunFullscreen(ui);
  return 0;
}
