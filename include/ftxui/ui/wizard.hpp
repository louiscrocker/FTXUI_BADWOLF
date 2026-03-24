// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_WIZARD_HPP
#define FTXUI_UI_WIZARD_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Fluent builder for multi-step wizard dialogs.
///
/// Example:
/// @code
/// auto wizard = ftxui::ui::Wizard("Setup")
///     .Step("Welcome",  welcome_component)
///     .Step("Options",  options_component)
///     .Step("Confirm",  confirm_component)
///     .OnComplete([&]{ app->Exit(); })
///     .OnCancel([&]{ app->Exit(); });
///
/// ftxui::ui::Run(wizard.Build());
/// @endcode
///
/// @ingroup ui
class Wizard {
 public:
  explicit Wizard(std::string_view title = "");

  Wizard& Step(std::string_view title, ftxui::Component content);
  Wizard& OnComplete(std::function<void()> on_complete);
  Wizard& OnCancel(std::function<void()> on_cancel);
  Wizard& Title(std::string_view title);

  ftxui::Component Build() const;
  operator ftxui::Component() const { return Build(); }  // NOLINT

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_WIZARD_HPP
