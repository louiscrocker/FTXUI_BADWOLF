// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file log_demo.cpp
/// Demonstrates ui::LogPanel — thread-safe, color-coded log viewer.

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Dark());

  auto log = std::make_shared<LogPanel>();
  log->SetMaxLines(200);
  log->SetMinLevel(LogLevel::Trace);

  // Background thread generating log entries
  std::atomic<bool> stop{false};
  std::thread worker([&] {
    int i = 0;
    while (!stop) {
      switch (i % 5) {
        case 0: log->Log("Server started on port 8080",       LogLevel::Info); break;
        case 1: log->Log("Request GET /api/health (200 OK)",  LogLevel::Debug); break;
        case 2: log->Log("Cache miss — fetching from DB",     LogLevel::Trace); break;
        case 3: log->Log("Response time 142ms",               LogLevel::Info); break;
        case 4:
          if (i % 15 == 4)
            log->Log("Connection pool exhausted — waiting",   LogLevel::Warn);
          else if (i % 20 == 9)
            log->Log("Failed to parse JSON body",             LogLevel::Error);
          else
            log->Log("Worker processed task #" + std::to_string(i), LogLevel::Debug);
          break;
      }
      ++i;
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
  });

  log->Log("Log panel demo started", LogLevel::Info);

  auto log_comp = log->Build("  Application Log  ");

  // Filter controls
  std::vector<std::string> level_names = {"Trace","Debug","Info","Warn","Error"};
  int level_sel = 0;
  auto radio_level = Radiobox(&level_names, &level_sel);
  radio_level |= CatchEvent([&](Event) {
    LogLevel levels[] = {LogLevel::Trace, LogLevel::Debug, LogLevel::Info,
                         LogLevel::Warn,  LogLevel::Error};
    log->SetMinLevel(levels[level_sel]);
    return false;
  });

  auto sidebar = Renderer(radio_level, [&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               text(" Min Level ") | bold | hcenter | color(t.secondary),
               separatorLight(),
               radio_level->Render(),
               separatorEmpty(),
               text(" q = quit ") | dim | hcenter,
           }) |
           borderStyled(t.border_style, t.border_color);
  });

  auto layout = Container::Horizontal({log_comp, radio_level});
  auto root   = Renderer(layout, [&]() -> Element {
    return hbox({
        log_comp->Render() | flex,
        sidebar->Render() | size(WIDTH, EQUAL, 16),
    }) | flex;
  });

  root |= Keymap()
    .Bind("q", [&]{
        stop = true;
        if (App* a = App::Active()) a->Exit();
    }, "Quit")
    .AsDecorator();

  App::Fullscreen().Loop(root);
  worker.join();
  return 0;
}
