// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_COMMAND_PALETTE_HPP
#define FTXUI_UI_COMMAND_PALETTE_HPP

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Searchable command palette overlay (VS Code / Telescope style).
///
/// Register commands with Add(), choose an activation key with Bind(), then
/// wrap your root component with Wrap() or AsDecorator().  The palette opens
/// as a modal centered on screen and filters commands as the user types.
///
/// Default activation key: Ctrl+P.
///
/// @code
/// auto app_root = ftxui::ui::CommandPalette()
///     .Add("Quit",     [&]{ App::Active()->Exit(); }, "Exit the application")
///     .Add("Save",     [&]{ save(); },                "Save current file")
///     .Bind("Ctrl+p")
///     .Wrap(my_component);
/// @endcode
///
/// @ingroup ui
class CommandPalette {
 public:
  struct Command {
    std::string name;
    std::string description;
    std::string category;
    std::function<void()> action;
  };

  /// @brief Register a command.
  CommandPalette& Add(std::string_view name,
                      std::function<void()> action,
                      std::string_view description = "",
                      std::string_view category = "");

  /// @brief Set the activation key string (e.g. "Ctrl+p", "F1").
  CommandPalette& Bind(std::string_view key = "Ctrl+p");

  /// @brief Return a ComponentDecorator (for use with |= operator).
  ftxui::ComponentDecorator AsDecorator() const;

  /// @brief Wrap @p parent with the command palette overlay.
  ftxui::Component Wrap(ftxui::Component parent) const;

 private:
  std::vector<Command> commands_;
  std::string bind_key_ = "Ctrl+p";
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_COMMAND_PALETTE_HPP
