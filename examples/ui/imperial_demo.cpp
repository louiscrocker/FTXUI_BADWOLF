// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file imperial_demo.cpp
/// Death Star operations center demo using the Imperial theme and alert system.

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

namespace {

struct DeathStarState {
  std::atomic<int> superlaser{100};
  std::atomic<int> shields{80};
  std::atomic<int> hull{100};
  std::atomic<int> power{95};
};

DeathStarState g_ds;

Element StatusRow(const std::string& label, const std::string& status,
                  Color status_color) {
  const Theme& t = GetTheme();
  return hbox({
      text("  " + label) | color(t.text) | size(WIDTH, EQUAL, 22),
      text("[") | color(t.border_color),
      text(status) | bold | color(status_color),
      text("]") | color(t.border_color),
  });
}

Element PowerBar(int value, Color bar_color) {
  const Theme& t = GetTheme();
  int filled = value * 18 / 100;
  std::string bar(filled, '|');
  std::string empty(18 - filled, ' ');
  std::string pct = std::to_string(value) + "%";
  return hbox({
      text("[") | color(t.border_color),
      text(bar) | color(bar_color),
      text(empty) | bgcolor(Color::RGB(30, 0, 0)),
      text("] ") | color(t.border_color),
      text(pct) | bold | color(bar_color),
  });
}

}  // namespace

int main() {
  SetTheme(Theme::Imperial());
  const Theme& t = GetTheme();

  auto content = Renderer([]() -> Element {
    const Theme& th = GetTheme();
    int superlaser = g_ds.superlaser.load();
    int shields    = g_ds.shields.load();
    int hull       = g_ds.hull.load();
    int power      = g_ds.power.load();

    Color sl_color = superlaser > 80 ? th.error_color : th.warning_color;

    return vbox({
               separatorEmpty(),
               text("DEATH STAR OPERATIONS CENTER") | bold | color(th.primary) |
                   hcenter,
               text("DS-1 ORBITAL BATTLE STATION") | color(th.text_muted) |
                   hcenter,
               separatorEmpty(),
               separator() | color(th.border_color),
               separatorEmpty(),
               text("POWER SYSTEMS") | bold | color(th.secondary),
               separatorEmpty(),
               hbox({text("  SUPERLASER:   ") | color(th.text_muted),
                     PowerBar(superlaser, sl_color)}),
               hbox({text("  MAIN SHIELDS: ") | color(th.text_muted),
                     PowerBar(shields, th.accent)}),
               hbox({text("  HULL STATUS:  ") | color(th.text_muted),
                     PowerBar(hull, th.success_color)}),
               hbox({text("  REACTOR:      ") | color(th.text_muted),
                     PowerBar(power, th.primary)}),
               separatorEmpty(),
               separator() | color(th.border_color),
               separatorEmpty(),
               text("SYSTEM STATUS") | bold | color(th.secondary),
               separatorEmpty(),
               StatusRow("Hyperdrive",     "OPERATIONAL", th.success_color),
               StatusRow("Tractor Beam",   "STANDBY",     th.text_muted),
               StatusRow("Turbolasers",    "ARMED",       th.warning_color),
               StatusRow("Ion Cannon",     "CHARGED",     th.primary),
               StatusRow("Life Support",   "NOMINAL",     th.success_color),
               StatusRow("Detention Block","SECURED",     th.success_color),
               separatorEmpty(),
               separator() | color(th.border_color),
               separatorEmpty(),
               text("Press r=Red Alert  y=Yellow  c=All Clear  q=Quit") |
                   dim | color(th.text_muted) | hcenter,
           }) |
           flex;
  });

  auto ui = Renderer(content, [content]() -> Element {
    const Theme& th = GetTheme();
    return content->Render() |
           borderStyled(HEAVY, th.border_color);
  });

  ui |= CatchEvent([](Event e) -> bool {
    if (e == Event::Character('r')) {
      RedAlert("REBEL ATTACK");
      return true;
    }
    if (e == Event::Character('y')) {
      YellowAlert("UNIDENTIFIED VESSEL");
      return true;
    }
    if (e == Event::Character('c')) {
      AllClear();
      return true;
    }
    if (e == Event::Character('q')) {
      if (App* a = App::Active()) a->Exit();
      return true;
    }
    return false;
  });

  auto root = WithAlertOverlay(ui);

  // Background thread simulates power fluctuations.
  std::atomic<bool> running{true};
  std::thread power_sim([&running]() {
    int tick = 0;
    while (running.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      g_ds.superlaser.store(80 + (tick * 7 % 20));
      g_ds.shields.store(75 + (tick * 3 % 25));
      ++tick;
      if (App* a = App::Active()) {
        a->PostEvent(Event::Custom);
      }
    }
  });

  App::Fullscreen().Loop(root);
  running.store(false);
  power_sim.join();
  return 0;
}
