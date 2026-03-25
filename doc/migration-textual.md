# Migrating from Textual to FTXUI

> You've built Python TUIs with Textual. FTXUI gives you the same declarative UI
> model in C++, with much lower memory use and no Python startup overhead.
> There are also Python bindings (pyftxui) if you want to keep writing Python.

---

## Concept mapping

| Textual | FTXUI |
|---|---|
| `App` class | `App::Fullscreen()` / `RunFullscreen(component)` |
| `Widget` | `Element` (static) or `Component` (interactive) |
| `compose()` | `hbox({...})` / `vbox({...})` |
| `on_mount()` | Initialization before `Loop()` |
| `reactive` attribute | `State<T>` |
| `self.query_one("#id")` | Direct variable capture in lambdas |
| `action_*` methods | `CatchEvent` / `Keymap::Bind` |
| `CSS` styling | Pipe operators: `\| color(...)`, `\| bold`, `\| border` |
| `Screen.notify()` | `ui::Notify(message, Severity::Success)` |
| `DataTable` widget | `ui::DataTable<T>` |
| `Input` widget | `ftxui::Input(...)` or `ui::TextInput` |
| `ProgressBar` widget | `gauge(value)` or `ui::ThemedProgressBar` |
| `TabbedContent` | `ui::TabView` |
| `DirectoryTree` widget | `ui::FilePicker` |
| `Markdown` widget | `ui::MarkdownComponent(source)` |

---

## 1. Hello world

**Textual (~12 lines):**
```python
from textual.app import App, ComposeResult
from textual.widgets import Label

class HelloApp(App):
    def compose(self) -> ComposeResult:
        yield Label("Hello, world!")

if __name__ == "__main__":
    app = HelloApp()
    app.run()
```

**FTXUI (~6 lines):**
```cpp
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  RunFullscreen(Renderer([]{ return text("Hello, world!") | center; }));
}
```

---

## 2. Layout with compose()

**Textual:**
```python
class MyApp(App):
    def compose(self) -> ComposeResult:
        with Horizontal():
            yield Label("Left", id="left")
            yield Label("Right", id="right")
```

**FTXUI:**
```cpp
auto ui = Renderer([]{
  return hbox({
    text("Left")  | border | flex,
    text("Right") | border | flex,
  });
});
```

---

## 3. Reactive state

**Textual:**
```python
from textual.reactive import reactive

class Counter(App):
    count: reactive[int] = reactive(0)

    def compose(self) -> ComposeResult:
        yield Label(str(self.count))

    def on_key(self, event):
        if event.key == "plus":
            self.count += 1  # triggers re-compose automatically
```

**FTXUI:**
```cpp
State<int> count(0);

auto ui = Renderer([&]{
  return text("Count: " + std::to_string(*count)) | center;
});
ui |= CatchEvent([&](Event e) {
  if (e.is_character("+")) { count.Mutate([](int& v){ v++; }); return true; }
  return false;
});
RunFullscreen(ui);
```

`State<T>::Set()` or `Mutate()` triggers a redraw automatically — same as Textual's `reactive`.

---

## 4. Actions / keybindings

**Textual:**
```python
BINDINGS = [
    ("q", "quit", "Quit"),
    ("s", "save", "Save"),
    ("ctrl+p", "command_palette", "Command Palette"),
]
def action_save(self): ...
```

**FTXUI (`Keymap`):**
```cpp
auto kb = Keymap()
  .Bind("q",      [&]{ App::Active()->Exit(); }, "Quit")
  .Bind("s",      [&]{ save(); },               "Save")
  .Bind("Ctrl+p", [&]{ show_palette = true; },  "Command Palette");

my_component |= kb.AsDecorator();
// show built-in help with: kb.HelpElement()
```

---

## 5. DataTable

**Textual:**
```python
from textual.widgets import DataTable

table = DataTable()
table.add_columns("Name", "Dept", "Salary")
table.add_rows([("Alice", "Eng", "$120k"), ("Bob", "Mkt", "$95k")])
```

**FTXUI:**
```cpp
struct Employee { std::string name, dept; int salary; };
std::vector<Employee> staff = { {"Alice","Eng",120000}, {"Bob","Mkt",95000} };

auto table = DataTable<Employee>()
  .Column("Name",   [](auto& e){ return e.name; })
  .Column("Dept",   [](auto& e){ return e.dept; })
  .Column("Salary", [](auto& e){ return "$" + std::to_string(e.salary); })
  .Data(&staff)
  .Sortable(true)
  .Build();
```

---

## 6. Notifications

**Textual:**
```python
self.notify("File saved!", severity="information")
self.notify("Error!", severity="error")
```

**FTXUI:**
```cpp
// First, wrap your root component:
root |= WithNotifications();

// Then from anywhere (including background threads):
Notify("File saved!",  Severity::Success);
Notify("Error!",       Severity::Error);
Notify("Watch out",    Severity::Warning);
```

---

## 7. Markdown widget

**Textual:**
```python
from textual.widgets import Markdown
yield Markdown(markdown_string)
```

**FTXUI:**
```cpp
auto md = MarkdownComponent(markdown_string);
RunFullscreen(md);
```

---

## 8. Tabs

**Textual:**
```python
with TabbedContent("Home", "Settings", "About"):
    yield home_content
    yield settings_content
    yield about_content
```

**FTXUI:**
```cpp
auto tabs = TabView()
  .Tab("Home",     home_component)
  .Tab("Settings", settings_component)
  .Tab("About",    about_component)
  .Build();
```

---

## Python bindings (pyftxui)

If you want to keep writing Python, [pyftxui](https://github.com/ArthurSonzogni/pyftxui)
provides Python bindings for FTXUI:

```python
import pyftxui as ftxui

screen = ftxui.Screen.FitComponent()
document = ftxui.vbox([
    ftxui.text("Hello from Python!") | ftxui.border,
    ftxui.gauge(0.5),
])
screen.Print(document)
```

---

## Performance comparison

| | Textual | FTXUI |
|---|---|---|
| Language | Python | C++ |
| Startup | ~200 ms | <1 ms |
| Memory (idle) | ~50 MB | ~2 MB |
| Binary size | N/A (Python) | ~2 MB |
| Chart support | ✅ (Plotext) | ✅ (native braille) |
| Globe / GeoJSON | ❌ | ✅ |
| Markdown | ✅ | ✅ |

---

## Further reading

- [Getting Started](getting-started.md)
- [doc/ui-layer.md](ui-layer.md) — full API reference
- [Cookbook](cookbook.md)
