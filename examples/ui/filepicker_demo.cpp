// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file filepicker_demo.cpp
/// Demonstrates ui::FilePicker — left panel file browser, right panel info.

#include <filesystem>
#include <sstream>
#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

namespace fs = std::filesystem;

int main() {
  SetTheme(Theme::Default());

  fs::path selected_path;
  std::string info_text = "No file selected";

  auto picker = FilePicker()
                    .StartDir(".")
                    .ShowHidden(false)
                    .OnSelect([&](const fs::path& p) {
                      selected_path = p;
                      std::error_code ec;
                      if (fs::is_directory(p, ec)) {
                        info_text = "Directory: " + p.string();
                      } else {
                        auto size = fs::file_size(p, ec);
                        info_text =
                            "File:      " + p.filename().string() + "\n" +
                            "Path:      " + p.string() + "\n" +
                            "Extension: " + p.extension().string() + "\n" +
                            "Size:      " + (ec ? "?" : std::to_string(size)) +
                            " bytes";
                      }
                    })
                    .Build();

  auto info_panel = Renderer([&]() -> Element {
    const Theme& t = GetTheme();
    Elements lines;
    lines.push_back(text(" File Info") | bold | color(t.primary));
    lines.push_back(separator());
    if (selected_path.empty()) {
      lines.push_back(text(" No file selected") | color(t.text_muted));
    } else {
      std::istringstream ss(info_text);
      std::string line;
      while (std::getline(ss, line)) {
        lines.push_back(text(" " + line));
      }
    }
    lines.push_back(filler());
    lines.push_back(separator());
    lines.push_back(
        text(" ↑↓=navigate  Enter=select  Backspace=up  h=toggle hidden") |
        color(t.text_muted) | dim);
    return vbox(std::move(lines));
  });

  auto layout = Container::Horizontal({picker, info_panel});

  auto root = Renderer(layout, [&]() -> Element {
    return hbox({
        picker->Render() | flex | border,
        info_panel->Render() | size(WIDTH, EQUAL, 36) | border,
    });
  });

  App::TerminalOutput().Loop(root);
  return 0;
}
