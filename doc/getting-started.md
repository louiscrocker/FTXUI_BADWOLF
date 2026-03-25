# Getting Started with FTXUI

This tutorial walks you from zero to a themed, interactive TUI application with reactive
state and a chart — step by step. Each section builds on the previous one.

---

## Prerequisites

- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.12+
- No other dependencies — FTXUI is self-contained

---

## Step 1: Set up the project

Create a new directory with two files:

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.12)
project(my_tui)

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG main
)
FetchContent_MakeAvailable(ftxui)

add_executable(my_tui main.cpp)
target_link_libraries(my_tui PRIVATE ftxui::ui)
target_compile_features(my_tui PRIVATE cxx_std_17)
```

**main.cpp:**
```cpp
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  auto ui = Renderer([]{ return text("Hello, FTXUI!") | center; });
  RunFullscreen(ui);
}
```

**Build and run:**
```sh
cmake -S . -B build
cmake --build build
./build/my_tui
# Press Ctrl+C to exit
```

You should see "Hello, FTXUI!" centered in your terminal.

---

## Step 2: Add quit and keyboard handling

FTXUI uses `CatchEvent` to handle input. Layer it onto your component with `|=`.

```cpp
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  auto ui = Renderer([]{ return text("Press [q] to quit") | center; });
  ui |= CatchEvent([](Event e) {
    if (e.is_character("q")) {
      App::Active()->Exit();
      return true;  // consumed
    }
    return false;   // pass through
  });
  RunFullscreen(ui);
}
```

---

## Step 3: Add a button

Use the high-level `Form` builder, or use low-level `Button` directly:

```cpp
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  int clicks = 0;

  auto btn = Button("Click me!", [&]{ clicks++; });

  auto ui = Renderer(btn, [&]{
    return vbox({
      text("Clicks: " + std::to_string(clicks)) | bold | hcenter,
      separator(),
      btn->Render() | hcenter,
    }) | border | center;
  });

  RunFullscreen(ui);
}
```

`Renderer(component, render_fn)` — the first argument is a component whose focus/events
are managed. The second is your pure render function.

---

## Step 4: Use a Form

The `Form` builder creates text fields, checkboxes, dropdowns, and buttons automatically.

```cpp
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  std::string name, email;
  bool subscribe = false;
  int plan = 0;
  const std::vector<std::string> plans = {"Free", "Pro", "Enterprise"};

  auto form = Form()
    .Title("Create Account")
    .Field("Name",    &name,  "Alice Smith")
    .Field("Email",   &email, "you@example.com")
    .Check("Subscribe to newsletter", &subscribe)
    .Select("Plan", &plans, &plan)
    .Submit("Sign Up", [&]{ App::Active()->Exit(); })
    .Cancel("Cancel",  [&]{ App::Active()->Exit(); });

  RunFullscreen(form.Build());
}
```

Tab/Shift+Tab navigate between fields. Enter activates buttons.

---

## Step 5: Add reactive state

`State<T>` is a reactive wrapper — any write triggers a redraw automatically.

```cpp
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  State<int> counter(0);
  State<std::string> label("Press [+] or [-]");

  auto ui = Renderer([&]{
    return vbox({
      text(*label) | hcenter | dim,
      text(std::to_string(*counter)) | bold | color(Color::Cyan) | hcenter,
      text("[+] inc  [-] dec  [q] quit") | hcenter | dim,
    }) | border | center;
  });

  ui |= CatchEvent([&](Event e) {
    if (!e.is_character()) return false;
    if (e.character() == "+") {
      counter.Mutate([](int& v){ v++; });
      label.Set("Incremented!");
      return true;
    }
    if (e.character() == "-") {
      counter.Mutate([](int& v){ v--; });
      label.Set("Decremented!");
      return true;
    }
    if (e.character() == "q") {
      App::Active()->Exit();
      return true;
    }
    return false;
  });

  RunFullscreen(ui);
}
```

---

## Step 6: Add a chart

Charts use braille rendering — multiple pixels per terminal character cell.

```cpp
#include <cmath>
#include <utility>
#include <vector>
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  // Generate sine wave data
  std::vector<std::pair<float, float>> data;
  for (int i = 0; i <= 100; i++) {
    float x = i / 100.f * 2.f * 3.14159f;
    data.emplace_back(x, std::sin(x));
  }

  auto chart = LineChart()
    .Title("sin(x)")
    .Series("sin", data, Color::Cyan)
    .Domain(0.f, 6.28f)
    .Range(-1.2f, 1.2f)
    .ShowAxes(true)
    .ShowLegend(true)
    .Build();

  auto ui = vbox({
    text("Chart Demo — [q] quit") | bold | hcenter,
    separator(),
    chart->Render() | flex,
  });

  auto root = Renderer(chart, [&]{ return ui; });
  root |= CatchEvent([](Event e){
    if (e.is_character("q")) { App::Active()->Exit(); return true; }
    return false;
  });

  RunFullscreen(root);
}
```

---

## Step 7: Apply a theme

One call before anything else changes every component in your application.

```cpp
int main() {
  SetTheme(Theme::Dracula()); // pick: Default, Dark, Light, Nord, Dracula, Monokai

  // ... rest of your app
}
```

Or build a custom theme:
```cpp
SetTheme(Theme::Dark()
  .WithPrimary(Color::Magenta)
  .WithBorderStyle(ROUNDED)
  .WithAnimations(true));
