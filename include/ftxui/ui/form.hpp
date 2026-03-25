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
#include "ftxui/ui/bind.hpp"
#include "ftxui/ui/textinput.hpp"

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

  // ── Reactive model binding (template helpers) ─────────────────────────────

  /// @brief Internal helper used by BindField/BindCheckbox/BindPassword.
  ///
  /// Adds a pre-built component with the given label to the form's entry list.
  Form& AddComponent(std::string_view label, ftxui::Component c);

  /// @brief Bind a string field of a reactive model struct to a text input.
  ///
  /// @code
  /// struct User { std::string name, email; };
  /// auto model = MakeBind<User>();
  /// Form().BindField("Name", model, &User::name)
  ///       .BindField("Email", model, &User::email)
  ///       .Build();
  /// @endcode
  template <typename ModelT>
  Form& BindField(std::string_view label,
                  Bind<ModelT>& model,
                  std::string ModelT::*field) {
    auto lens = MakeLens(model, field);
    // TextInput::Bind(Bind<string>&) captures the underlying reactive by
    // shared_ptr, so 'lens' may go out of scope safely after Build().
    return AddComponent(
        label, TextInput(label).Bind(lens).Build());
  }

  /// @brief Bind a string field as a password (masked) input.
  template <typename ModelT>
  Form& BindPassword(std::string_view label,
                     Bind<ModelT>& model,
                     std::string ModelT::*field) {
    auto lens = MakeLens(model, field);
    return AddComponent(
        label, TextInput(label).Password(true).Bind(lens).Build());
  }

  /// @brief Bind a bool field of a reactive model struct to a checkbox.
  template <typename ModelT>
  Form& BindCheckbox(std::string_view label,
                     Bind<ModelT>& model,
                     bool ModelT::*field) {
    // Build a reactive lens for the bool field.
    auto lens          = MakeLens(model, field);
    auto bool_reactive = lens.AsReactive();

    // Allocate stable storage that FTXUI's Checkbox can write to.
    auto storage = std::make_shared<bool>(bool_reactive->Get());

    // reactive → storage
    auto weak_storage = std::weak_ptr<bool>(storage);
    bool_reactive->OnChange([weak_storage](const bool& v) {
      if (auto s = weak_storage.lock()) {
        *s = v;
      }
    });

    // storage → reactive: wrap checkbox with an on-change observer.
    auto opt = ftxui::CheckboxOption::Simple();
    opt.on_change = [bool_reactive, storage] {
      bool_reactive->Set(*storage);
    };

    auto comp = ftxui::Checkbox(label, storage.get(), opt);

    // Keep storage alive for the lifetime of the component.
    auto keeper = ftxui::Renderer(comp, [comp, storage]() {
      return comp->Render();
    });

    return AddComponent(label, keeper);
  }

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_FORM_HPP
