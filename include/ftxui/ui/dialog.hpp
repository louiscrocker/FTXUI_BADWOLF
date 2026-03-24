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

// ─────────────────────────────────────────────────────────────────────────────

/// @brief Side for a Drawer overlay.
enum class DrawerSide { Left, Right, Bottom };

/// @brief Overlay a slide-in drawer panel on top of @p parent.
///
/// The drawer renders @p content in a panel anchored to the chosen side.
/// It is shown when *show is true.
///
/// @code
/// bool show_drawer = false;
/// auto settings = Renderer([]{ return text("Settings"); });
/// main_comp = ui::WithDrawer(main_comp, DrawerSide::Right, "Settings",
///                            settings, &show_drawer);
/// // Toggle with:  show_drawer = !show_drawer;
/// @endcode
ftxui::Component WithDrawer(ftxui::Component parent,
                             DrawerSide side,
                             std::string_view title,
                             ftxui::Component content,
                             const bool* show,
                             int size = 40);

ftxui::ComponentDecorator WithDrawer(DrawerSide side,
                                     std::string_view title,
                                     ftxui::Component content,
                                     const bool* show,
                                     int size = 40);

// ─────────────────────────────────────────────────────────────────────────────

/// @brief A button definition for WithModal.
struct ModalButton {
  std::string          label;
  std::function<void()> action;
  bool                 is_primary = false;  ///< Render with primary color.
};

/// @brief Overlay a general-purpose modal dialog on top of @p parent.
///
/// @param title    Title bar text.
/// @param content  Body component rendered inside the modal.
/// @param buttons  Row of action buttons (right-aligned).
/// @param show     Pointer to boolean that controls visibility.
///
/// @code
/// bool show_modal = false;
/// auto body = Renderer([]{ return paragraph("Are you sure?"); });
/// main_comp = ui::WithModal(main_comp, "Confirm", body,
///     {{"Cancel",  [&]{ show_modal=false; },         false},
///      {"Delete",  [&]{ do_delete(); show_modal=false; }, true}},
///     &show_modal);
/// @endcode
ftxui::Component WithModal(ftxui::Component parent,
                            std::string_view title,
                            ftxui::Component content,
                            std::vector<ModalButton> buttons,
                            const bool* show);

ftxui::ComponentDecorator WithModal(std::string_view title,
                                    ftxui::Component content,
                                    std::vector<ModalButton> buttons,
                                    const bool* show);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_DIALOG_HPP
