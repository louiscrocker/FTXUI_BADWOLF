// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_PROGRESS_HPP
#define FTXUI_UI_PROGRESS_HPP

#include <functional>
#include <string>
#include <string_view>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

// ── Themed progress bar ───────────────────────────────────────────────────────

/// @brief A labeled, themed progress bar component.
///
/// @param progress  Pointer to a float in [0.0, 1.0].
/// @param label     Optional label displayed to the left of the bar.
///
/// @code
/// float progress = 0.0f;
///
/// auto bar = ui::ThemedProgressBar(&progress, "Uploading");
/// App::FitComponent().Loop(bar);
/// @endcode
ftxui::Component ThemedProgressBar(float* progress,
                                    std::string_view label = "");

/// @brief Progress bar driven by a render-time callback.
ftxui::Component ThemedProgressBar(std::function<float()> progress_fn,
                                    std::string_view label = "");

// ── Spinner overlay ───────────────────────────────────────────────────────────

/// @brief Overlay an animated spinner over a component while @p show is true.
///
/// @code
/// bool loading = false;
/// comp |= ui::WithSpinner("Loading data…", &loading);
/// @endcode
ftxui::ComponentDecorator WithSpinner(std::string_view message,
                                       const bool* show);
ftxui::Component WithSpinner(ftxui::Component parent,
                              std::string_view message,
                              const bool* show);

/// @brief Spinner driven by a callback (useful with ui::State<bool>).
ftxui::ComponentDecorator WithSpinner(std::string_view message,
                                       std::function<bool()> is_visible);
ftxui::Component WithSpinner(ftxui::Component parent,
                              std::string_view message,
                              std::function<bool()> is_visible);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_PROGRESS_HPP
