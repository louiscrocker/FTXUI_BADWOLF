// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file wizard_demo.cpp
/// Demonstrates ui::Wizard — multi-step guided flow.

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Nord());

  // Step 1 — Welcome
  auto step1 = Renderer([]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        separatorEmpty(),
        text("Welcome to FTXUI Project Setup!") | bold | hcenter | color(t.primary),
        separatorEmpty(),
        paragraph(
            "This wizard will guide you through creating a new FTXUI "
            "project. You will choose a name, configure options, and "
            "review your selections before finishing.") | xflex,
        separatorEmpty(),
    });
  });

  // Step 2 — Project name
  std::string proj_name;
  auto input_name  = Input(&proj_name, "my-app");
  auto step2_inner = Container::Vertical({input_name});
  auto step2       = Renderer(step2_inner, [&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        separatorEmpty(),
        text("Project Name") | bold | color(t.primary),
        separatorEmpty(),
        hbox({ text("  Name: ") | dim, input_name->Render() | flex }),
        separatorEmpty(),
        text("  Enter a name for your new project.") | dim,
    });
  });

  // Step 3 — Options
  std::vector<std::string> tgt_opts = {"Library", "Executable", "Both"};
  int tgt_sel = 0;
  bool add_tests = true;
  auto radio_target = Radiobox(&tgt_opts, &tgt_sel);
  auto chk_tests    = Checkbox("Include unit tests", &add_tests);
  auto step3_inner  = Container::Vertical({radio_target, chk_tests});
  auto step3        = Renderer(step3_inner, [&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        separatorEmpty(),
        text("Build Options") | bold | color(t.primary),
        separatorEmpty(),
        text("  Target type:") | dim,
        radio_target->Render(),
        separatorEmpty(),
        chk_tests->Render(),
    });
  });

  // Step 4 — Review
  auto step4 = Renderer([&]() -> Element {
    const Theme& t = GetTheme();
    std::string name_val = proj_name.empty() ? "(unnamed)" : proj_name;
    return vbox({
        separatorEmpty(),
        text("Review Your Configuration") | bold | color(t.primary),
        separatorEmpty(),
        hbox({ text("  Name:   ") | dim, text(name_val) | bold }),
        hbox({ text("  Target: ") | dim, text(tgt_opts[tgt_sel]) | bold }),
        hbox({ text("  Tests:  ") | dim, text(add_tests ? "Yes" : "No") | bold }),
        separatorEmpty(),
        text("  Click Finish to create the project.") | color(t.accent),
    });
  });

  std::string result_msg;
  auto comp = Wizard("  New Project Setup  ")
                  .Step("Welcome",  step1)
                  .Step("Name",     step2)
                  .Step("Options",  step3)
                  .Step("Review",   step4)
                  .OnComplete([&]{
                      result_msg = "Created: " +
                          (proj_name.empty() ? "my-app" : proj_name);
                      if (App* a = App::Active()) a->Exit();
                  })
                  .OnCancel([]{
                      if (App* a = App::Active()) a->Exit();
                  })
                  .Build();

  App::Fullscreen().Loop(comp);

  if (!result_msg.empty()) {
    printf("\n  ✓ %s\n\n", result_msg.c_str());
  }
  return 0;
}
