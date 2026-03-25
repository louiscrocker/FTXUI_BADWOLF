// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file process_demo.cpp
///
/// Demonstrates ui::ProcessPanel — a subprocess runner that streams
/// stdout/stderr into scrollable log panels.
///
/// Three panels are shown side by side:
///   1. ls -la /usr/local/bin        (auto-starts)
///   2. cmake --build ... --parallel 8  (manual start)
///   3. git log --oneline -30        (auto-starts)
///
/// Keys:
///   1 / 2 / 3  — select active panel
///   S          — start active panel's process
///   X          — stop  active panel's process (SIGTERM)
///   C          — clear active panel's output
///   Q          — quit

#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/process_panel.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;
using namespace ftxui::ui;

static const std::string kRepoDir = "/Users/lcrocker/Documents/GitHub/FTXUI";

int main() {
  SetTheme(Theme::Dark());

  // ── Panel 1: directory listing (auto-start) ───────────────────────────────
  auto p1 = ProcessPanel::Create();
  p1->SetCommand("ls -la /usr/local/bin");

  // ── Panel 2: cmake build (manual start) ───────────────────────────────────
  auto p2 = ProcessPanel::Create();
  p2->SetCommand("cmake --build " + kRepoDir + "/build --parallel 8");

  // ── Panel 3: git log (auto-start) ─────────────────────────────────────────
  auto p3 = ProcessPanel::Create();
  p3->SetCommand("git -C " + kRepoDir + " log --oneline -30");

  auto c1 = p1->Build("ls /usr/local/bin");
  auto c2 = p2->Build("cmake build");
  auto c3 = p3->Build("git log");

  // Auto-start panels 1 and 3.
  p1->Start();
  p3->Start();

  std::vector<std::shared_ptr<ProcessPanel>> panels = {p1, p2, p3};
  std::vector<Component> comps = {c1, c2, c3};
  int focused = 0;

  auto layout = Container::Horizontal({c1, c2, c3});

  auto root = Renderer(layout, [&]() -> Element {
    const Theme& t = GetTheme();

    Elements panel_cols;
    for (int i = 0; i < 3; ++i) {
      Element col = comps[i]->Render() | flex;
      if (i == focused) {
        col = col | color(t.primary);
      }
      panel_cols.push_back(col);
    }

    // Bottom status bar.
    std::string focused_label = "[" + std::to_string(focused + 1) + "]";
    Element status_bar = hbox({
        text(" Active: ") | dim,
        text(focused_label) | bold | color(t.primary),
        text("  1/2/3=select  S=start  X=stop  C=clear  Q=quit") | dim,
    });

    return vbox({
               hbox(std::move(panel_cols)) | flex,
               separator(),
               status_bar,
           }) |
           flex;
  });

  root |= CatchEvent([&](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      for (auto& p : panels) {
        p->Stop();
      }
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    if (e == Event::Character('1')) {
      focused = 0;
      return true;
    }
    if (e == Event::Character('2')) {
      focused = 1;
      return true;
    }
    if (e == Event::Character('3')) {
      focused = 2;
      return true;
    }
    if (e == Event::Character('s') || e == Event::Character('S')) {
      panels[focused]->Start();
      return true;
    }
    if (e == Event::Character('x') || e == Event::Character('X')) {
      panels[focused]->Stop();
      return true;
    }
    if (e == Event::Character('c') || e == Event::Character('C')) {
      panels[focused]->Clear();
      return true;
    }
    return false;
  });

  RunFullscreen(root);
  return 0;
}
