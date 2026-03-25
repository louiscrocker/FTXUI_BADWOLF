// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file constraint_demo.cpp
/// Demonstrates the ftxui::ui constraint layout engine:
///   ConstraintRow, ConstraintGrid, ConstraintBox, AspectRatio, Stack

#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/constraint_layout.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Helpers
// ───────────────────────────────────────────────────────────────────

static Element LabelBox(const std::string& label, Color bg) {
  return text(label) | bold | color(Color::White) | bgcolor(bg) | center;
}

// ── Tab 1: Responsive
// ─────────────────────────────────────────────────────────

static Component MakeResponsiveTab() {
  return Renderer([]() -> Element {
    int w = Terminal::Size().dimx;

    auto narrow = vbox({
        LabelBox("[ narrow — 1 col ]", Color::Blue),
        text("Resize the terminal to see the layout change."),
    });

    auto medium = hbox({
        LabelBox("Left", Color::Green) | size(WIDTH, EQUAL, w / 2),
        LabelBox("Right", Color::Cyan) | size(WIDTH, EQUAL, w / 2),
    });

    auto wide = hbox({
        LabelBox("Col 1", Color::Red) | size(WIDTH, EQUAL, w / 3),
        LabelBox("Col 2", Color::Green) | size(WIDTH, EQUAL, w / 3),
        LabelBox("Col 3", Color::Blue) | size(WIDTH, EQUAL, w / 3),
    });

    auto content = Responsive().XS(narrow).SM(medium).MD(wide).Build();

    return vbox({
        text("Responsive Layout") | bold | underlined,
        text(""),
        text("XS: 1 col  |  SM (60+): 2 cols  |  MD (80+): 3 cols"),
        text(""),
        content,
    });
  });
}

// ── Tab 2: Grid
// ───────────────────────────────────────────────────────────────

static Component MakeGridTab() {
  return Renderer([]() -> Element {
    ConstraintGrid grid;
    grid.Columns({SizeConstraint::Chars(20), SizeConstraint::Percent(30),
                  SizeConstraint::Fill()});
    grid.Gap(1, 0);
    grid.Add(borderLight(LabelBox("Chars(20)", Color::Red)));
    grid.Add(borderLight(LabelBox("Percent(30)", Color::Green)));
    grid.Add(borderLight(LabelBox("Fill", Color::Blue)));
    grid.Add(borderLight(LabelBox("Chars(20)", Color::Magenta)));
    grid.Add(borderLight(LabelBox("Percent(30)", Color::Yellow)));
    grid.Add(borderLight(LabelBox("Fill", Color::Cyan)));

    return vbox({
        text("ConstraintGrid") | bold | underlined,
        text(""),
        text("Columns: Chars(20) | Percent(30) | Fill"),
        text(""),
        grid.Build(),
    });
  });
}

// ── Tab 3: ConstraintBox
// ──────────────────────────────────────────────────────

static Component MakeConstraintsTab() {
  auto min_w = std::make_shared<int>(10);
  auto max_w = std::make_shared<int>(60);

  auto min_slider = Slider("Min Width", min_w.get(), 1, 80, 1);
  auto max_slider = Slider("Max Width", max_w.get(), 1, 80, 1);

  auto demo = Renderer([min_w, max_w]() -> Element {
    Constraints c;
    c.width = SizeConstraint::Fill();
    c.min_width = *min_w;
    c.max_width = *max_w;
    auto inner = borderLight(text("ConstraintBox") | hcenter);
    return ConstraintBox(inner, c);
  });

  return Container::Vertical({
      min_slider,
      max_slider,
      Renderer([]() -> Element { return text(""); }),
      demo,
      Renderer([min_w, max_w]() -> Element {
        return text("min_width=" + std::to_string(*min_w) +
                    "  max_width=" + std::to_string(*max_w));
      }),
  });
}

// ── Tab 4: AspectRatio
// ────────────────────────────────────────────────────────

static Component MakeAspectRatioTab() {
  return Renderer([]() -> Element {
    auto inner = borderRounded(vbox(
        {filler(), text("16 : 9 aspect ratio") | bold | hcenter, filler()}));

    return vbox({
        text("AspectRatio(element, 16, 9)") | bold | underlined,
        text(""),
        text("Terminal cell height ≈ 2× width, so rows ≈ width×(9/16)/2"),
        text(""),
        AspectRatio(inner, 16, 9),
    });
  });
}

// ── Tab 5: Stack
// ──────────────────────────────────────────────────────────────

static Component MakeStackTab() {
  return Renderer([]() -> Element {
    int w = std::min(40, Terminal::Size().dimx);
    int h = 8;

    auto background = text(std::string(w, '#')) | color(Color::GrayDark) |
                      size(WIDTH, EQUAL, w) | size(HEIGHT, EQUAL, h);

    auto foreground = borderRounded(text("Foreground overlay") | bold |
                                    hcenter | color(Color::Yellow)) |
                      size(WIDTH, EQUAL, w - 4) | size(HEIGHT, EQUAL, 4);

    auto centered_fg =
        vbox({filler(), hbox({filler(), foreground, filler()}), filler()}) |
        size(WIDTH, EQUAL, w) | size(HEIGHT, EQUAL, h);

    return vbox({
        text("Stack — dbox overlay") | bold | underlined,
        text(""),
        text("Two layers rendered at the same position:"),
        text(""),
        Stack({background, centered_fg}),
    });
  });
}

// ── Main
// ──────────────────────────────────────────────────────────────────────

int main() {
  std::vector<std::string> tab_names = {"Responsive", "Grid", "Constraints",
                                        "Aspect Ratio", "Stack"};
  int tab_selected = 0;

  auto tabs = Container::Tab(
      {
          MakeResponsiveTab(),
          MakeGridTab(),
          MakeConstraintsTab(),
          MakeAspectRatioTab(),
          MakeStackTab(),
      },
      &tab_selected);

  auto tab_toggle = Toggle(&tab_names, &tab_selected);

  auto ui = Container::Vertical({
      tab_toggle,
      tabs,
  });

  auto renderer = Renderer(ui, [&]() -> Element {
    return vbox({
               text("  Constraint Layout Demo  ") | bold | hcenter,
               separatorLight(),
               tab_toggle->Render(),
               separatorLight(),
               tabs->Render() | flex,
               separatorLight(),
               text("q/Ctrl-C to quit") | dim | hcenter,
           }) |
           border;
  });

  App::Fullscreen().Loop(renderer);
  return 0;
}
