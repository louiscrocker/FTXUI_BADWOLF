// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
//
// plugin_demo.cpp — demonstrates the BadWolf plugin system.
//
// Two InProcessPlugins are registered (no .so needed), then a split layout
// shows the PluginManager on the left and a live component preview on the
// right with a JSON params editor.

#include <chrono>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/loop.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/plugin.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Sample clock widget
// ───────────────────────────────────────────────────────

static ftxui::Component MakeClockWidget() {
  return ftxui::Renderer([] {
    return ftxui::vbox({
               ftxui::text("🕐 Clock Widget") | ftxui::bold |
                   ftxui::color(ftxui::Color::Cyan),
               ftxui::separator(),
               ftxui::text("  Loaded via InProcessPlugin"),
               ftxui::text("  No .so file required!") |
                   ftxui::color(ftxui::Color::GreenLight),
           }) |
           ftxui::border;
  });
}

// ── Sample system info widget
// ─────────────────────────────────────────────────

static ftxui::Component MakeSysInfoWidget(const std::string& params) {
  return ftxui::Renderer([params] {
    return ftxui::vbox({
               ftxui::text("💻 System Info Widget") | ftxui::bold |
                   ftxui::color(ftxui::Color::Magenta),
               ftxui::separator(),
               ftxui::text("  params: " + params) |
                   ftxui::color(ftxui::Color::YellowLight),
               ftxui::text("  Platform: macOS/Linux"),
               ftxui::text("  Plugin API: BadWolf v1.0"),
           }) |
           ftxui::border;
  });
}

// ─────────────────────────────────────────────────────────────────────────────

int main() {
  // Register two in-process plugins.
  InProcessPlugin clock_plugin(
      PluginInfo{"clock-plugin",
                 "1.0.0",
                 "Clock widget plugin",
                 "BadWolf",
                 {"ClockWidget"}},
      [](const std::string& name, const std::string& /*params*/) {
        if (name == "ClockWidget") {
          return MakeClockWidget();
        }
        return ftxui::Component{nullptr};
      });

  InProcessPlugin sysinfo_plugin(
      PluginInfo{"sysinfo-plugin",
                 "1.0.0",
                 "System info plugin",
                 "BadWolf",
                 {"SysInfoWidget"}},
      [](const std::string& name, const std::string& params) {
        if (name == "SysInfoWidget") {
          return MakeSysInfoWidget(params);
        }
        return ftxui::Component{nullptr};
      });

  clock_plugin.Register();
  sysinfo_plugin.Register();

  // UI state
  std::string selected_component = "ClockWidget";
  std::string params_json = "{}";
  ftxui::Component preview_component = MakeClockWidget();

  std::vector<std::string> component_names = {"ClockWidget", "SysInfoWidget"};
  int selected_index = 0;

  auto params_input = ftxui::Input(&params_json, "JSON params...");

  auto create_btn = ftxui::Button("Create Component", [&] {
    auto& reg = PluginRegistry::Instance();
    auto comp = reg.Create(component_names[selected_index], params_json);
    if (comp) {
      preview_component = comp;
    }
  });

  auto component_menu = ftxui::Menu(&component_names, &selected_index);

  // Left panel: Plugin Manager
  auto plugin_mgr = PluginManager();

  // Right panel controls
  auto right_controls =
      ftxui::Container::Vertical({component_menu, params_input, create_btn});

  auto right_panel = ftxui::Renderer(right_controls, [&] {
    return ftxui::vbox({
        ftxui::text("Component Selector") | ftxui::bold | ftxui::border,
        component_menu->Render() | ftxui::border,
        ftxui::hbox(
            {ftxui::text("Params: "), params_input->Render() | ftxui::flex}) |
            ftxui::border,
        create_btn->Render(),
        ftxui::separator(),
        ftxui::text("Preview:") | ftxui::bold,
        (preview_component ? preview_component->Render()
                           : ftxui::text("(none)") | ftxui::dim),
    });
  });

  // Split layout: left = plugin manager, right = component creator
  auto split = ftxui::ResizableSplitLeft(plugin_mgr, right_panel, nullptr);

  auto screen = ftxui::ScreenInteractive::Fullscreen();

  // Quit on 'q'
  auto root = ftxui::CatchEvent(split, [&](ftxui::Event event) {
    if (event == ftxui::Event::Character('q')) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });

  screen.Loop(root);

  clock_plugin.Unregister();
  sysinfo_plugin.Unregister();

  return 0;
}
