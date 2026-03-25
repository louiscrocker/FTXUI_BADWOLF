# pyftxui — Python bindings for FTXUI

`pyftxui` brings the [FTXUI](https://github.com/ArthurSonzogni/ftxui) C++
terminal UI library to Python via pybind11, giving you a clean Pythonic API
for building rich TUIs.

## Quick start

```python
from pyftxui import App, theme
import pyftxui.reactive as rx
import pyftxui.components as ui

counter = rx.Reactive(0)
label = rx.Computed(counter, lambda v: f"Count: {v}")

app = App()
app.theme(theme.NORD)

def view():
    return ui.border(ui.vbox([
        ui.text(label.get(), bold=True),
        ui.hbox([
            ui.button("+ Increment", on_click=lambda: counter.set(counter.get() + 1)),
            ui.button("- Decrement", on_click=lambda: counter.set(counter.get() - 1)),
            ui.button("Quit",        on_click=app.quit),
        ]),
    ]))

app.run(view)
```

## Building

### Prerequisites

- CMake ≥ 3.15
- C++20 compiler (Clang or GCC)
- Python 3.8+ with development headers
- Internet access (pybind11 is fetched via CMake FetchContent on first build)

### Build the extension

```bash
# From the FTXUI repository root:
cmake -S . -B build \
      -DFTXUI_BUILD_PYTHON_BINDINGS=ON \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8 --target _ftxui
```

The compiled `_ftxui*.so` lands in `python/pyftxui/`.

### Run examples

```bash
cd python
python examples/counter.py
python examples/dashboard.py
python examples/markdown_viewer.py
```

## API overview

### `pyftxui.reactive`

| Symbol | Description |
|--------|-------------|
| `Reactive(initial)` | Observable value (`int`, `float`, or `str`). Methods: `get()`, `set(v)`, `on_change(fn)`. |
| `Computed(source, fn)` | Read-only derived value that updates whenever *source* changes. |

### `pyftxui.components` (DOM elements)

| Function | Description |
|----------|-------------|
| `text(s, bold=, dim=, italic=, color=)` | Styled text element |
| `hbox(elements)` | Horizontal layout |
| `vbox(elements)` | Vertical layout |
| `border(el)` / `border_rounded(el)` | Draw a border around element |
| `separator()` | Horizontal/vertical divider |
| `flex(el)` | Make element fill available space |
| `gauge(progress)` | Progress bar (0.0–1.0) |
| `paragraph(s)` | Word-wrapped text |
| `spinner(idx, frame)` | Animated spinner character |
| `markdown(md)` | Render Markdown string as Element |

### `pyftxui.components` (interactive components)

| Function | Returns | Description |
|----------|---------|-------------|
| `button(label, on_click)` | `Component` | Clickable button |
| `renderer(fn)` | `Component` | Calls `fn()` on each render |
| `input(placeholder, value)` | `Input` | Text input field |
| `checkbox(label, checked, on_change)` | `Checkbox` | Toggle checkbox |

### `pyftxui.theme` presets

`DEFAULT`, `DARK`, `LIGHT`, `NORD`, `DRACULA`, `MONOKAI`

### `App`

```python
app = App()
app.theme(theme.NORD)   # optional
app.run(view_fn)        # blocks until quit
app.quit()              # call from a callback to exit
```

### `LogPanel`

```python
from pyftxui import LogPanel, LogLevel, run_fullscreen

log = LogPanel.create(max_lines=500)
log.info("Server started")
log.warn("High memory usage")
run_fullscreen(log.build("App Log"))
```

## Thread safety

- `Reactive.set()` is thread-safe (mutex-protected).
- The GIL is released during `run_fullscreen()` so background Python threads run freely.
- Python callbacks registered via `on_click`, `on_change`, etc. re-acquire the GIL before calling into Python.
