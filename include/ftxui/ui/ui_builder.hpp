// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_UI_BUILDER_HPP
#define FTXUI_UI_UI_BUILDER_HPP

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Launch the interactive visual UI builder TUI.
///
/// The builder has three panels:
///   Left:   Component palette (Text, HBox, VBox, Border, Button, …)
///   Center: Preview of the current layout tree
///   Right:  Generated C++ code for the current tree
///
/// Controls:
///   Tab       – cycle focus between panels
///   Enter     – add selected palette item to the layout tree
///   Del       – remove last element from the tree
///   's'       – export generated C++ to "builder_output.cpp"
///   'q'/Esc   – exit the builder (calls App::Active()->Exit())
///   Ctrl+C    – copy generated C++ code to stdout
///
/// @return A Component that renders and drives the builder UI.
/// @ingroup ui
ftxui::Component UIBuilder();

}  // namespace ftxui::ui

#endif  // FTXUI_UI_UI_BUILDER_HPP
