// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "ftxui/ui/textinput.hpp"

#include <string>
#include <utility>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/bind.hpp"
#include "ftxui/ui/theme.hpp"

namespace ftxui::ui {

struct TextInput::Impl {
  std::string  label;
  std::string  placeholder;
  std::string* bound       = nullptr;
  std::string  owned_value;          // used when no external binding
  size_t       max_length  = 0;      // 0 = unlimited
  int          label_width = 14;
  bool         password    = false;
  bool         touched     = false;  // user has blurred after typing

  // Reactive two-way binding (optional).
  std::shared_ptr<ftxui::ui::Reactive<std::string>> reactive_binding;
  bool suppress_reactive_update = false;

  std::function<bool(std::string_view)> validator;
  std::string  error_msg   = "Invalid input";
  std::function<void()>     on_change;
  std::function<void()>     on_submit;
};

TextInput::TextInput(std::string_view label)
    : impl_(std::make_shared<Impl>()) {
  impl_->label = std::string(label);
}

TextInput& TextInput::Bind(std::string* value) {
  impl_->bound = value;
  return *this;
}

TextInput& TextInput::Bind(ftxui::ui::Bind<std::string>& binding) {
  impl_->reactive_binding = binding.AsReactive();
  impl_->owned_value      = impl_->reactive_binding->Get();
  impl_->bound            = &impl_->owned_value;
  return *this;
}
TextInput& TextInput::Placeholder(std::string_view text) {
  impl_->placeholder = std::string(text);
  return *this;
}
TextInput& TextInput::MaxLength(size_t n) {
  impl_->max_length = n;
  return *this;
}
TextInput& TextInput::Validate(std::function<bool(std::string_view)> fn,
                               std::string_view error_msg) {
  impl_->validator  = std::move(fn);
  impl_->error_msg  = std::string(error_msg);
  return *this;
}
TextInput& TextInput::OnChange(std::function<void()> fn) {
  impl_->on_change = std::move(fn);
  return *this;
}
TextInput& TextInput::OnSubmit(std::function<void()> fn) {
  impl_->on_submit = std::move(fn);
  return *this;
}
TextInput& TextInput::LabelWidth(int w) {
  impl_->label_width = w;
  return *this;
}
TextInput& TextInput::Password(bool v) {
  impl_->password = v;
  return *this;
}

ftxui::Component TextInput::Build() {
  auto s = impl_;

  // Determine which string the Input component writes to.
  std::string* target = s->bound ? s->bound : &s->owned_value;

  InputOption opt;
  opt.placeholder = s->placeholder;
  opt.password    = s->password;

  // Intercept on_change for MaxLength and user callback.
  opt.on_change = [s, target] {
    // Enforce max length
    if (s->max_length > 0 && target->size() > s->max_length) {
      target->resize(s->max_length);
    }
    s->touched = true;
    // Propagate to reactive binding (without triggering the mirror listener).
    if (s->reactive_binding && !s->suppress_reactive_update) {
      s->suppress_reactive_update = true;
      s->reactive_binding->Set(*target);
      s->suppress_reactive_update = false;
    }
    if (s->on_change) s->on_change();
  };

  opt.on_enter = [s] {
    s->touched = true;
    if (s->on_submit) s->on_submit();
  };

  auto input = Input(target, opt);

  // Mirror reactive → owned_value whenever an external Set() occurs.
  if (s->reactive_binding) {
    auto weak_s = std::weak_ptr<Impl>(s);
    s->reactive_binding->OnChange([weak_s, target](const std::string& val) {
      if (auto sp = weak_s.lock()) {
        if (!sp->suppress_reactive_update) {
          *target = val;
        }
      }
    });
  }

  return Renderer(input, [s, input, target]() -> Element {
    const Theme& t  = GetTheme();
    bool   focused  = input->Focused();
    bool   valid    = !s->validator || s->validator(*target);
    bool   show_err = s->touched && !valid && !target->empty();

    // Label
    std::string padded = s->label;
    while ((int)padded.size() < s->label_width) padded += ' ';

    Element lbl = text(padded + ": ") | dim;

    // Input box: colored border when focused or errored
    Element inp = input->Render();
    if (show_err) {
      inp = inp | color(t.error_color);
    } else if (focused) {
      inp = inp | color(t.primary);
    }
    inp = inp | flex;

    // Optional char-count hint
    Element hint = text("");
    if (s->max_length > 0) {
      hint = text(" " + std::to_string(target->size()) + "/" +
                  std::to_string(s->max_length)) |
             dim;
    }

    Element row = hbox({lbl, inp, hint});

    if (show_err) {
      Element err = text(std::string(s->label_width + 2, ' ') + s->error_msg) |
                    color(t.error_color) | dim;
      return vbox({row, err});
    }
    return row;
  });
}

}  // namespace ftxui::ui
