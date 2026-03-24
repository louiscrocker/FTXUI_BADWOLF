// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_FORM_HPP
#define FTXUI_UI_FORM_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Fluent builder for creating labeled data-entry forms.
///
/// Example:
/// @code
/// std::string name, email, password;
/// bool subscribe = false;
///
/// auto form = ftxui::ui::Form()
///     .Title("Sign Up")
///     .Field("Name",     &name,     "Your full name")
///     .Field("Email",    &email,    "you@example.com")
///     .Password("Pass",  &password)
///     .Check("Subscribe to newsletter", &subscribe)
///     .Submit("Register", on_submit)
///     .Cancel("Cancel",  on_cancel);
///
/// ftxui::App::TerminalOutput().Loop(form.Build());
/// @endcode
///
/// @ingroup ui
class Form {
 public:
  Form();

  // ── Text input fields ──────────────────────────────────────────────────────
  Form& Field(std::string_view label,
              std::string* value,
              std::string_view placeholder = "");

  Form& Password(std::string_view label,
                 std::string* value,
                 std::string_view placeholder = "");

  Form& Multiline(std::string_view label,
                  std::string* value,
                  std::string_view placeholder = "");

  // ── Boolean fields ─────────────────────────────────────────────────────────
  Form& Check(std::string_view label, bool* value);

  // ── Selection fields ───────────────────────────────────────────────────────
  Form& Radio(std::string_view label,
              const std::vector<std::string>* options,
              int* selected);

  Form& Select(std::string_view label,
               const std::vector<std::string>* options,
               int* selected);

  // ── Numeric fields ─────────────────────────────────────────────────────────
  Form& Integer(std::string_view label,
                int* value,
                int min = 0,
                int max = 100,
                int step = 5);

  Form& Float(std::string_view label,
              float* value,
              float min = 0.f,
              float max = 100.f,
              float step = 1.f);

  // ── Layout / grouping ──────────────────────────────────────────────────────
  Form& Section(std::string_view label);
  Form& Separator();

  // ── Action buttons ─────────────────────────────────────────────────────────
  Form& Button(std::string_view label, std::function<void()> on_click = {});
  Form& Submit(std::string_view label, std::function<void()> on_submit = {});
  Form& Cancel(std::string_view label, std::function<void()> on_cancel = {});

  // ── Appearance ─────────────────────────────────────────────────────────────
  Form& Title(std::string_view title);
  Form& LabelWidth(int width);

  // ── Build ──────────────────────────────────────────────────────────────────
  ftxui::Component Build() const;

  // Implicit conversion so a Form can be passed directly where a Component
  // is expected.
  operator ftxui::Component() const { return Build(); }  // NOLINT

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_FORM_HPP
