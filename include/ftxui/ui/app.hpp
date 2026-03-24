// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_APP_HPP
#define FTXUI_UI_APP_HPP

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Run @p component in terminal-output mode (scrolling, no alt-screen).
inline void Run(ftxui::Component component) {
  App::TerminalOutput().Loop(std::move(component));
}

/// @brief Run @p component fullscreen on the alternate screen buffer.
inline void RunFullscreen(ftxui::Component component) {
  App::Fullscreen().Loop(std::move(component));
}

/// @brief Run @p component sized to fit its rendered output.
inline void RunFitComponent(ftxui::Component component) {
  App::FitComponent().Loop(std::move(component));
}

/// @brief Run @p component in a fixed-size window.
inline void RunFixed(ftxui::Component component, int width, int height) {
  App::FixedSize(width, height).Loop(std::move(component));
}

}  // namespace ftxui::ui

#endif  // FTXUI_UI_APP_HPP
