// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/form.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Internal implementation
// ───────────────────────────────────────────────────

struct Form::Impl {
  enum class Kind {
    Field,
    Password,
    Multiline,
    Check,
    Radio,
    Select,
    IntSlider,
    FloatSlider,
    Section,
    Separator,
    // Buttons are stored separately.
  };

  struct Entry {
    Kind kind = Kind::Field;
    std::string label;
    Component component;
    // Owned storage for string data so pointer stability is guaranteed.
    std::string placeholder_storage;
  };

  std::vector<Entry> entries;
  std::vector<std::pair<std::string, std::function<void()>>> buttons;
  std::string title;
  int label_width = 14;
};

// ── Constructor
// ───────────────────────────────────────────────────────────────

Form::Form() : impl_(std::make_shared<Impl>()) {}

// ── Reactive binding helper
// ───────────────────────────────────────────────────

Form& Form::AddComponent(std::string_view label, ftxui::Component c) {
  Impl::Entry e;
  e.kind = Impl::Kind::Field;  // renders the same as a text field row
  e.label = std::string(label);
  e.component = std::move(c);
  impl_->entries.push_back(std::move(e));
  return *this;
}

// ── Field builders
// ────────────────────────────────────────────────────────────

Form& Form::Field(std::string_view label,
                  std::string* value,
                  std::string_view placeholder) {
  Impl::Entry e;
  e.kind = Impl::Kind::Field;
  e.label = std::string(label);
  e.placeholder_storage = std::string(placeholder);
  InputOption opt = GetTheme().MakeInputStyle();
  opt.multiline = false;
  e.component = Input(value, &e.placeholder_storage, opt);
  impl_->entries.push_back(std::move(e));
  return *this;
}

Form& Form::Password(std::string_view label,
                     std::string* value,
                     std::string_view placeholder) {
  Impl::Entry e;
  e.kind = Impl::Kind::Password;
  e.label = std::string(label);
  e.placeholder_storage = std::string(placeholder);
  InputOption opt = GetTheme().MakeInputStyle();
  opt.password = true;
  opt.multiline = false;
  e.component = Input(value, &e.placeholder_storage, opt);
  impl_->entries.push_back(std::move(e));
  return *this;
}

Form& Form::Multiline(std::string_view label,
                      std::string* value,
                      std::string_view placeholder) {
  Impl::Entry e;
  e.kind = Impl::Kind::Multiline;
  e.label = std::string(label);
  e.placeholder_storage = std::string(placeholder);
  InputOption opt = GetTheme().MakeInputStyle();
  opt.multiline = true;
  e.component = Input(value, &e.placeholder_storage, opt);
  impl_->entries.push_back(std::move(e));
  return *this;
}

Form& Form::Check(std::string_view label, bool* value) {
  Impl::Entry e;
  e.kind = Impl::Kind::Check;
  e.label = std::string(label);
  CheckboxOption opt = CheckboxOption::Simple();
  e.component = Checkbox(label, value, opt);
  impl_->entries.push_back(std::move(e));
  return *this;
}

Form& Form::Radio(std::string_view label,
                  const std::vector<std::string>* options,
                  int* selected) {
  Impl::Entry e;
  e.kind = Impl::Kind::Radio;
  e.label = std::string(label);
  e.component = Radiobox(options, selected);
  impl_->entries.push_back(std::move(e));
  return *this;
}

Form& Form::Select(std::string_view label,
                   const std::vector<std::string>* options,
                   int* selected) {
  Impl::Entry e;
  e.kind = Impl::Kind::Select;
  e.label = std::string(label);
  e.component = Dropdown(options, selected);
  impl_->entries.push_back(std::move(e));
  return *this;
}

Form& Form::Integer(std::string_view label,
                    int* value,
                    int min,
                    int max,
                    int step) {
  Impl::Entry e;
  e.kind = Impl::Kind::IntSlider;
  e.label = std::string(label);
  e.component = Slider(std::string(label), value, min, max, step);
  impl_->entries.push_back(std::move(e));
  return *this;
}

