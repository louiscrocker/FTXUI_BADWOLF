// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file router_demo.cpp
/// Demonstrates ui::Router — named view navigation with history.

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// Forward-declare the router so views can navigate it.
std::shared_ptr<Router> g_router;

Component MakeDashboard();
Component MakeSettings();
Component MakeAbout();

Component MakeDashboard() {
  auto btn_settings =
      Button(" ⚙  Settings ", [&] { g_router->Push("settings"); });
  auto btn_about = Button(" ℹ  About    ", [&] { g_router->Push("about"); });
  auto layout = Container::Horizontal({btn_settings, btn_about});
  return Renderer(layout, [=]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        text("  Dashboard  ") | bold | hcenter | color(t.primary),
        separatorLight(),
        separatorEmpty(),
        hbox({
            text("    CPU  ") | dim,
            gauge(0.42f) | color(t.primary) | flex,
            text("  42% "),
        }) | xflex,
        hbox({
            text("    RAM  ") | dim,
            gauge(0.71f) | color(t.accent) | flex,
            text("  71% "),
        }) | xflex,
        hbox({
            text("    DSK  ") | dim,
            gauge(0.18f) | color(t.secondary) | flex,
            text("  18% "),
        }) | xflex,
        separatorEmpty(),
        hbox({
            btn_settings->Render() | flex,
            text("   "),
            btn_about->Render() | flex,
        }) | hcenter,
    });
  });
}

Component MakeSettings() {
  std::string theme_val = "Nord";
  auto input_theme = Input(&theme_val, "theme");
  auto btn_back = Button(" ← Back ", [&] { g_router->Pop(); });
  auto layout = Container::Vertical({input_theme, btn_back});
  return Renderer(layout, [=]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        text("  Settings  ") | bold | hcenter | color(t.secondary),
        separatorLight(),
        separatorEmpty(),
        hbox({text("  Theme: ") | dim, input_theme->Render() | flex}) | xflex,
        separatorEmpty(),
        btn_back->Render() | hcenter,
    });
  });
}

Component MakeAbout() {
  auto btn_back = Button(" ← Back ", [&] { g_router->Pop(); });
  return Renderer(btn_back, [=]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        text("  About  ") | bold | hcenter | color(t.accent),
        separatorLight(),
        separatorEmpty(),
        paragraph("FTXUI ui:: layer — a high-level API built on top of FTXUI "
                  "that makes terminal UI as easy as writing Go with Bubbletea "
                  "or tview.") |
            xflex,
        separatorEmpty(),
        text("  Version  0.1.0") | dim,
        separatorEmpty(),
        btn_back->Render() | hcenter,
    });
  });
}

int main() {
  SetTheme(Theme::Dracula());

  g_router = std::make_shared<Router>();
  g_router->Route("dashboard", MakeDashboard)
      .Route("settings", MakeSettings)
      .Route("about", MakeAbout)
      .Default("dashboard");

  auto router_comp = g_router->Build();

  auto root = Renderer(router_comp, [&]() -> Element {
    const Theme& t = GetTheme();
    std::string breadcrumb(g_router->Current());
    return vbox({
               hbox({
                   text(" Router Demo  ") | bold | color(t.primary),
                   text("▸ " + breadcrumb) | dim | flex,
                   text(" q=quit ") | dim,
               }),
               separator(),
               router_comp->Render() | flex,
           }) |
           borderStyled(t.border_style, t.border_color) | flex;
  });

  root |= Keymap()
              .Bind(
                  "q",
                  [] {
                    if (App* a = App::Active()) {
                      a->Exit();
                    }
                  },
                  "Quit")
              .AsDecorator();

  App::Fullscreen().Loop(root);
  return 0;
}
