// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file starship_demo.cpp
/// Starship operations demo using LCARS theme, LCARSScreen, LCARSReadout,
/// alert overlay, and a background sensor thread.

#include <atomic>
#include <chrono>
#include <cmath>
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

struct ShipState {
  std::atomic<int> shields{55};
  std::atomic<int> hull{98};
  std::atomic<int> warp_power{96};
  std::atomic<int> stardate_frac{4};  // last digit of fractional stardate
};

ShipState g_ship;

Element GaugeBar(int value, Color bar_color, Color bg_color = Color::Default) {
  const Theme& t = GetTheme();
  int filled = value * 20 / 100;
  std::string bar(filled, '#');
  std::string empty(20 - filled, '-');
  std::string pct = std::to_string(value) + "%";
  return hbox({
      text("[") | color(t.border_color),
      text(bar) | color(bar_color),
      text(empty) | color(t.text_muted),
      text("] ") | color(t.border_color),
      text(pct) | bold | color(bar_color),
  });
}

}  // namespace

int main() {
  SetTheme(Theme::LCARS());
  const Theme& t = GetTheme();

  // ── Sidebar ────────────────────────────────────────────────────────────────
  auto sidebar = Renderer([]() -> Element {
    const Theme& th = GetTheme();
    return vbox({
        LCARSBar({{"NAV",    Color::RGB(255, 153,   0)},
                  {"OPS",    Color::RGB(153, 153, 255)}}),
        separatorEmpty(),
        LCARSReadout("STARDATE", "47634." + std::to_string(g_ship.stardate_frac.load())),
        LCARSReadout("SECTOR",   "001-J"),
        LCARSReadout("CREW",     "1012"),
        separatorEmpty(),
        text("SYSTEMS") | bold | color(th.primary),
        separatorEmpty(),
        LCARSReadout("Warp Core",    "NOMINAL",    th.success_color),
        LCARSReadout("Life Support", "NOMINAL",    th.success_color),
        LCARSReadout("Sensors",      "ACTIVE",     th.success_color),
        LCARSReadout("Phasers",      "ARMED",      th.warning_color),
        LCARSReadout("Torpedoes",    "12 REMAIN",  th.text),
        LCARSReadout("Shields",      "RAISED",     th.warning_color),
    }) | flex;
  });

  // ── Main content ──────────────────────────────────────────────────────────
  auto main_content = Renderer([]() -> Element {
    const Theme& th = GetTheme();
    int shields = g_ship.shields.load();
    int hull    = g_ship.hull.load();
    int warp    = g_ship.warp_power.load();

    Color shields_color = shields > 70 ? th.success_color
                        : shields > 30 ? th.warning_color
                                       : th.error_color;

    return vbox({
        separatorEmpty(),
        text("USS ENTERPRISE NCC-1701-D") | bold | color(th.primary) | hcenter,
        separatorEmpty(),
        separator() | color(th.border_color),
        separatorEmpty(),
        hbox({text("WARP SPEED:  ") | color(th.text_muted),
              GaugeBar(warp, th.primary)}),
        separatorEmpty(),
        hbox({text("SHIELDS:     ") | color(th.text_muted),
              GaugeBar(shields, shields_color)}),
        separatorEmpty(),
        hbox({text("HULL INTGTY: ") | color(th.text_muted),
              GaugeBar(hull, th.success_color)}),
        separatorEmpty(),
        separator() | color(th.border_color),
        separatorEmpty(),
        text("Press r=Red Alert  y=Yellow Alert  c=All Clear  q=Quit") |
            dim | color(th.text_muted) | hcenter,
    }) | flex;
  });

  // ── Full LCARS chrome ──────────────────────────────────────────────────────
  auto screen = LCARSScreen("LCARS MAIN COMPUTER", sidebar, main_content);

  // ── Key bindings ──────────────────────────────────────────────────────────
  screen |= CatchEvent([](Event e) -> bool {
    if (e == Event::Character('r')) {
      RedAlert("KLINGON VESSEL DECLOAKING");
      return true;
    }
    if (e == Event::Character('y')) {
      YellowAlert("SENSOR CONTACT");
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

  // ── Alert overlay ──────────────────────────────────────────────────────────
  auto root = WithAlertOverlay(screen);

  // ── Background sensor thread ──────────────────────────────────────────────
  std::atomic<bool> running{true};
  std::thread sensor([&running]() {
    int tick = 0;
    while (running.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      // Simulate slowly changing sensors.
      g_ship.shields.store(50 + static_cast<int>(20 * std::sin(tick * 0.4)));
      g_ship.warp_power.store(90 + (tick % 10));
      g_ship.stardate_frac.store(tick % 10);
      ++tick;
      if (App* a = App::Active()) {
        a->PostEvent(Event::Custom);
      }
    }
  });

  App::Fullscreen().Loop(root);
  running.store(false);
  sensor.join();
  return 0;
}
