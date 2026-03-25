// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file reactive_demo.cpp
/// Demonstrates ui::Reactive<T>, ui::Computed<T>, and ui::ReactiveGroup.
///
///  - A background thread increments a Reactive<int> counter every 500 ms.
///  - A MakeComputed() derives a label string automatically.
///  - Three Reactive<float> latencies are updated by a second background thread
///    and visualised with a Sparkline per row.
///  - Press 'q' or Escape to quit.

#include <atomic>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/reactive.hpp"

using namespace ftxui;
using namespace ftxui::ui;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string PadRight(std::string s, int width) {
  while (static_cast<int>(s.size()) < width) {
    s += ' ';
  }
  return s;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
  SetTheme(Theme::Nord());

  // ── Counter state ──────────────────────────────────────────────────────────
  Reactive<int> counter(0);

  // Derived label: recomputes whenever counter changes.
  auto counter_label = MakeComputed(
      [](int v) -> std::string {
        return "Count: " + std::to_string(v) +
               (v % 2 == 0 ? "  (even)" : "  (odd)");
      },
      counter);

  // ── Latency state ──────────────────────────────────────────────────────────
  struct LatencyRow {
    std::string name;
    Reactive<float> current{0.f};
    Reactive<std::vector<float>> history{std::vector<float>(40, 0.f)};
  };

  LatencyRow rows[3];
  rows[0].name = "DNS lookup";
  rows[1].name = "TCP connect";
  rows[2].name = "HTTP round-trip";

  // ── Stop flag ─────────────────────────────────────────────────────────────
  std::atomic<bool> running{true};

  // ── Background thread 1: increment counter ─────────────────────────────────
  std::thread counter_thread([&] {
    while (running.load()) {
      std::this_thread::sleep_for(500ms);
      counter.Update([](int& v) { v++; });
    }
  });

  // ── Background thread 2: simulate latency updates ─────────────────────────
  std::thread latency_thread([&] {
    double t = 0.0;
    while (running.load()) {
      std::this_thread::sleep_for(200ms);
      t += 0.25;

      ReactiveGroup group;
      group.BeginBatch();

      for (int i = 0; i < 3; ++i) {
        float base = 10.f + static_cast<float>(i) * 15.f;
        float jitter =
            base * 0.3f *
            static_cast<float>(std::sin(t + static_cast<double>(i) * 1.2));
        float v = base + jitter;

        rows[i].current.Set(v);
        rows[i].history.Update([v](std::vector<float>& h) {
          h.erase(h.begin());
          h.push_back(v);
        });
      }

      group.EndBatch();
    }
  });

  // ── UI Component ──────────────────────────────────────────────────────────
  const Theme& t = GetTheme();

  auto ui = Renderer([&]() -> Element {
    // ── Counter section ────────────────────────────────────────────────────
    Element counter_section =
        vbox({
            text(" Counter ") | bold | color(t.primary) | hcenter,
            separatorLight(),
            separatorEmpty(),
            hbox({
                text("  Value : ") | dim,
                text(std::to_string(*counter)) | bold | color(t.accent),
            }),
            hbox({
                text("  Label : ") | dim,
                text(*counter_label) | color(t.secondary),
            }),
            separatorEmpty(),
            text("  Incremented every 500 ms by a background thread.") | dim,
            separatorEmpty(),
        }) |
        borderStyled(t.border_style, t.border_color);

    // ── Latency section ────────────────────────────────────────────────────
    std::vector<Element> latency_rows;
    latency_rows.push_back(hbox({
        text(PadRight("Service", 18)) | bold | color(t.text_muted),
        text(PadRight("ms", 8)) | bold | color(t.text_muted),
        text("Trend (last 40 samples)") | bold | color(t.text_muted),
    }));
    latency_rows.push_back(separatorLight());

    for (int i = 0; i < 3; ++i) {
      float cur = rows[i].current.Get();
      const std::vector<float>& hist = rows[i].history.Get();

      // Choose colour by latency magnitude.
      Color val_color = (cur < 20.f)   ? t.success_color
                        : (cur < 35.f) ? t.warning_color
                                       : t.error_color;

      latency_rows.push_back(hbox({
          text(PadRight(rows[i].name, 18)) | color(t.text),
          text(PadRight(std::to_string(static_cast<int>(cur)) + "ms", 8)) |
              bold | color(val_color),
          Sparkline(hist, 36, val_color),
      }));
    }
    latency_rows.push_back(separatorEmpty());
    latency_rows.push_back(
        text("  Updated every 200 ms; uses ReactiveGroup batch to fire "
             "one redraw.") |
        dim);
    latency_rows.push_back(separatorEmpty());

    Element latency_section =
        vbox({
            text(" Network Latency ") | bold | color(t.primary) | hcenter,
            separatorLight(),
            separatorEmpty(),
            vbox(std::move(latency_rows)) | hcenter,
        }) |
        borderStyled(t.border_style, t.border_color);

    // ── Controls ────────────────────────────────────────────────────────────
    Element controls = hbox({
                           text(" q / Esc ") | bold | color(t.secondary),
                           text(" quit   ") | dim,
                           text(" r ") | bold | color(t.secondary),
                           text(" reset counter ") | dim,
                       }) |
                       hcenter;

    return vbox({
               text("  ui::Reactive<T> Demo  ") | bold | color(t.primary) |
                   hcenter,
               separatorEmpty(),
               counter_section,
               separatorEmpty(),
               latency_section,
               separatorEmpty(),
               controls,
           }) |
           size(WIDTH, EQUAL, 70) | center;
  });

  // ── Key bindings ──────────────────────────────────────────────────────────
  ui |= CatchEvent([&](Event event) -> bool {
    if (event == Event::Character('q') || event == Event::Escape) {
      if (App* app = App::Active()) {
        app->Exit();
      }
      return true;
    }
    if (event == Event::Character('r')) {
      counter.Set(0);
      return true;
    }
    return false;
  });

  // ── Run ───────────────────────────────────────────────────────────────────
  RunFullscreen(ui);

  // ── Cleanup ───────────────────────────────────────────────────────────────
  running.store(false);
  counter_thread.join();
  latency_thread.join();

  return 0;
}
