// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file new_features_demo.cpp
/// Demonstrates the 5 new ui:: features:
///   TextInput · Grid · Theme persistence · WithModal · WithDrawer

#include <string>
#include <string_view>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Nord());

  // ── TextInput ────────────────────────────────────────────────────────────
  std::string name_val, email_val, age_val;

  auto txt_name = TextInput("Name")
                      .Bind(&name_val)
                      .Placeholder("Your full name")
                      .MaxLength(40)
                      .Build();

  auto txt_email =
      TextInput("Email")
          .Bind(&email_val)
          .Placeholder("you@example.com")
          .MaxLength(64)
          .Validate(
              [](std::string_view s) {
                return s.empty() || s.find('@') != std::string_view::npos;
              },
              "Must contain @")
          .Build();

  auto txt_age = TextInput("Age")
                     .Bind(&age_val)
                     .Placeholder("18-120")
                     .MaxLength(3)
                     .Validate(
                         [](std::string_view s) {
                           if (s.empty()) {
                             return true;
                           }
                           for (char c : s) {
                             if (!std::isdigit(c)) {
                               return false;
                             }
                           }
                           int v = std::stoi(std::string(s));
                           return v >= 1 && v <= 120;
                         },
                         "Must be 1-120")
                     .Build();

  // ── Grid ────────────────────────────────────────────────────────────────
  auto btn1 = Button(" Dashboard  ", [] {});
  auto btn2 = Button(" Analytics  ", [] {});
  auto btn3 = Button(" Settings   ", [] {});
  auto btn4 = Button(" Profile    ", [] {});
  auto btn5 = Button(" Billing    ", [] {});
  auto btn6 = Button(" Help       ", [] {});

  auto grid = Grid(3)
                  .Gap(1)
                  .RowGap(1)
                  .CellComponent(btn1)
                  .CellComponent(btn2)
                  .CellComponent(btn3)
                  .CellComponent(btn4)
                  .CellComponent(btn5)
                  .CellComponent(btn6)
                  .Build();

  // ── Modal ────────────────────────────────────────────────────────────────
  bool show_modal = false;
  std::string modal_result;

  auto modal_body = Renderer([&]() -> Element {
    return vbox({
        separatorEmpty(),
        paragraph("Are you sure you want to delete this item? "
                  "This action cannot be undone.") |
            xflex,
        separatorEmpty(),
    });
  });

  auto btn_open_modal = Button(" Open Modal ", [&] { show_modal = true; });

  // ── Drawer ────────────────────────────────────────────────────────────────
  bool show_drawer = false;
  auto drawer_content = Renderer([&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        hbox({text("  Name:  ") | dim,
              text(name_val.empty() ? "(empty)" : name_val) | bold}),
        hbox({text("  Email: ") | dim,
              text(email_val.empty() ? "(empty)" : email_val) | bold}),
        hbox({text("  Age:   ") | dim,
              text(age_val.empty() ? "(empty)" : age_val) | bold}),
        separatorEmpty(),
        text("  (from TextInput above)") | dim | color(t.text_muted),
    });
  });
  auto btn_open_drawer =
      Button(" Open Drawer", [&] { show_drawer = !show_drawer; });

  // ── Theme persistence ────────────────────────────────────────────────────
  std::string save_status;
  auto btn_save = Button(" Save Theme ", [&] {
    if (SaveTheme("/tmp/ftxui_theme.cfg")) {
      save_status = "Saved to /tmp/ftxui_theme.cfg";
    } else {
      save_status = "Save failed!";
    }
  });
  auto btn_load = Button(" Load Theme ", [&] {
    if (LoadTheme("/tmp/ftxui_theme.cfg")) {
      save_status = "Theme loaded!";
    } else {
      save_status = "Load failed (save first)";
    }
  });

  // ── Layout ────────────────────────────────────────────────────────────────
  auto form_inputs = Container::Vertical({txt_name, txt_email, txt_age});
  auto grid_comp = Container::Vertical({grid});
  auto btns_comp = Container::Horizontal(
      {btn_open_modal, btn_open_drawer, btn_save, btn_load});
  auto root_layout =
      Container::Vertical({form_inputs, grid_comp, btns_comp, modal_body});

  auto body = Renderer(root_layout, [&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               text("  ftxui::ui — New Features Demo  ") | bold | hcenter |
                   color(t.primary),
               separatorLight(),
               separatorEmpty(),

               // TextInput section
               text(" TextInput (label · validation · maxlength)") | bold |
                   color(t.secondary),
               separatorEmpty(),
               txt_name->Render(),
               txt_email->Render(),
               txt_age->Render(),
               separatorEmpty(),

               // Grid section
               separator(),
               separatorEmpty(),
               text(" Grid(3) — 3-column button grid") | bold |
                   color(t.secondary),
               separatorEmpty(),
               grid->Render(),
               separatorEmpty(),

               // Modal + Drawer + Theme persist
               separator(),
               separatorEmpty(),
               text(" Modal · Drawer · Theme Persistence") | bold |
                   color(t.secondary),
               separatorEmpty(),
               hbox({
                   btn_open_modal->Render(),
                   text("  "),
                   btn_open_drawer->Render(),
                   text("  "),
                   btn_save->Render(),
                   text("  "),
                   btn_load->Render(),
               }) | hcenter,
               separatorEmpty(),
               (modal_result.empty()
                    ? text("")
                    : text("  " + modal_result) | color(t.accent)),
               (save_status.empty()
                    ? text("")
                    : text("  " + save_status) | color(t.text_muted)),
               separatorEmpty(),
               separator(),
               separatorEmpty(),
               text("  q=quit") | dim | hcenter,
           }) |
           flex;
  });

  // Wrap with drawer
  auto with_drawer = WithDrawer(body, DrawerSide::Right, " Profile Preview ",
                                drawer_content, &show_drawer, 35);

  // Wrap with modal
  auto with_modal = WithModal(with_drawer, "Confirm Delete", modal_body,
                              {{"Cancel",
                                [&] {
                                  show_modal = false;
                                  modal_result = "Cancelled";
                                },
                                false},
                               {"Delete",
                                [&] {
                                  show_modal = false;
                                  modal_result = "Deleted!";
                                },
                                true}},
                              &show_modal);

  // Keymap
  with_modal |=
      Keymap()
          .Bind(
              "q",
              [] {
                if (App* a = App::Active()) {
                  a->Exit();
                }
              },
              "Quit")
          .Bind(
              "d", [&] { show_drawer = !show_drawer; }, "Toggle drawer")
          .AsDecorator();

  App::Fullscreen().Loop(with_modal);
  return 0;
}
