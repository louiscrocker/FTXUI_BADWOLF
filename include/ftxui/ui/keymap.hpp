// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_KEYMAP_HPP
#define FTXUI_UI_KEYMAP_HPP

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

/// @brief Registry of keyboard shortcuts with automatic help rendering.
///
/// @code
/// auto kb = ftxui::ui::Keymap()
///     .Bind("q",      [&]{ app.Exit(); },     "Quit")
///     .Bind("Ctrl+s", [&]{ save(); },          "Save")
///     .Bind("?",      [&]{ show_help=true; },  "Help");
///
/// my_component |= kb.AsDecorator();
/// @endcode
///
/// Supported key string formats:
///  - Single printable character:  "q", "?", "/"
///  - Named keys:  "Enter", "Escape", "Tab", "Backspace", "Delete",
///                 "Up", "Down", "Left", "Right"
///  - Function keys:  "F1" … "F12"
///  - Control combos:  "Ctrl+a" … "Ctrl+z" (case-insensitive letter)
///
/// @ingroup ui
class Keymap {
 public:
  /// @brief Register a keybinding.
  /// @param key          Key string (see class description for formats).
  /// @param action       Callback invoked when the key is pressed.
  /// @param description  Optional human-readable description shown in help.
  Keymap& Bind(std::string_view key,
               std::function<void()> action,
               std::string_view description = "");

  /// @brief Wrap @p component so that registered keys are intercepted.
  ftxui::Component Wrap(ftxui::Component component) const;

  /// @brief Return a ComponentDecorator for use with |= operator.
  ftxui::ComponentDecorator AsDecorator() const;

  /// @brief Render a two-column table of key bindings for display in a help
  ///        dialog.
  ftxui::Element HelpElement() const;

  /// @brief Implicit conversion to ComponentDecorator.
  operator ftxui::ComponentDecorator() const {  // NOLINT
    return AsDecorator();
  }

 private:
  struct Binding {
    std::string key;
    std::function<void()> action;
    std::string description;
  };
  std::vector<Binding> bindings_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_KEYMAP_HPP
