// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_DIALOG_HPP
#define FTXUI_UI_DIALOG_HPP

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Overlay a Yes/No confirmation dialog on top of @p parent.
///
/// Set *show to true to open the dialog.  Both callbacks close it automatically.
///
/// @code
/// bool show_confirm = false;
/// main_comp |= ui::WithConfirm(
///     "Delete file?",
///     [&]{ do_delete(); show_confirm = false; },
///     [&]{ show_confirm = false; },
///     &show_confirm);
/// @endcode
ftxui::ComponentDecorator WithConfirm(std::string_view message,
                                      std::function<void()> on_yes,
                                      std::function<void()> on_no,
                                      const bool* show);

/// @brief Full form: wrap @p parent with a confirm dialog.
ftxui::Component WithConfirm(ftxui::Component parent,
                              std::string_view message,
                              std::function<void()> on_yes,
                              std::function<void()> on_no,
                              const bool* show);

// ─────────────────────────────────────────────────────────────────────────────

/// @brief Overlay an informational alert dialog on top of @p parent.
///
/// @code
/// bool show_alert = false;
/// main_comp |= ui::WithAlert("Error", "File not found.", &show_alert);
/// @endcode
ftxui::ComponentDecorator WithAlert(std::string_view title,
                                    std::string_view message,
                                    const bool* show,
                                    std::function<void()> on_close = {});

/// @brief Full form: wrap @p parent with an alert dialog.
ftxui::Component WithAlert(ftxui::Component parent,
                            std::string_view title,
                            std::string_view message,
                            const bool* show,
                            std::function<void()> on_close = {});

// ─────────────────────────────────────────────────────────────────────────────

/// @brief Overlay a keyboard-shortcut help panel on top of @p parent.
///
/// @param bindings  List of { key_display, description } pairs.
///
/// @code
/// bool show_help = false;
/// main_comp |= ui::WithHelp(
///     "Keyboard shortcuts",
///     {{"q",      "Quit"},
///      {"Ctrl+s", "Save"},
///      {"?",      "Show help"}},
///     &show_help);
/// @endcode
ftxui::ComponentDecorator WithHelp(
    std::string_view title,
    const std::vector<std::pair<std::string, std::string>>& bindings,
    const bool* show,
    std::function<void()> on_close = {});

/// @brief Full form: wrap @p parent with a help dialog.
ftxui::Component WithHelp(
    ftxui::Component parent,
    std::string_view title,
    const std::vector<std::pair<std::string, std::string>>& bindings,
    const bool* show,
    std::function<void()> on_close = {});

}  // namespace ftxui::ui

#endif  // FTXUI_UI_DIALOG_HPP
