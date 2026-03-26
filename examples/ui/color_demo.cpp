// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file color_demo.cpp
/// Showcase of the FTXUI rich text and colorization system.
/// Tabs: Attributes | 16 Colors | 256 Colors | TrueColor | Rich Markup | ANSI

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/rich_text.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Tab 1: Attributes ────────────────────────────────────────────────────────

Element AttributesTab() {
  struct AttrDemo {
    std::string name;
    std::function<TextStyle&(TextStyle&)> apply;
  };

  std::vector<AttrDemo> attrs = {
      {"Bold", [](TextStyle& s) -> TextStyle& { return s.Bold(); }},
      {"Dim", [](TextStyle& s) -> TextStyle& { return s.Dim(); }},
      {"Italic", [](TextStyle& s) -> TextStyle& { return s.Italic(); }},
      {"Underline", [](TextStyle& s) -> TextStyle& { return s.Underline(); }},
      {"Blink", [](TextStyle& s) -> TextStyle& { return s.Blink(); }},
      {"Reverse", [](TextStyle& s) -> TextStyle& { return s.Reverse(); }},
      {"Strikethrough",
       [](TextStyle& s) -> TextStyle& { return s.Strikethrough(); }},
      {"Bold+Dim", [](TextStyle& s) -> TextStyle& { return s.Bold().Dim(); }},
      {"Bold+Underline",
       [](TextStyle& s) -> TextStyle& { return s.Bold().Underline(); }},
  };

  Elements rows;
  rows.push_back(hbox({text(" Attribute     ") | bold,
                       text(" Example             ") | bold}) |
                 color(Color::YellowLight));

  for (const auto& a : attrs) {
    TextStyle s;
    a.apply(s);
    s.Fg(Color::CyanLight);
    rows.push_back(hbox({
        text(" " + a.name) | size(WIDTH, EQUAL, 15),
        text(" "),
        s(a.name + " text example"),
    }));
  }
  return vbox(std::move(rows)) | border;
}

// ── Tab 2: 16 Colors ─────────────────────────────────────────────────────────

Element Colors16Tab() {
  static const std::pair<Color::Palette16, std::string> palette[] = {
      {Color::Black, "Black       "},
      {Color::Red, "Red         "},
      {Color::Green, "Green       "},
      {Color::Yellow, "Yellow      "},
      {Color::Blue, "Blue        "},
      {Color::Magenta, "Magenta     "},
      {Color::Cyan, "Cyan        "},
      {Color::GrayLight, "GrayLight   "},
      {Color::GrayDark, "GrayDark    "},
      {Color::RedLight, "RedLight    "},
      {Color::GreenLight, "GreenLight  "},
      {Color::YellowLight, "YellowLight "},
      {Color::BlueLight, "BlueLight   "},
      {Color::MagentaLight, "MagentaLight"},
      {Color::CyanLight, "CyanLight   "},
      {Color::White, "White       "},
  };

  Elements rows;
  rows.push_back(hbox({text(" Swatch") | bold, text("  Name           ") | bold,
                       text("  BgSwatch") | bold}) |
                 color(Color::YellowLight));

  for (const auto& [c, name] : palette) {
    rows.push_back(hbox({
        text("  "),
        ColorSwatch(Color(c)),
        text("  " + name) | color(Color(c)),
        text("  "),
        bgcolor(Color(c), text("  " + name + "  ")),
    }));
  }
  return vbox(std::move(rows)) | border;
}

// ── Tab 3: 256 Colors ────────────────────────────────────────────────────────

Element Colors256Tab() {
  return vbox({
             text("256-color palette (16×16 grid)") | bold |
                 color(Color::YellowLight),
             separator(),
             ColorPalette256(),
         }) |
         border;
}

// ── Tab 4: TrueColor ─────────────────────────────────────────────────────────

Component TrueColorTab() {
  struct SliderState {
    int r = 255, g = 0, b = 128;
    int r2 = 0, g2 = 200, b2 = 255;
  };
  auto state = std::make_shared<SliderState>();

  auto slider_r = Slider("R1:", &state->r, 0, 255, 1);
  auto slider_g = Slider("G1:", &state->g, 0, 255, 1);
  auto slider_b = Slider("B1:", &state->b, 0, 255, 1);
  auto slider_r2 = Slider("R2:", &state->r2, 0, 255, 1);
  auto slider_g2 = Slider("G2:", &state->g2, 0, 255, 1);
  auto slider_b2 = Slider("B2:", &state->b2, 0, 255, 1);

  auto preview = Renderer([state] {
    Color c1 = Color::RGB(static_cast<uint8_t>(state->r),
                          static_cast<uint8_t>(state->g),
                          static_cast<uint8_t>(state->b));
    Color c2 = Color::RGB(static_cast<uint8_t>(state->r2),
                          static_cast<uint8_t>(state->g2),
                          static_cast<uint8_t>(state->b2));
    return vbox({
               text("TrueColor Gradients") | bold | color(Color::YellowLight),
               separator(),
               hbox({text("Red → Blue:   "),
                     ColorGradient(Color::RGB(255, 0, 0), Color::RGB(0, 0, 255),
                                   40)}),
               hbox({text("Green → Red:  "),
                     ColorGradient(Color::RGB(0, 255, 0), Color::RGB(255, 0, 0),
                                   40)}),
               hbox({text("Blue → White: "),
                     ColorGradient(Color::RGB(0, 0, 255),
                                   Color::RGB(255, 255, 255), 40)}),
               hbox({text("Custom C1→C2: "), ColorGradient(c1, c2, 40)}),
               separator(),
               text("Custom Color 1") | bold,
               hbox(
                   {ColorSwatch(c1), text("  RGB(" + std::to_string(state->r) +
                                          "," + std::to_string(state->g) + "," +
                                          std::to_string(state->b) + ")") |
                                         color(c1)}),
               text("Custom Color 2") | bold,
               hbox({ColorSwatch(c2),
                     text("  RGB(" + std::to_string(state->r2) + "," +
                          std::to_string(state->g2) + "," +
                          std::to_string(state->b2) + ")") |
                         color(c2)}),
           }) |
           border;
  });

  auto sliders = Container::Vertical({
      slider_r,
      slider_g,
      slider_b,
      slider_r2,
      slider_g2,
      slider_b2,
  });

  return Container::Vertical({
      sliders,
      preview,
  });
}

