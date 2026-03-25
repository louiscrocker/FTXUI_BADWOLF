// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file notification_demo.cpp
/// Demonstrates the Notification/Toast system.

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Dark());

  auto btn_info = Button(
      "[i] Info", [] { Notify("Build succeeded in 2.3s", Severity::Info); });
  auto btn_success = Button("[✓] Success", [] {
    Notify("File saved successfully!", Severity::Success);
  });
  auto btn_warning = Button(
      "[!] Warning", [] { Notify("Disk usage above 80%", Severity::Warning); });
  auto btn_error = Button(
      "[✗] Error", [] { Notify("Connection timed out", Severity::Error); });
  auto btn_persist = Button("[∞] Persist", [] {
    Notify("Persistent notification (dismiss with Esc)", Severity::Info,
           std::chrono::milliseconds{0});
  });

  auto layout = Container::Vertical({
      btn_info,
      btn_success,
      btn_warning,
      btn_error,
      btn_persist,
  });

  auto body = Renderer(layout, [&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               text("  Notification / Toast Demo  ") | bold | hcenter |
                   color(t.primary),
               separatorLight(),
               separatorEmpty(),
               paragraph("Click a button to fire a toast notification. "
                         "Notifications appear in the top-right corner and "
                         "auto-dismiss after 3 seconds (except Persist).") |
                   xflex,
               separatorEmpty(),
               separatorLight(),
               separatorEmpty(),
               btn_info->Render() | hcenter,
               separatorEmpty(),
               btn_success->Render() | hcenter,
               separatorEmpty(),
               btn_warning->Render() | hcenter,
               separatorEmpty(),
               btn_error->Render() | hcenter,
               separatorEmpty(),
               btn_persist->Render() | hcenter,
               separatorEmpty(),
           }) |
           borderStyled(t.border_style, t.border_color) | flex;
  });

  auto comp = WithNotifications(body);

  comp |= Keymap()
              .Bind(
                  "q",
                  [] {
                    if (App* a = App::Active()) {
                      a->Exit();
                    }
                  },
                  "Quit")
              .AsDecorator();

  App::Fullscreen().Loop(comp);
  return 0;
}
