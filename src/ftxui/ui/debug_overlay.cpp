// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/debug_overlay.hpp"

#include <chrono>
#include <memory>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

using namespace ftxui;

namespace ftxui::ui {

namespace {

class DebugOverlayComponent : public ComponentBase {
 public:
  explicit DebugOverlayComponent(Component inner) : inner_(std::move(inner)) {
    Add(inner_);
  }

  Element OnRender() override {
    auto start = std::chrono::high_resolution_clock::now();
    Element content = inner_->Render();
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    last_render_ms_ =
        std::chrono::duration<double, std::milli>(elapsed).count();

    if (!show_debug_) {
      return content;
    }

    // ── Build debug panel ───────────────────────────────────────────────────
    const Color accent = Color::Cyan;
    const Color label = Color::GrayDark;

    // Compute events/s from accumulated counter.
    const auto now = std::chrono::steady_clock::now();
    const double elapsed_s =
        std::chrono::duration<double>(now - window_start_).count();
    double eps = 0.0;
    if (elapsed_s >= 1.0) {
      eps = static_cast<double>(event_count_in_window_) / elapsed_s;
      event_count_in_window_ = 0;
      window_start_ = now;
    }

    auto field = [&](const std::string& lbl,
                     const std::string& val) -> Element {
      return hbox({
          text(lbl) | color(label),
          text(val) | bold | color(accent),
          text("  "),
      });
    };

    // Render time string.
    char render_buf[32];
    std::snprintf(render_buf, sizeof(render_buf), "%.2f ms", last_render_ms_);

    // eps string.
    char eps_buf[32];
    std::snprintf(eps_buf, sizeof(eps_buf), "%.1f ev/s", eps);

    // Mouse position string.
    char mouse_buf[32];
    std::snprintf(mouse_buf, sizeof(mouse_buf), "(%d, %d)", last_mouse_x_,
                  last_mouse_y_);

    Element panel =
        vbox({
            separatorLight(),
            hbox({
                text(" ") | color(label),
                text("\u256d\u2500 FTXUI Debug ") | color(accent) | bold,
                filler(),
                text("Ctrl+D to hide ") | color(label) | dim,
            }),
            hbox({
                text(" "),
                field("Render: ", render_buf),
                field("Events: ", eps_buf),
                field("Mouse: ", mouse_buf),
                field("Last event: ", last_event_str_),
                filler(),
            }),
        }) |
        border;

    return vbox({content | flex, panel});
  }

  bool OnEvent(Event event) override {
    // Ctrl+D toggles the overlay.
    if (event == Event::Special("\x04")) {
      show_debug_ = !show_debug_;
      return true;
    }

    // Track mouse position for display.
    if (event.is_mouse()) {
      last_mouse_x_ = event.mouse().x;
      last_mouse_y_ = event.mouse().y;
    }

    // Track last event description.
    last_event_str_ = DescribeEvent(event);
    ++event_count_in_window_;

    return inner_->OnEvent(event);
  }

  Component ActiveChild() override { return inner_; }

 private:
  Component inner_;
  bool show_debug_ = false;
  double last_render_ms_ = 0.0;

  int last_mouse_x_ = 0;
  int last_mouse_y_ = 0;

  std::string last_event_str_ = "(none)";

  // Event rate tracking.
  using Clock = std::chrono::steady_clock;
  Clock::time_point window_start_ = Clock::now();
  int event_count_in_window_ = 0;

  static std::string DescribeEvent(Event e) {
    if (e.is_mouse()) {
      static const char* const kBtn[] = {
          "Left", "Mid", "Right", "None", "WheelUp", "WheelDn",
      };
      int bi = static_cast<int>(e.mouse().button);
      const char* btn = (bi >= 0 && bi < 6) ? kBtn[bi] : "?";
      static const char* const kMot[] = {"Released", "Pressed", "Moved"};
      int mi = static_cast<int>(e.mouse().motion);
      const char* mot = (mi >= 0 && mi < 3) ? kMot[mi] : "?";
      char buf[64];
      std::snprintf(buf, sizeof(buf), "Mouse %s %s (%d,%d)", btn, mot,
                    e.mouse().x, e.mouse().y);
      return buf;
    }
    if (e.is_character()) {
      return "Char('" + e.character() + "')";
    }
    // Use the raw input string as a fallback (truncated to 20 chars).
    std::string raw = e.input();
    if (raw.size() > 20) {
      raw = raw.substr(0, 20) + "…";
    }
    if (raw.empty()) {
      return "(custom)";
    }
    return raw;
  }
};

}  // namespace

ftxui::Component WithDebugOverlay(ftxui::Component inner) {
  return std::make_shared<DebugOverlayComponent>(std::move(inner));
}

}  // namespace ftxui::ui
