<p align="center">
  <a href="#"><img src="https://img.shields.io/badge/c++-%2300599C.svg?style=flat&logo=c%2B%2B&logoColor=white" alt="C++"></a>
  <a href="http://opensource.org/licenses/MIT"><img src="https://img.shields.io/github/license/louiscrocker/FTXUI_BADWOLF?color=black" alt="MIT License"></a>
  <a href="https://github.com/louiscrocker/FTXUI_BADWOLF/actions"><img src="https://github.com/louiscrocker/FTXUI_BADWOLF/actions/workflows/build.yaml/badge.svg" alt="Build"></a>
  <a href="https://codecov.io/gh/louiscrocker/FTXUI_BADWOLF"><img src="https://codecov.io/gh/louiscrocker/FTXUI_BADWOLF/branch/master/graph/badge.svg?token=C41FdRpNVA" alt="Coverage"></a>
  <a href="https://github.com/louiscrocker/FTXUI_BADWOLF/stargazers"><img src="https://img.shields.io/github/stars/louiscrocker/FTXUI_BADWOLF" alt="Stars"></a>
  <a href="https://repology.org/project/ftxui/versions"><img src="https://repology.org/badge/latest-versions/ftxui.svg" alt="Packaged versions"></a>
</p>

# FTXUI BadWolf — The C++ Terminal UI Framework

```
     ████████████████████████████████████████████████████████████
     ██  ⣾⣷⣦⡀  ⣶⣶  ⣶⣶⣶  ⣶⣶⣶  ⣠⣶⣶⣦   ⣶  ⣶  ⣶  ██  ┌─ FTXUI ─────────────────┐
     ██ ⣿⣿⣿⣿⣿  ──  ─────  ────  ────  ⣿⣿⣿⣿⣿  ██  │  ✅ Charts & Braille art  │
     ██  ⠛⠉⠉⠉   Braille Globe            ⠙⠋    ██  │  ✅ 3D Spinning Globe     │
     ██ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  ██  │  ✅ GeoJSON World Maps    │
     ██ ⡤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⡄  ██  │  ✅ Markdown Renderer     │
     ██ ⡇ ╭──────────────────────────╮     ⡇  ██  │  ✅ Reactive State        │
     ██ ⡇ │  ▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░  │     ⡇  ██  │  ✅ Process Runner        │
     ██ ⡇ │  Progress: 62%           │     ⡇  ██  │  ✅ 6 Built-in Themes     │
     ██ ⡇ ╰──────────────────────────╯     ⡇  ██  │  ✅ Zero dependencies     │
     ████████████████████████████████████████████████████████████  └──────────────────────────┘
```

> Build beautiful terminal applications in C++ — inspired by React, as fast as bare metal.

**FTXUI BadWolf** is a cross-platform, zero-dependency C++ library for terminal UIs. Its functional
style (declarative DOM + reactive components) makes complex TUIs feel simple to write.
The new high-level `ftxui::ui` layer cuts boilerplate to near zero — a full interactive form
in 10 lines of code.

---

## Why FTXUI BadWolf?

| Feature | **FTXUI BadWolf** | ncurses | Bubbletea | Textual | Ratatui |
|---|---|---|---|---|---|
| Language | **C++** | C | Go | Python | Rust |
| Hello world (lines) | **~10** | ~35 | ~30 | ~12 | ~30 |
| Startup time | **<1 ms** | <1 ms | ~10 ms | ~200 ms | ~5 ms |
| Memory (idle) | **~2 MB** | ~1 MB | ~8 MB | ~50 MB | ~4 MB |
| Zero dependencies | **✅** | ❌ | ❌ | ❌ | ❌ |
| Reactive state | **✅** | ❌ | ✅ | ✅ | ✅ |
| Braille charts | **✅** | ❌ | ❌ | ✅ | ✅ |
| 3D spinning globe | **✅** | ❌ | ❌ | ❌ | ❌ |
| GeoJSON maps | **✅** | ❌ | ❌ | ❌ | ❌ |
| Markdown renderer | **✅** | ❌ | ❌ | ✅ | ❌ |
| Process runner | **✅** | ❌ | ❌ | ✅ | ❌ |
| Mouse support | **✅** | ✅ | ✅ | ✅ | ✅ |
| WebAssembly | **✅** | ❌ | ❌ | ❌ | ❌ |
| C++20 modules | **✅** | ❌ | N/A | N/A | ❌ |
| High-level API | **✅** | ❌ | ✅ | ✅ | ✅ |

---

## 30-Second Quick Start

**CMakeLists.txt:**
```cmake
include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/louiscrocker/FTXUI_BADWOLF
  GIT_TAG main
)
```