Form& Form::Float(std::string_view label,
                  float* value,
                  float min,
                  float max,
                  float step) {
  Impl::Entry e;
  e.kind = Impl::Kind::FloatSlider;
  e.label = std::string(label);
  e.component = Slider(std::string(label), value, min, max, step);
  impl_->entries.push_back(std::move(e));
  return *this;
}

Form& Form::Section(std::string_view label) {
  Impl::Entry e;
  e.kind = Impl::Kind::Section;
  e.label = std::string(label);
  impl_->entries.push_back(std::move(e));
  return *this;
}

Form& Form::Separator() {
  Impl::Entry e;
  e.kind = Impl::Kind::Separator;
  impl_->entries.push_back(std::move(e));
  return *this;
}

Form& Form::Button(std::string_view label, std::function<void()> on_click) {
  impl_->buttons.emplace_back(std::string(label), std::move(on_click));
  return *this;
}

Form& Form::Submit(std::string_view label, std::function<void()> on_submit) {
  return Button(label, std::move(on_submit));
}

Form& Form::Cancel(std::string_view label, std::function<void()> on_cancel) {
  return Button(label, std::move(on_cancel));
}

Form& Form::Title(std::string_view title) {
  impl_->title = std::string(title);
  return *this;
}

Form& Form::LabelWidth(int width) {
  impl_->label_width = width;
  return *this;
}

// ── Build
// ─────────────────────────────────────────────────────────────────────

Component Form::Build() const {
  // Collect focusable components in vertical order.
  Components focusable;
  for (const auto& e : impl_->entries) {
    if (e.component) {
      focusable.push_back(e.component);
    }
  }

  // Build button row.
  Components btn_comps;
  auto btn_style = GetTheme().MakeButtonStyle();
  for (const auto& [lbl, action] : impl_->buttons) {
    btn_comps.push_back(ftxui::Button(lbl, action, btn_style));
  }
  Component btn_row;
  if (!btn_comps.empty()) {
    btn_row = Container::Horizontal(btn_comps);
    focusable.push_back(btn_row);
  }

  auto container = Container::Vertical(focusable);

  // Capture everything needed for rendering.
  auto impl = impl_;
  int lw = impl_->label_width;

  auto render_fn = [impl, container, btn_row, lw]() -> Element {
    Elements rows;

    // Determine which entries have a separate label prefix.
    auto needs_label_prefix = [](Impl::Kind k) {
      switch (k) {
        case Impl::Kind::Field:
        case Impl::Kind::Password:
        case Impl::Kind::Multiline:
        case Impl::Kind::Select:
        case Impl::Kind::Radio:
          return true;
        default:
          return false;
      }
    };

    for (const auto& e : impl->entries) {
      switch (e.kind) {
        case Impl::Kind::Section:
          rows.push_back(separatorEmpty());
          rows.push_back(text(" " + e.label) | bold |
                         color(GetTheme().primary));
          rows.push_back(separatorLight());
          break;

        case Impl::Kind::Separator:
          rows.push_back(separator());
          break;

        default:
          if (needs_label_prefix(e.kind)) {
            // "Label         : [field]"
            auto lbl = text(e.label + " :") | size(WIDTH, EQUAL, lw);
            rows.push_back(
                hbox({lbl, text(" "), e.component->Render() | xflex}));
          } else {
            // Checkboxes / sliders render themselves (include their own label).
            rows.push_back(e.component->Render() | xflex);
          }
          break;
      }
    }

    if (btn_row) {
      rows.push_back(separatorEmpty());
      rows.push_back(btn_row->Render() | hcenter);
    }

    Element doc = vbox(std::move(rows));

    const Theme& t = GetTheme();
    if (!impl->title.empty()) {
      doc = window(text(" " + impl->title + " ") | bold, doc);
    } else {
      doc = doc | borderStyled(t.border_style, t.border_color);
    }

    return doc;
  };

  return Renderer(container, render_fn);
}

}  // namespace ftxui::ui
