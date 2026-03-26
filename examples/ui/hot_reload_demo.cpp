// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file hot_reload_demo.cpp
/// Demonstrates hot-reload dev mode.
/// Run normally:      ./hot_reload_demo
/// Run in dev mode:   ./hot_reload_demo --dev

#include <chrono>
#include <memory>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/hot_reload.hpp"
#include "ftxui/ui/reactive.hpp"

using namespace ftxui;
using namespace ftxui::ui;

namespace {

Component MakeDemo() {
  auto counter = std::make_shared<Reactive<int>>(0);

  auto btn_inc = Button(" + ", [counter] { counter->Set(counter->Get() + 1); });
  auto btn_dec = Button(" - ", [counter] { counter->Set(counter->Get() - 1); });
  auto btn_reset = Button("Reset", [counter] { counter->Set(0); });

  auto buttons = Container::Horizontal({btn_inc, btn_dec, btn_reset});

  return Renderer(buttons, [=]() -> Element {
    return vbox({
               text("🔥 Hot Reload Demo") | bold | hcenter,
               separator(),
               hbox({
                   text(" Counter: ") | bold,
                   text(std::to_string(counter->Get())) | color(Color::Cyan) |
                       bold,
               }) | hcenter,
               separator(),
               buttons->Render() | hcenter,
               separator(),
               text("Edit this file and watch it reload!") | dim | hcenter,
           }) |
           border | hcenter | vcenter;
  });
}

}  // namespace

int main(int argc, char** argv) {
  bool dev_mode = false;
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--dev") {
      dev_mode = true;
    }
  }

  if (dev_mode) {
    DevApp(argc, argv, MakeDemo, {"examples/ui/"});
  } else {
    auto screen = ScreenInteractive::Fullscreen();
    screen.Loop(MakeDemo());
  }
  return 0;
}