**main.cpp — a full interactive form:**
```cpp
#include <string>
#include "ftxui/ui.hpp"
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Nord());
  std::string name, email;
  auto form = Form()
    .Title("Sign Up")
    .Field("Name",  &name)
    .Field("Email", &email)
    .Submit("OK", []{ App::Active()->Exit(); });
  RunFullscreen(form.Build());
}
```

That's it. No `initscr()`, no `endwin()`, no manual `refresh()`.

---

## Feature Showcase

### 📊 Charts & Visualizations

Braille-rendered charts at sub-character resolution. All chart types share a fluent builder API.

```cpp
#include "ftxui/ui.hpp"
using namespace ftxui::ui;

// Multi-series line chart
auto chart = LineChart()
  .Title("CPU Usage")
  .Series("core0", data0, Color::Cyan)
  .Series("core1", data1, Color::Yellow)
  .Domain(0.f, 60.f).Range(0.f, 100.f)
  .ShowLegend(true).ShowAxes(true)
  .Build();

// Also: BarChart(), ScatterPlot(), Histogram(), Sparkline()
```

### 🌍 GeoJSON Maps & Globe

Render any GeoJSON data as a terminal map. Or spin a 3D globe — in braille.

```cpp
// Interactive world map with zoom/pan
auto geo = LoadGeoJSON("world.geojson");
auto map = GeoMap()
  .Data(geo)
  .Projection(Projection::Mercator)
  .ShowGraticule(true)
  .Build();

// 3D spinning globe
auto globe = GlobeMap()
  .LineColor(Color::GreenLight)
  .RotationSpeed(0.8)
  .Build();
```

### 🔄 Reactive State

`State<T>` wraps any value. Any mutation automatically triggers a redraw — no manual refresh calls.

```cpp
State<int> count(0);
State<std::string> status("idle");

auto ui = Renderer([&] {
  return vbox({
    text("Count: " + std::to_string(*count)) | bold,
    text("Status: " + *status),
  }) | border | center;
});
ui |= CatchEvent([&](Event e) {
  if (e == Event::ArrowUp) { count.Mutate([](int& v){ v++; }); return true; }
  return false;
});
RunFullscreen(ui);
```

### 🏗️ MVU Architecture

Full Elm/Bubbletea-style Model-View-Update pattern, typed and pure.

```cpp
struct Model { int count = 0; };
enum class Msg { Inc, Dec, Quit };

Model Update(Model m, Msg msg) {
  switch (msg) {
    case Msg::Inc:  m.count++; break;
    case Msg::Dec:  m.count--; break;
    case Msg::Quit: App::Active()->Exit(); break;
  }
  return m;
}

Element View(const Model& m) {
  return text(std::to_string(m.count)) | border | center;
}
```

### 📝 Markdown Renderer

Live-render Markdown in your terminal UI. Bold, italic, code blocks, tables, and links.

```cpp
auto preview = MarkdownComponent(markdown_source_string);
RunFullscreen(preview);
```

### ⚡ Process Runner

Stream subprocess stdout/stderr into a scrollable log panel.

```cpp
auto panel = ProcessPanel::Create();
panel->SetCommand("cmake --build . --parallel 8");
panel->Start();
RunFullscreen(panel->AsComponent());
```

### 📋 DataTable & Virtual List

Sortable, filterable tables and lists over arbitrary data types.

```cpp
struct Employee { std::string name, dept; int salary; };
std::vector<Employee> staff = { /* ... */ };

auto table = DataTable<Employee>()
  .Column("Name",   [](auto& e){ return e.name; })
  .Column("Dept",   [](auto& e){ return e.dept; })
  .Column("Salary", [](auto& e){ return "$" + std::to_string(e.salary); })
  .Data(&staff)
  .Sortable(true)
  .Build();
```

### 🎨 6 Built-in Themes

One call switches every component in your app.

```cpp
SetTheme(Theme::Nord());      // calm blue
SetTheme(Theme::Dracula());   // classic purple-dark
SetTheme(Theme::Monokai());   // warm editor colors
SetTheme(Theme::Dark());      // high-contrast dark
SetTheme(Theme::Light());     // light background
SetTheme(Theme::Default());   // cyan accent
```

Or build your own:
```cpp
SetTheme(Theme::Dark()
  .WithPrimary(Color::Magenta)
  .WithBorderStyle(ROUNDED));
```

### 🗂️ More High-Level Components

| Component | Description |
|---|---|
| `Form` | Fluent form builder: text, password, checkbox, radio, select, integer |
| `Wizard` | Multi-step guided flow with progress indicator |
| `Router` | Named view navigation with push/pop history |
| `CommandPalette` | Ctrl+P fuzzy command launcher |
| `FilePicker` | OS filesystem browser with keyboard navigation |
| `Tree` | Collapsible tree-view |
| `ConfigEditor` | Persistent key-value settings editor |
| `Keymap` | Keybinding registry with auto-generated help view |
| `WithNotifications` | Thread-safe toast overlay with auto-dismiss |
| `WithModal` / `WithDrawer` | Overlays and slide-in panels |
| `TabView` / `HSplit` / `VSplit` | Layout containers |
| `LogPanel` | Thread-safe scrolling log |
| `BackgroundTask` | `RunAsync<T>` thread-safe async with UI-thread callback |

