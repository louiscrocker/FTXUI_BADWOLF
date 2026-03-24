// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "ftxui/ui/config_editor.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

namespace ftxui::ui {

// ── Field kinds ───────────────────────────────────────────────────────────────

enum class FieldKind { String, Int, Float, Bool, Section };

struct Field {
  FieldKind kind = FieldKind::String;
  std::string key;          // empty for Section entries
  std::string description;

  // All non-bool values are stored as strings in the input buffer.
  std::string buffer;  // TextInput target
  bool bool_value = false;

  // Defaults (used when key is absent in saved file)
  std::string default_str;
  bool default_bool = false;

  // The built component for this field (set during Build())
  ftxui::Component component;
};

// ── Impl ──────────────────────────────────────────────────────────────────────

struct ConfigEditor::Impl {
  std::string file_path;
  std::string title;
  std::vector<Field> fields;
  std::function<void()> on_change;

  // Status message shown near Save/Load buttons
  std::string status_msg;

  // Helper: find field by key
  Field* FindField(std::string_view key) {
    for (auto& f : fields) {
      if (f.key == key) {
        return &f;
      }
    }
    return nullptr;
  }

  const Field* FindField(std::string_view key) const {
    for (const auto& f : fields) {
      if (f.key == key) {
        return &f;
      }
    }
    return nullptr;
  }

  bool SaveToFile() const {
    if (file_path.empty()) {
      return false;
    }
    std::ofstream out(file_path);
    if (!out) {
      return false;
    }
    for (const auto& f : fields) {
      if (f.kind == FieldKind::Section) {
        continue;
      }
      if (f.kind == FieldKind::Bool) {
        out << f.key << "=" << (f.bool_value ? "true" : "false") << "\n";
      } else {
        out << f.key << "=" << f.buffer << "\n";
      }
    }
    return true;
  }

