// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file config_demo.cpp
/// Demonstrates ui::ConfigEditor — persistent key-value settings editor.

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Default());

  // Build the ConfigEditor. Values are persisted to "config_demo.cfg".
  auto cfg = std::make_shared<ConfigEditor>();
  *cfg = ConfigEditor()
      .File("config_demo.cfg")
      .Title("Application Settings")
      .String("server_host",   "localhost",    "Hostname or IP address")
      .Int("server_port",      8080,           "Port number (1-65535)")
      .Bool("enable_tls",      false,          "Use TLS encryption")
      .Bool("debug_mode",      false,          "Enable debug logging")
      .Float("timeout_sec",    30.0f,          "Request timeout in seconds")
      .Section("Appearance")
      .String("theme",         "Default",      "Theme name (Default/Dark/Nord)")
      .String("font_family",   "monospace",    "Terminal font family")
      .Int("font_size",        12,             "Font size in points");

  auto editor = cfg->Build();

  // Preview panel: reads live values from ConfigEditor
  auto preview = Renderer([&cfg]() -> Element {
    const Theme& t = GetTheme();

    auto row = [&](const std::string& k, const std::string& v) -> Element {
      return hbox({
          text(" " + k) | color(t.text_muted) | size(WIDTH, EQUAL, 16),
          text(" = ") | dim,
          text(v) | bold | color(t.primary),
      });
    };

    return vbox({
        text(" Preview") | bold | color(t.primary),
        separator(),
        row("server_host",  cfg->GetString("server_host")),
        row("server_port",  std::to_string(cfg->GetInt("server_port"))),
        row("enable_tls",   cfg->GetBool("enable_tls") ? "true" : "false"),
        row("debug_mode",   cfg->GetBool("debug_mode") ? "true" : "false"),
        row("timeout_sec",  std::to_string(cfg->GetFloat("timeout_sec"))),
        separator(),
        row("theme",        cfg->GetString("theme")),
        row("font_family",  cfg->GetString("font_family")),
        row("font_size",    std::to_string(cfg->GetInt("font_size"))),
        filler(),
        separator(),
        text(" Values update live as you type") |
            color(t.text_muted) | dim,
    });
  });

  auto layout = Container::Horizontal({editor, preview});

  auto root = Renderer(layout, [&]() -> Element {
    return hbox({
        editor->Render() | flex | border,
        preview->Render() | size(WIDTH, EQUAL, 36) | border,
    });
  });

  App::TerminalOutput().Loop(root);
  return 0;
}