---

## Installation

### CMake FetchContent (recommended)

```cmake
include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/louiscrocker/FTXUI_BADWOLF
  GIT_TAG main   # or a specific release tag
)
FetchContent_MakeAvailable(ftxui)

# Link to what you need:
target_link_libraries(my_app PRIVATE
  ftxui::ui        # high-level API (pulls in component, dom, screen)
  # ftxui::component  # interactive components only
  # ftxui::dom        # layout / elements only
  # ftxui::screen     # terminal rendering only
)
```

### vcpkg

```
vcpkg install ftxui
```

### Conan

```
conan install ftxui/6.1.9
```

### Package Managers

| Platform | Command |
|---|---|
| Debian/Ubuntu | `apt install libftxui-dev` |
| Arch Linux (AUR) | `yay -S ftxui` |
| OpenSUSE | `zypper install ftxui` |
| Nix | `nix-shell -p ftxui` |
| XMake | `xrepo install ftxui` |
| Bazel | See [MODULE.bazel](MODULE.bazel) |

### C++20 Modules

```cpp
import ftxui;
import ftxui.component;
import ftxui.dom;
import ftxui.screen;
```

Requires CMake 3.28+, Ninja generator, and a recent Clang/GCC/MSVC compiler.

---

## Architecture

### Dependency Stack

```
┌─────────────────────────────────────────────────────────────┐
│                    Your Application                          │
└──────────────────────────┬──────────────────────────────────┘
                           │ #include "ftxui/ui.hpp"
┌──────────────────────────▼──────────────────────────────────┐
│                  ftxui::ui  (BadWolf Layer)                   │
│                                                              │
│  Text & Richness    │  Data           │  AI & Intelligence   │
│  ─ TypewriterText   │  ─ JsonValue    │  ─ NLParser          │
│  ─ RichText         │  ─ Json::Parse  │  ─ UIGenerator       │
│  ─ AnsiParser       │  ─ JsonPath     │  ─ LLMAdapter        │
│                     │  ─ ReactiveJson │                      │
│  Reactive & Binding │  Visualization │  Networking           │
│  ─ Reactive<T>      │  ─ BrailleCanvas│  ─ CollabServer      │
│  ─ Bind<T>          │  ─ LineChart    │  ─ NetworkReactive   │
│  ─ ReactiveList<T>  │  ─ GeoMap       │  ─ LiveSource<T>     │
│  ─ ReactiveJson     │  ─ GlobeMap     │                      │
│                     │  ─ GalaxyMap    │  Animation           │
│  Themes             │               │  ─ AnimationLoop      │
│  ─ LCARS / Imperial │  WebAssembly  │  ─ Tween              │
│  ─ Enterprise/Rebel │  ─ WebGLRenderer│ ─ ParticleSystem    │
│  ─ Matrix/Dracula   │  ─ WasmBridge  │                      │
└─────────────────────┴───────────────┴──────────────────────┘
                           │ links against
┌──────────────────────────▼──────────────────────────────────┐
│              ftxui::component                                │
│  Interactive components, event loop, App runner              │
└──────────────────────────┬──────────────────────────────────┘
                           │ links against
┌──────────────────────────▼──────────────────────────────────┐
│                   ftxui::dom                                 │
│  Layout engine, Elements, Canvas, Flexbox                    │
└──────────────────────────┬──────────────────────────────────┘
                           │ links against
┌──────────────────────────▼──────────────────────────────────┐
│                  ftxui::screen                               │
│  Terminal I/O, Color, ANSI rendering, String utilities       │
└─────────────────────────────────────────────────────────────┘
```

### Distribution Options

| Method | Command | Best For |
|--------|---------|----------|
| **CMake FetchContent** | See below | Most projects |
| **vcpkg** | `vcpkg install ftxui-badwolf` | Enterprise / Windows |
| **Conan** | `conan install ftxui-badwolf/1.0.0` | Conan users |
| **Single-header** | `#define BADWOLF_IMPLEMENTATION` + `#include "dist/ftxui.hpp"` | Scripts / embedding |
| **Homebrew** | `brew install ftxui-badwolf` | macOS quick install |

### Single-Header Usage

```cpp
// In exactly ONE .cpp file in your project:
#define BADWOLF_IMPLEMENTATION
#include "ftxui/badwolf.hpp"

// In all other files (optional):
#include "ftxui/badwolf.hpp"
```