  bool LoadFromFile() {
    if (file_path.empty()) {
      return false;
    }
    std::ifstream in(file_path);
    if (!in) {
      return false;
    }
    std::string line;
    while (std::getline(in, line)) {
      auto eq = line.find('=');
      if (eq == std::string::npos) {
        continue;
      }
      std::string key = line.substr(0, eq);
      std::string val = line.substr(eq + 1);

      Field* f = FindField(key);
      if (!f) {
        continue;
      }
      if (f->kind == FieldKind::Bool) {
        f->bool_value = (val == "true" || val == "1");
        f->buffer = f->bool_value ? "true" : "false";
      } else {
        f->buffer = val;
      }
    }
    return true;
  }
};

// ── Constructor ───────────────────────────────────────────────────────────────

ConfigEditor::ConfigEditor() : impl_(std::make_shared<Impl>()) {}

// ── Fluent setters ────────────────────────────────────────────────────────────

ConfigEditor& ConfigEditor::File(std::string_view path) {
  impl_->file_path = std::string(path);
  return *this;
}

ConfigEditor& ConfigEditor::Title(std::string_view title) {
  impl_->title = std::string(title);
  return *this;
}

ConfigEditor& ConfigEditor::String(std::string_view key,
                                   std::string default_value,
                                   std::string_view description) {
  Field f;
  f.kind = FieldKind::String;
  f.key = std::string(key);
  f.description = std::string(description);
  f.buffer = std::move(default_value);
  f.default_str = f.buffer;
  impl_->fields.push_back(std::move(f));
  return *this;
}

ConfigEditor& ConfigEditor::Int(std::string_view key,
                                int default_value,
                                std::string_view description) {
  Field f;
  f.kind = FieldKind::Int;
  f.key = std::string(key);
  f.description = std::string(description);
  f.buffer = std::to_string(default_value);
  f.default_str = f.buffer;
  impl_->fields.push_back(std::move(f));
  return *this;
}

ConfigEditor& ConfigEditor::Float(std::string_view key,
                                  float default_value,
                                  std::string_view description) {
  Field f;
  f.kind = FieldKind::Float;
  f.key = std::string(key);
  f.description = std::string(description);
  std::ostringstream oss;
  oss << default_value;
  f.buffer = oss.str();
  f.default_str = f.buffer;
  impl_->fields.push_back(std::move(f));
  return *this;
}

ConfigEditor& ConfigEditor::Bool(std::string_view key,
                                 bool default_value,
                                 std::string_view description) {
  Field f;
  f.kind = FieldKind::Bool;
  f.key = std::string(key);
  f.description = std::string(description);
  f.bool_value = default_value;
  f.default_bool = default_value;
  f.buffer = default_value ? "true" : "false";
  impl_->fields.push_back(std::move(f));
  return *this;
}

ConfigEditor& ConfigEditor::Section(std::string_view label) {
  Field f;
  f.kind = FieldKind::Section;
  f.key = "";
  f.description = std::string(label);
  impl_->fields.push_back(std::move(f));
  return *this;
}

ConfigEditor& ConfigEditor::OnChange(std::function<void()> fn) {
  impl_->on_change = std::move(fn);
  return *this;
}

// ── Value accessors ───────────────────────────────────────────────────────────

std::string ConfigEditor::GetString(std::string_view key) const {
  const Field* f = impl_->FindField(key);
  if (!f) {
    return {};
  }
  return f->buffer;
}

int ConfigEditor::GetInt(std::string_view key) const {
  const Field* f = impl_->FindField(key);
  if (!f) {
    return 0;
  }
  try {
    return std::stoi(f->buffer);
  } catch (...) {
    return 0;
  }
}

float ConfigEditor::GetFloat(std::string_view key) const {
  const Field* f = impl_->FindField(key);
  if (!f) {
    return 0.f;
  }
  try {
    return std::stof(f->buffer);
  } catch (...) {
    return 0.f;
  }
}

bool ConfigEditor::GetBool(std::string_view key) const {
  const Field* f = impl_->FindField(key);
  if (!f) {
    return false;
  }
  return f->bool_value;
}

// ── Persistence ───────────────────────────────────────────────────────────────

bool ConfigEditor::Save() const {
  return impl_->SaveToFile();
}

bool ConfigEditor::Load() {
  return impl_->LoadFromFile();
}

// ── Build ─────────────────────────────────────────────────────────────────────

ftxui::Component ConfigEditor::Build() {
  auto s = impl_;

  // Auto-load if file exists
  {
    std::ifstream probe(s->file_path);
    if (probe.good()) {
      s->LoadFromFile();
    }
  }

  // Build per-field components
  ftxui::Components field_components;
  for (auto& f : s->fields) {
    if (f.kind == FieldKind::Section) {
      // Section: pure renderer, no interaction
      std::string label = f.description;
      f.component = ftxui::Renderer([label]() -> ftxui::Element {
        const Theme& t = GetTheme();
        return ftxui::vbox({
            ftxui::separatorLight(),
            ftxui::text(" " + label) | ftxui::bold |
                ftxui::color(t.primary),
        });
      });
    } else if (f.kind == FieldKind::Bool) {
      // Checkbox
      std::string key = f.key;
      std::string desc = f.description;
      auto* bool_ptr = &f.bool_value;
      auto* buf_ptr = &f.buffer;
      auto on_change = s->on_change;

      ftxui::CheckboxOption opt;
      opt.on_change = [bool_ptr, buf_ptr, on_change]() {
        *buf_ptr = *bool_ptr ? "true" : "false";
        if (on_change) {
          on_change();
        }
      };

      auto checkbox = ftxui::Checkbox(key, bool_ptr, opt);

      f.component = ftxui::Renderer(checkbox, [checkbox, key, desc]() -> ftxui::Element {
        const Theme& t = GetTheme();
        ftxui::Element cb_el = checkbox->Render();
        ftxui::Element desc_el =
            desc.empty() ? ftxui::text("")
                         : ftxui::text("  " + desc) |
                               ftxui::color(t.text_muted) | ftxui::dim;
        return ftxui::hbox({cb_el, desc_el, ftxui::filler()});
      });
    } else {
      // TextInput for String/Int/Float
      std::string key = f.key;
      std::string desc = f.description;
      FieldKind kind = f.kind;
      auto* buf_ptr = &f.buffer;
      auto on_change = s->on_change;

      ftxui::InputOption opt;
      opt.on_change = [buf_ptr, on_change]() {
        if (on_change) {
          on_change();
        }
      };

      // Inline validation for Int/Float
      auto input_comp = ftxui::Input(buf_ptr, opt);

      f.component = ftxui::Renderer(input_comp, [input_comp, key, desc, kind, buf_ptr]() -> ftxui::Element {
        const Theme& t = GetTheme();
        bool focused = input_comp->Focused();

        // Validation
        bool valid = true;
        if (kind == FieldKind::Int) {
          try {
            std::stoi(*buf_ptr);
          } catch (...) {
            valid = buf_ptr->empty();
          }
        } else if (kind == FieldKind::Float) {
          try {
            std::stof(*buf_ptr);
          } catch (...) {
            valid = buf_ptr->empty();
          }
        }

        // Label
        std::string padded = key;
        while (static_cast<int>(padded.size()) < 16) {
          padded += ' ';
        }
        ftxui::Element lbl = ftxui::text(padded + ": ") | ftxui::dim;

        ftxui::Element inp = input_comp->Render() | ftxui::flex;
        if (!valid) {
          inp = inp | ftxui::color(t.error_color);
        } else if (focused) {
          inp = inp | ftxui::color(t.primary);
        }

        ftxui::Element desc_el =
            desc.empty()
                ? ftxui::text("")
                : ftxui::text("  " + desc) | ftxui::color(t.text_muted) |
                      ftxui::dim;

        return ftxui::hbox({lbl, inp, desc_el});
      });
    }
    field_components.push_back(f.component);
  }

  // Save / Load buttons + status
  auto* status_ptr = &s->status_msg;

  // We need shared_ptr copies for button callbacks
  auto save_fn = [s, status_ptr]() {
    if (s->SaveToFile()) {
      *status_ptr = "Saved!";
    } else {
      *status_ptr = "Error: could not save";
    }
  };
  auto load_fn = [s, status_ptr]() {
    if (s->LoadFromFile()) {
      *status_ptr = "Loaded!";
    } else {
      *status_ptr = "Error: could not load";
    }
  };

  ftxui::ButtonOption btn_opt = ftxui::ButtonOption::Simple();
  auto save_btn = ftxui::Button("Save", save_fn, btn_opt);
  auto load_btn = ftxui::Button("Load", load_fn, btn_opt);

  auto btn_row = ftxui::Container::Horizontal({save_btn, load_btn});

  // Compose all fields + buttons into a vertical container
  auto all_fields = ftxui::Container::Vertical(field_components);
  auto root = ftxui::Container::Vertical({all_fields, btn_row});

  std::string title = s->title;

  return ftxui::Renderer(root, [root, title, status_ptr]() -> ftxui::Element {
    const Theme& t = GetTheme();

    ftxui::Elements rows;

    if (!title.empty()) {
      rows.push_back(ftxui::text(" " + title) | ftxui::bold |
                     ftxui::color(t.primary));
      rows.push_back(ftxui::separator());
    }

    rows.push_back(root->Render() | ftxui::flex);

    rows.push_back(ftxui::separator());

    // Status bar
    ftxui::Element status_el =
        ftxui::hbox({ftxui::filler(),
                     ftxui::text(*status_ptr) | ftxui::color(t.accent)});
    rows.push_back(status_el);

    return ftxui::vbox(std::move(rows));
  });
}

}  // namespace ftxui::ui
