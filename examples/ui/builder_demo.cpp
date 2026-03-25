// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @brief Demo: Visual UI Builder
///
/// Launches the interactive UI builder TUI directly.
///   Tab   – cycle focus
///   Enter – add palette item to layout
///   Del   – remove last element
///   's'   – export generated C++ to builder_output.cpp
///   'q'   – quit

#include "ftxui/ui.hpp"
#include "ftxui/ui/ui_builder.hpp"

int main() {
  ftxui::ui::RunFullscreen(ftxui::ui::UIBuilder());
  return 0;
}