You can use any layer independently, or build from the top down with `ftxui::ui`.

---

## Documentation

| Resource | Link |
|---|---|
| API Reference | [louiscrocker.github.io/FTXUI_BADWOLF](https://louiscrocker.github.io/FTXUI_BADWOLF/) |
| Getting Started | [doc/getting-started.md](doc/getting-started.md) |
| Cookbook (30+ recipes) | [doc/cookbook.md](doc/cookbook.md) |
| High-Level UI Layer | [doc/ui-layer.md](doc/ui-layer.md) |
| Migrating from ncurses | [doc/migration-ncurses.md](doc/migration-ncurses.md) |
| Migrating from Bubbletea | [doc/migration-bubbletea.md](doc/migration-bubbletea.md) |
| Migrating from Textual | [doc/migration-textual.md](doc/migration-textual.md) |
| Migrating from Ratatui | [doc/migration-ratatui.md](doc/migration-ratatui.md) |
| Live Examples (WASM) | [louiscrocker.github.io/FTXUI_BADWOLF/examples](https://louiscrocker.github.io/FTXUI_BADWOLF/examples/) |
| CMake Starter Project | [ftxui-starter](https://github.com/ArthurSonzogni/ftxui-starter) |
| Bazel Starter Project | [ftxui-bazel](https://github.com/ArthurSonzogni/ftxui-bazel) |

Available in: [English](https://louiscrocker.github.io/FTXUI_BADWOLF/) · [Français](https://louiscrocker.github.io/FTXUI_BADWOLF/fr/) · [Español](https://louiscrocker.github.io/FTXUI_BADWOLF/es/) · [繁體中文](https://louiscrocker.github.io/FTXUI_BADWOLF/zh-TW/) · [简体中文](https://louiscrocker.github.io/FTXUI_BADWOLF/zh-CH/) · [日本語](https://louiscrocker.github.io/FTXUI_BADWOLF/ja/)

---

## Core DOM API (Low-Level)

For those who want fine-grained control, the DOM layer is always available:

```cpp
#include "ftxui/dom/elements.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/app.hpp"
using namespace ftxui;

int main() {
  auto document = vbox({
    hbox({ text("one") | border, text("two") | border | flex }),
    gauge(0.62f) | color(Color::Cyan),
    hbox({ text("Press "), text("q") | bold, text(" to quit") }),
  });

  auto screen = App::TerminalOutput();
  screen.Loop(Renderer([&]{ return document; }));
}
```

**DOM features at a glance:**

<details><summary>Layout</summary>

- `hbox` / `vbox` / `gridbox` / `flexbox` — arrange elements
- `flex` / `flex_grow` / `flex_shrink` — responsive sizing
- `filler()` — push elements apart

</details>

<details><summary>Styling</summary>

- `bold` · `italic` · `dim` · `inverted` · `underlined` · `blink` · `strikethrough`
- `color(Color::X)` · `bgcolor(Color::X)` — 256-color and true-color support
- `hyperlink("url")` — clickable links in supporting terminals

</details>

<details><summary>Borders & Separators</summary>

- `border` · `borderStyled(ROUNDED)` · `borderDouble` · `borderHeavy`
- `separator()` · `separatorLight()` · `separatorHeavy()`

</details>

<details><summary>Advanced</summary>

- `canvas(w, h, fn)` — braille pixel canvas
- `table(cells)` — grid table with full cell styling
- `paragraph(text)` — auto-wrapping text
- `scrollable` — scrollable regions

</details>

---

## Platform Support

| Platform | Status |
|---|---|
| Linux | ✅ Primary target |
| macOS | ✅ Primary target |
| Windows | ✅ (via contributors) |
| WebAssembly | ✅ |
| FreeBSD | ✅ |

---

## WebAssembly

Any BadWolf app can be compiled to WebAssembly and run directly in a browser.

```bash
# Configure with Emscripten
emcmake cmake -S . -B build-wasm -DFTXUI_BUILD_WASM=ON
cmake --build build-wasm
```

Then serve the output with the required COOP/COEP headers (e.g. using
`examples/run_webassembly.py`) and open `badwolf_playground.html` in a browser.

See the [live playground](docs/playground.html) and
[cmake/ftxui_wasm.cmake](cmake/ftxui_wasm.cmake) for full details.

---

## Contributing

Contributions are welcome! Please read the [contributing guide](.github/CONTRIBUTING.md) and open a PR.

- 🐛 [Report a bug](.github/ISSUE_TEMPLATE/bug_report.md)
- 💡 [Request a feature](.github/ISSUE_TEMPLATE/feature_request.md)
- 📖 [Improve documentation](doc/)

---

## License

MIT © [Arthur Sonzogni](https://github.com/ArthurSonzogni) and contributors.

See [LICENSE](LICENSE) for the full text.
