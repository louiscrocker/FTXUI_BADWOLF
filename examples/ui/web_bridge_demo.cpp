// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file web_bridge_demo.cpp
///
/// Demonstrates how to serve any BadWolf TUI app in a browser via the
/// Terminal-to-Web bridge.
///
/// Run normally:     ./web_bridge_demo
/// Run in browser:   ./web_bridge_demo --web
///                   Then open http://localhost:7681 in your browser.

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
#include "ftxui/ui/web_bridge.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Live counter tab
// ──────────────────────────────────────────────────────────

Component MakeCounterTab() {
  auto count = std::make_shared<int>(0);

  auto inc = Button("  +  ", [count] { (*count)++; });
  auto dec = Button("  -  ", [count] { (*count)--; });
  auto rst = Button(" Reset ", [count] { *count = 0; });

  return Renderer(Container::Horizontal({inc, dec, rst}), [=] {
    return vbox({
               text("Reactive Counter") | bold | color(Color::Cyan) | hcenter,
               separator(),
               text("Value: " + std::to_string(*count)) | hcenter |
                   color(Color::Yellow) | bold,
               separator(),
               hbox({
                   inc->Render() | flex,
                   text(" "),
                   dec->Render() | flex,
                   text(" "),
                   rst->Render() | flex,
               }),
           }) |
           border;
  });
}

// ── Live log tab
// ──────────────────────────────────────────────────────────────

Component MakeLogTab(std::atomic<bool>& stop) {
  auto entries = std::make_shared<std::vector<std::string>>();
  auto mtx = std::make_shared<std::mutex>();

  std::thread([entries, mtx, &stop] {
    static const char* kMsgs[] = {
        "Server started on port 7681", "WebSocket client connected",
        "Rendering frame 1/30 fps",    "Event: ArrowDown dispatched",
        "Cache hit — 0.3ms",           "Broadcast ANSI frame (4.2 KB)",
    };
    int i = 0;
    while (!stop) {
      {
        std::lock_guard<std::mutex> lk(*mtx);
        entries->push_back(kMsgs[i % 6]);
        if (entries->size() > 20) {
          entries->erase(entries->begin());
        }
      }
      ++i;
      std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }
  }).detach();

  return Renderer([entries, mtx] {
    std::lock_guard<std::mutex> lk(*mtx);
    Elements lines;
    lines.push_back(text("Live Log") | bold | color(Color::Green) | hcenter);
    lines.push_back(separator());
    for (const auto& e : *entries) {
      lines.push_back(text("  › " + e) | color(Color::GrayLight));
    }
    return vbox(std::move(lines)) | border;
  });
}

// ── Charts tab
// ────────────────────────────────────────────────────────────────

Component MakeChartsTab() {
  return Renderer([] {
    constexpr int kW = 50;
    auto c = Canvas(kW * 2, 20);
    for (int x = 0; x < kW * 2; ++x) {
      float t = static_cast<float>(x) / static_cast<float>(kW * 2);
      int y_sin = static_cast<int>(10.f - 8.f * std::sin(t * 6.28f));
      int y_cos = static_cast<int>(10.f - 8.f * std::cos(t * 6.28f));
      c.DrawPoint(x, y_sin, true, Color::Cyan);
      c.DrawPoint(x, y_cos, true, Color::Yellow);
    }
    return vbox({
               text("Charts") | bold | color(Color::Magenta) | hcenter,
               separator(),
               text(" ─ sin(x): Cyan   cos(x): Yellow") |
                   color(Color::GrayLight),
               canvas(std::move(c)),
           }) |
           border;
  });
}

// ── Root component
// ────────────────────────────────────────────────────────────

Component MakeDemo(std::atomic<bool>& stop) {
  int tab = 0;
  std::vector<std::string> tab_labels = {"Counter", "Log", "Charts"};
  auto tab_toggle = Toggle(&tab_labels, &tab);

  auto counter_tab = MakeCounterTab();
  auto log_tab = MakeLogTab(stop);
  auto charts_tab = MakeChartsTab();
  auto tabs = Container::Tab({counter_tab, log_tab, charts_tab}, &tab);

  auto quit = Button("  Quit  ", [] { App::Active()->Exit(); });

  auto layout = Container::Vertical({tab_toggle, tabs, quit});

  return Renderer(layout, [=, &tab] {
    return vbox({
               hbox({
                   text("\xf0\x9f\x90\xba BadWolf Web Bridge Demo") | bold |
                       color(Color::Magenta),
                   filler(),
                   text("Web: http://localhost:7681") | color(Color::GrayLight),
               }) | border,
               tab_toggle->Render(),
               separator(),
               tabs->Render() | flex,
               hbox({filler(), quit->Render(), filler()}),
           }) |
           flex;
  });
}

// ── main
// ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
  std::atomic<bool> stop{false};

  WebBridgeConfig cfg;
  cfg.port = 7681;
  cfg.cols = 220;
  cfg.rows = 50;
  cfg.fps = 30;
  cfg.title = "BadWolf Web Bridge Demo";

  RunWebOrTerminal(argc, argv, [&stop] { return MakeDemo(stop); }, cfg);

  stop = true;
  return 0;
}
