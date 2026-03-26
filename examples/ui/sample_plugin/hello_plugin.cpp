// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
//
// hello_plugin.cpp — sample BadWolf plugin.
//
// Build as a shared library (.so / .dylib) and load at runtime via:
//   PluginRegistry::Instance().Load("path/to/libhello_plugin.dylib");
//
// Uses the BADWOLF_PLUGIN_DEFINE / BADWOLF_PLUGIN_FACTORY /
// BADWOLF_PLUGIN_DESTROY macros to export the required C symbols.

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/plugin.hpp"

// Declare plugin metadata and version (exports badwolf_plugin_info +
// badwolf_plugin_version as extern "C" symbols).
BADWOLF_PLUGIN_DEFINE("hello",
                      "1.0.0",
                      "Hello World plugin",
                      "BadWolf Team",
                      (std::vector<std::string>{"HelloWorld", "Greeting"}))

// Factory: creates a component by name.
BADWOLF_PLUGIN_FACTORY {
  std::string n(name);

  if (n == "HelloWorld") {
    return new ftxui::Component(ftxui::Renderer([] {
      return ftxui::vbox({
                 ftxui::text("🐺 Hello from BadWolf Plugin!") | ftxui::bold,
                 ftxui::text("Dynamic loading works!") |
                     ftxui::color(ftxui::Color::Green),
             }) |
             ftxui::border;
    }));
  }

  if (n == "Greeting") {
    std::string p(params_json ? params_json : "{}");
    return new ftxui::Component(ftxui::Renderer([p] {
      return ftxui::vbox({
                 ftxui::text("👋 Greeting Widget") | ftxui::bold,
                 ftxui::text("  params: " + p) |
                     ftxui::color(ftxui::Color::YellowLight),
             }) |
             ftxui::border;
    }));
  }

  return nullptr;
}

// Destroy: releases the heap-allocated Component wrapper created by the
// factory.
BADWOLF_PLUGIN_DESTROY {
  delete c;
}
