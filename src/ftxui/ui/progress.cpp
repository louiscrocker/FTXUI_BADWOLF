// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/progress.hpp"

#include <cstddef>
#include <string>
#include <utility>

#include "ftxui/component/animation.hpp"
#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── ThemedProgressBar
// ─────────────────────────────────────────────────────────

Component ThemedProgressBar(float* progress, std::string_view label) {
  std::string lbl{label};
  return ThemedProgressBar([progress]() { return *progress; }, lbl);
}

Component ThemedProgressBar(std::function<float()> progress_fn,
                            std::string_view label) {
  std::string lbl{label};
  return Renderer([progress_fn = std::move(progress_fn), lbl]() -> Element {
    const Theme& t = GetTheme();
    float p = progress_fn();
    if (p < 0.f) {
      p = 0.f;
    }
    if (p > 1.f) {
      p = 1.f;
    }

    int pct = static_cast<int>(p * 100.f);
    auto bar = gauge(p) | color(t.primary) | xflex;
    auto pct_text = text(" " + std::to_string(pct) + "%") |
                    color(t.text_muted) | size(WIDTH, EQUAL, 5);

    if (lbl.empty()) {
      return hbox({bar, pct_text});
    }
    return hbox({
        text(lbl + " ") | color(t.text_muted) |
            size(WIDTH, EQUAL, static_cast<int>(lbl.size()) + 1),
        bar,
        pct_text,
    });
  });
}

// ── WithSpinner
// ───────────────────────────────────────────────────────────────

namespace {

// Renders the spinner overlay element (frame animated via
// RequestAnimationFrame).
Element SpinnerOverlayElement(std::string_view message,
                              bool visible,
                              size_t& frame_counter) {
  if (!visible) {
    return emptyElement();
  }

  const Theme& t = GetTheme();
  ++frame_counter;

  auto spin = spinner(3, frame_counter) | color(t.primary);
  auto overlay = hbox({
                     spin,
                     text(" " + std::string(message)) | bold | color(t.text),
                 }) |
                 borderStyled(t.border_style, t.primary) | center;

  return dbox({
      // Dim the background.
      text("") | xflex | yflex | dim,
      overlay,
  });
}

}  // namespace

Component WithSpinner(Component parent,
                      std::string_view message,
                      const bool* show) {
  return WithSpinner(std::move(parent), message,
                     [show]() -> bool { return *show; });
}

ComponentDecorator WithSpinner(std::string_view message, const bool* show) {
  return [message = std::string(message), show](Component comp) -> Component {
    return WithSpinner(std::move(comp), message, show);
  };
}

Component WithSpinner(Component parent,
                      std::string_view message,
                      std::function<bool()> is_visible) {
  std::string msg{message};
  auto frame = std::make_shared<size_t>(0);

  auto overlay_comp = Renderer([is_visible, msg, frame]() -> Element {
    bool visible = is_visible();
    if (visible) {
      // Keep requesting animation frames while the spinner is shown.
      if (App* app = App::Active()) {
        app->RequestAnimationFrame();
      }
    }
    return SpinnerOverlayElement(msg, visible, *frame);
  });

  auto container = Container::Stacked({parent, overlay_comp});

  return Renderer(container, [parent, overlay_comp]() -> Element {
    return dbox({
        parent->Render(),
        overlay_comp->Render(),
    });
  });
}

ComponentDecorator WithSpinner(std::string_view message,
                               std::function<bool()> is_visible) {
  return [message = std::string(message),
          is_visible = std::move(is_visible)](Component comp) -> Component {
    return WithSpinner(std::move(comp), message, is_visible);
  };
}

}  // namespace ftxui::ui
