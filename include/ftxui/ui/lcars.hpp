// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_LCARS_HPP
#define FTXUI_UI_LCARS_HPP

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

/// @brief Creates an LCARS-style panel with a characteristic left color bar
/// and title header.
///
/// @param title    Header text displayed in the top bar.
/// @param content  The element rendered in the main content area.
/// @param color    The LCARS accent color (defaults to theme primary).
/// @ingroup ui
ftxui::Element LCARSPanel(std::string title,
                          ftxui::Element content,
                          ftxui::Color color = ftxui::Color::Default);

/// @brief LCARS-style button (rounded pill shape).
/// @ingroup ui
ftxui::Component LCARSButton(std::string label,
                             std::function<void()> on_click,
                             ftxui::Color color = ftxui::Color::Default);

/// @brief LCARS status bar — horizontal series of colored label segments.
/// @ingroup ui
ftxui::Element LCARSBar(
    std::vector<std::pair<std::string, ftxui::Color>> segments);

/// @brief LCARS data readout — right-aligned label + value pair.
/// @ingroup ui
ftxui::Element LCARSReadout(std::string label,
                            std::string value,
                            ftxui::Color label_color = ftxui::Color::Default);

/// @brief Full LCARS screen layout: title bar + side column + main content.
/// @ingroup ui
ftxui::Component LCARSScreen(std::string title,
                             ftxui::Component sidebar,
                             ftxui::Component main_content);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_LCARS_HPP