```

---

## Step 8: Full app with layout

Combine form, chart, and layout:

```cpp
#include <cmath>
#include <utility>
#include <vector>
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Nord());

  // Reactive model
  State<float> freq(1.0f);
  State<std::string> wave("sin");

  // Build chart from current state
  auto make_chart = [&]() -> Component {
    std::vector<std::pair<float,float>> pts;
    for (int i = 0; i <= 200; i++) {
      float x = i / 200.f * 4.f * 3.14159f;
      float y = (*wave == "sin") ? std::sin(*freq * x) : std::cos(*freq * x);
      pts.emplace_back(x, y);
    }
    return LineChart().Title("Wave").Series(*wave, pts, Color::Cyan)
      .Domain(0.f, 12.57f).Range(-1.2f, 1.2f)
      .ShowAxes(true).Build();
  };

  float freq_val = 1.0f;
  int wave_idx   = 0;
  const std::vector<std::string> waves = {"sin", "cos"};

  auto ctrl = Form()
    .Title("Controls")
    .Float("Frequency", &freq_val, 0.1f, 5.f, 0.1f)
    .Select("Wave", &waves, &wave_idx)
    .Submit("Apply", [&]{
      freq.Set(freq_val);
      wave.Set(waves[wave_idx]);
    })
    .Build();

  auto chart_area = Renderer([&]{ return make_chart()->Render() | flex; });

  auto root = HSplit(
    Panel("Chart",    chart_area),
    Panel("Controls", ctrl),
    60   // chart gets 60% width
  );

  root |= CatchEvent([](Event e){
    if (e.is_character("q")) { App::Active()->Exit(); return true; }
    return false;
  });

  RunFullscreen(root);
}
```

---

## Next steps

| Topic | Resource |
|---|---|
| 30+ copy-paste recipes | [Cookbook](cookbook.md) |
| Full API reference | [doc/ui-layer.md](ui-layer.md) |
| Coming from ncurses | [migration-ncurses.md](migration-ncurses.md) |
| Coming from Bubbletea | [migration-bubbletea.md](migration-bubbletea.md) |
| Coming from Textual | [migration-textual.md](migration-textual.md) |
| Coming from Ratatui | [migration-ratatui.md](migration-ratatui.md) |
| Live WASM examples | [arthursonzogni.github.io/FTXUI/examples](https://arthursonzogni.github.io/FTXUI/examples/) |