// ── Tab 5: Rich Markup
// ────────────────────────────────────────────────────────

Component RichMarkupTab() {
  auto markup = std::make_shared<std::string>(
      "[bold]Bold[/bold] [red]Red[/red] [green]Green[/green] "
      "[blue]Blue[/blue] [yellow]Yellow[/yellow] "
      "[bold][cyan]Bold Cyan[/][/] "
      "[underline]Underlined[/underline] "
      "[rgb(255,128,0)]Orange RGB[/rgb(255,128,0)]");

  auto input_option = InputOption::Default();
  auto input = Input(markup.get(), "Enter markup...", input_option);

  auto preview = Renderer([markup] {
    return vbox({
        text("Rendered output:") | bold | color(Color::YellowLight),
        separator(),
        RichText::Element(*markup),
    });
  });

  return Container::Vertical({
      Renderer([markup, input] {
        return vbox({
            text("Rich Markup Editor") | bold | color(Color::YellowLight),
            separator(),
            text("Supported tags: [bold] [dim] [italic] [underline] [blink]") |
                dim,
            text("  [red/green/blue/yellow/cyan/magenta/white/black]") | dim,
            text("  [bright_red] [bg_red] [rgb(r,g,b)] [bg_rgb(r,g,b)] [/]") |
                dim,
            separator(),
            hbox({text("Markup: "), input->Render()}),
        });
      }),
      preview,
  });
}

// ── Tab 6: ANSI Input ────────────────────────────────────────────────────────

Component AnsiInputTab() {
  auto ansi_text = std::make_shared<std::string>(
      "\033[1mBold\033[0m \033[31mRed\033[0m \033[32mGreen\033[0m "
      "\033[38;2;255;128;0mOrange TrueColor\033[0m");

  auto input = Input(ansi_text.get(), "Enter ANSI text...");

  auto preview = Renderer([ansi_text] {
    return vbox({
        text("Parsed ANSI output:") | bold | color(Color::YellowLight),
        separator(),
        AnsiParser::Element(*ansi_text),
        separator(),
        text("Stripped:") | bold,
        text(AnsiParser::Strip(*ansi_text)),
    });
  });

  return Container::Vertical({
      Renderer([ansi_text, input] {
        return vbox({
            text("ANSI Escape Parser") | bold | color(Color::YellowLight),
            separator(),
            text("SGR codes: \\033[1m bold  \\033[2m dim  \\033[3m italic") |
                dim,
            text("  \\033[31m-\\033[37m fg  \\033[41m-\\033[47m bg") | dim,
            text("  \\033[38;2;r;g;bm TrueColor fg") | dim,
            separator(),
            hbox({text("Input: "), input->Render()}),
        });
      }),
      preview,
  });
}

// ── Main
// ──────────────────────────────────────────────────────────────────────

int main() {
  auto screen = ScreenInteractive::Fullscreen();

  int tab = 0;
  const std::vector<std::string> tab_names = {
      "Attributes", "16 Colors",   "256 Colors",
      "TrueColor",  "Rich Markup", "ANSI Input",
  };

  auto tab_toggle = Toggle(&tab_names, &tab);

  auto truecolor_tab = TrueColorTab();
  auto markup_tab = RichMarkupTab();
  auto ansi_tab = AnsiInputTab();

  auto tab_container = Container::Tab(
      {
          Renderer([] { return AttributesTab(); }),
          Renderer([] { return Colors16Tab(); }),
          Renderer([] { return Colors256Tab(); }),
          truecolor_tab,
          markup_tab,
          ansi_tab,
      },
      &tab);

  auto main_component = Container::Vertical({
      tab_toggle,
      tab_container,
  });

  auto renderer = Renderer(main_component, [&] {
    return vbox({
               text(" FTXUI Color & Rich Text Demo ") | bold | inverted |
                   hcenter,
               tab_toggle->Render(),
               separator(),
               tab_container->Render() | flex,
               separator(),
               text(" [Q] Quit  [←/→] Switch Tab ") | dim | hcenter,
           }) |
           border;
  });

  auto with_quit = CatchEvent(renderer, [&](Event e) {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });

  screen.Loop(with_quit);
  return 0;
}
