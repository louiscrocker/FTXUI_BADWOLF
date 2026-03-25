// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_TEXTINPUT_HPP
#define FTXUI_UI_TEXTINPUT_HPP

#include <functional>
#include <string>
#include <string_view>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/bind.hpp"

namespace ftxui::ui {

/// @brief A fully-featured single-line text input field with label,
/// placeholder, validation, error message, and character limit.
///
/// Unlike the raw FTXUI `Input`, `TextInput` is self-contained — it owns its
/// label rendering, validation state, and error display.  Drop it anywhere in
/// a layout without wrapping it in a separate `Renderer`.
///
/// @code
/// std::string email;
/// auto field = ui::TextInput("Email")
///     .Bind(&email)
///     .Placeholder("you@example.com")
///     .MaxLength(64)
///     .Validate([](std::string_view s){
///         return s.find('@') != std::string_view::npos;
///     }, "Must be a valid email")
///     .OnChange([&]{ /* ... */ })
///     .Build();
/// @endcode
///
/// @ingroup ui
class TextInput {
 public:
  explicit TextInput(std::string_view label = "");

  /// @brief Bind an external string to hold the input value.
  TextInput& Bind(std::string* value);

  /// @brief Two-way reactive binding: syncs the input with a Bind<string>.
  ///
  /// Changes typed by the user update the reactive; external changes to the
  /// reactive (via Set()) update the displayed text.
  TextInput& Bind(ftxui::ui::Bind<std::string>& binding);

  /// @brief Grey placeholder shown when the field is empty.
  TextInput& Placeholder(std::string_view text);

  /// @brief Maximum number of characters the user may type (0 = unlimited).
  TextInput& MaxLength(size_t n);

  /// @brief Validation function + error message shown below the field.
  /// The validator is called on every keystroke; the error is shown only when
  /// the field has been touched (user typed something and moved away).
  TextInput& Validate(std::function<bool(std::string_view)> fn,
                      std::string_view error_msg = "Invalid input");

  /// @brief Called on every change (after MaxLength truncation).
  TextInput& OnChange(std::function<void()> fn);

  /// @brief Called when the user presses Enter.
  TextInput& OnSubmit(std::function<void()> fn);

  /// @brief Width of the label column. Default: 14.
  TextInput& LabelWidth(int w);

  /// @brief Password mode — characters are masked with '●'.
  TextInput& Password(bool v = true);

  /// @brief Build the FTXUI component.
  ftxui::Component Build();

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_TEXTINPUT_HPP
