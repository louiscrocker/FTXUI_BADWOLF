// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/wizard.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Impl ──────────────────────────────────────────────────────────────────────

struct Wizard::Impl {
  struct Step {
    std::string title;
    Component component;
  };
  std::string title;
  std::vector<Step> steps;
  std::function<void()> on_complete;
  std::function<void()> on_cancel;
};

// ── Constructor / fluent setters ──────────────────────────────────────────────

Wizard::Wizard(std::string_view title)
    : impl_(std::make_shared<Impl>()) {
  impl_->title = std::string(title);
}

Wizard& Wizard::Step(std::string_view title, ftxui::Component content) {
  impl_->steps.push_back({std::string(title), std::move(content)});
  return *this;
}

Wizard& Wizard::OnComplete(std::function<void()> on_complete) {
  impl_->on_complete = std::move(on_complete);
  return *this;
}

Wizard& Wizard::OnCancel(std::function<void()> on_cancel) {
  impl_->on_cancel = std::move(on_cancel);
  return *this;
}

Wizard& Wizard::Title(std::string_view title) {
  impl_->title = std::string(title);
  return *this;
}

// ── Build ─────────────────────────────────────────────────────────────────────

Component Wizard::Build() const {
  if (impl_->steps.empty()) {
    return Renderer([] { return text("(no steps)"); });
  }

  auto impl = impl_;
  int n = static_cast<int>(impl->steps.size());
  auto current_step = std::make_shared<int>(0);

  // Container::Tab switches which step content is active/focused.
  Components step_components;
  step_components.reserve(impl->steps.size());
  for (auto& s : impl->steps) {
    step_components.push_back(s.component);
  }
  auto tab = Container::Tab(step_components, current_step.get());

  auto btn_style = GetTheme().MakeButtonStyle();

  // Cancel button
  auto btn_cancel = Button(" Cancel ", [impl]() {
    if (impl->on_cancel) {
      impl->on_cancel();
    }
  }, btn_style);

  // Back button — silently no-ops on step 0
  auto btn_back = Button("  Back  ", [current_step]() {
    if (*current_step > 0) {
      --(*current_step);
    }
  }, btn_style);

  // Next/Finish button — dynamic label via pointer
  auto action_label = std::make_shared<std::string>("  Next  ");
  auto btn_action = Button(action_label.get(), [current_step, n, impl]() {
    if (*current_step < n - 1) {
      ++(*current_step);
    } else if (impl->on_complete) {
      impl->on_complete();
    }
  }, btn_style);

  auto buttons = Container::Horizontal({btn_cancel, btn_back, btn_action});
  auto all = Container::Vertical({tab, buttons});

  return Renderer(
      all,
      [impl, current_step, tab, buttons, btn_cancel, btn_back, btn_action,
       action_label, n]() -> Element {
        const Theme& t = GetTheme();
        int step = *current_step;

        // Update action button label before rendering.
        *action_label = (step < n - 1) ? "  Next  " : " Finish ";

        // ── Progress indicator ────────────────────────────────────────────
        Elements progress;
        progress.reserve(static_cast<size_t>(2 * n - 1));
        for (int i = 0; i < n; i++) {
          if (i > 0) {
            Color line_color = (i <= step) ? t.accent : t.text_muted;
            progress.push_back(text("──") | color(line_color));
          }
          if (i < step) {
            // Done step
            progress.push_back(text("●") | color(t.accent));
          } else if (i == step) {
            // Current step
            progress.push_back(text("●") | bold | color(t.primary));
          } else {
            // Future step
            progress.push_back(text("○") | color(t.text_muted));
          }
        }

        // ── Navigation row ────────────────────────────────────────────────
        Elements nav;
        nav.push_back(btn_cancel->Render());
        nav.push_back(filler());
        if (step > 0) {
          nav.push_back(btn_back->Render());
        } else {
          // Placeholder with the same width as Back button to avoid layout jump.
          nav.push_back(text("        ") | dim);
        }
        nav.push_back(btn_action->Render());

        // ── Step content ──────────────────────────────────────────────────
        std::string step_title =
            (step < static_cast<int>(impl->steps.size()))
                ? impl->steps[static_cast<size_t>(step)].title
                : "";

        Element content = vbox({
            hbox(std::move(progress)) | hcenter,
            separatorLight(),
            tab->Render() | flex,
            separatorEmpty(),
            hbox(std::move(nav)),
        });

        std::string header = impl->title.empty()
                                 ? (" " + step_title + " ")
                                 : (" " + impl->title + " – " + step_title + " ");

        return window(text(header), std::move(content), t.border_style);
      });
}

}  // namespace ftxui::ui
