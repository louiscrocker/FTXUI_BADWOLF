# Migrating from Bubbletea to FTXUI

> You know Go and Bubbletea's MVU pattern. FTXUI supports the same architecture — with
> C++ performance and a larger built-in widget library.

---

## Concept mapping

| Bubbletea | FTXUI |
|---|---|
| `tea.Model` | Your own `Model` struct |
| `Model.Update(msg)` | `Update(model, msg)` — pure reducer function |
| `Model.View()` | `View(model)` — returns `ftxui::Element` |
| `tea.Cmd` | `BackgroundTask::RunAsync<T>(work, callback)` |
| `tea.Msg` | Any type — usually an `enum class Msg { ... }` |
| `tea.Program` | `App::FitComponent().Loop(component)` |
| `tea.Quit()` | `App::Active()->Exit()` |
| `lipgloss.Style` | `element \| color(...) \| border \| bold \| ...` |
| `bubbles/textinput` | `ftxui::Input(...)` or `ui::TextInput` |
| `bubbles/list` | `ui::List<T>` |
| `bubbles/table` | `ui::DataTable<T>` |
| `bubbles/progress` | `ui::ThemedProgressBar(...)` |
| `bubbles/spinner` | `ui::WithSpinner(...)` |

---

## 1. Counter app

**Bubbletea (~30 lines):**
```go
package main

import (
    "fmt"
    tea "github.com/charmbracelet/bubbletea"
)

type model struct{ count int }

func (m model) Init() tea.Cmd { return nil }

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
    switch msg := msg.(type) {
    case tea.KeyMsg:
        switch msg.String() {
        case "+": m.count++
        case "-": m.count--
        case "q": return m, tea.Quit
        }
    }
    return m, nil
}

func (m model) View() string {
    return fmt.Sprintf("Count: %d\n\n[+] increment  [-] decrement  [q] quit", m.count)
}

func main() {
    p := tea.NewProgram(model{})
    p.Run()
}
```

**FTXUI (~25 lines):**
```cpp
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

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
  return vbox({
    text("Count: " + std::to_string(m.count)) | bold | center,
    text("[+] increment  [-] decrement  [q] quit") | dim | center,
  }) | border | center;
}

int main() {
  auto model = std::make_shared<Model>();
  auto dispatch = [model](Msg msg){ *model = Update(*model, msg); App::Active()->PostEvent(Event::Custom); };
  auto comp = Renderer([model]{ return View(*model); });
  comp |= CatchEvent([dispatch](Event e) {
    if (!e.is_character()) return false;
    if (e.character() == "+") { dispatch(Msg::Inc);  return true; }
    if (e.character() == "-") { dispatch(Msg::Dec);  return true; }
    if (e.character() == "q") { dispatch(Msg::Quit); return true; }
    return false;
  });
  App::FitComponent().Loop(comp);
}
```

---

## 2. List navigation

**Bubbletea** uses the `bubbles/list` package with ~50 lines of setup.

**FTXUI:**
```cpp
std::vector<std::string> items = {"Alpha", "Beta", "Gamma", "Delta"};
auto list = List<std::string>()
  .Items(&items)
  .Label([](const std::string& s){ return s; })
  .OnSelect([](const std::string& s){
    // called when user presses Enter
  })
  .Build();
RunFullscreen(list);
```

Or use the built-in low-level `Menu`:
```cpp
int selected = 0;
auto menu = Menu(&items, &selected);
```

---

## 3. Async commands (tea.Cmd equivalent)

**Bubbletea:**
```go
type resultMsg struct{ data string }

func fetchData() tea.Msg {
    // ... do work ...
    return resultMsg{data: "done"}
}

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
    switch msg := msg.(type) {
    case tea.KeyMsg:
        if msg.String() == "f" {
            return m, fetchData  // return command
        }
    case resultMsg:
        m.result = msg.data
    }
    return m, nil
}
```

**FTXUI (`BackgroundTask`):**
```cpp
#include "ftxui/ui.hpp"
using namespace ftxui::ui;

State<std::string> result("(not fetched)");

auto comp = Renderer([&]{
  return vbox({
    text("Result: " + *result),
    text("[f] fetch") | dim,
  }) | border | center;
});

comp |= CatchEvent([&](Event e) {
  if (e.is_character("f")) {
    BackgroundTask::RunAsync<std::string>(
      []{ return std::string("fetched data"); },   // runs on thread pool
      [&](std::string data){ result.Set(data); }   // called on UI thread
    );
    return true;
  }
  return false;
});

RunFullscreen(comp);
```

`State<T>::Set()` automatically triggers a redraw — no `tea.Cmd` plumbing needed.

---

## 4. Text input

**Bubbletea (bubbles/textinput):**
```go
import "github.com/charmbracelet/bubbles/textinput"
ti := textinput.New()
ti.Placeholder = "Enter name"
ti.Focus()
// ...wiring Update/View for ti...
```

**FTXUI:**
```cpp
std::string value;
auto input = Input(&value, "Enter name");
// or high-level with validation:
auto field = TextInput()
  .Label("Name")
  .Placeholder("Enter name")
  .MaxLength(64)
  .Build();
```

---

## 5. Spinner / loading state

**Bubbletea (bubbles/spinner):**
```go
s := spinner.New()
s.Spinner = spinner.Dot
// ...wire into Update/View...
```

**FTXUI:**
```cpp
bool loading = false;
my_component |= WithSpinner("Loading data…", &loading);
// set loading = true to show, false to hide
```

---

## Key differences

| | Bubbletea | FTXUI |
|---|---|---|
| Language | Go | C++ |
| MVU enforcement | Required (interface) | Optional (`ui::MVU<>` or roll your own) |
| Async | `tea.Cmd` | `BackgroundTask::RunAsync<T>` |
| Reactive auto-redraw | Via `Cmd` return | `State<T>::Set()` / `Mutate()` |
| Styling | lipgloss (separate library) | Built-in pipe operators |
| Chart support | Limited (bubblechart) | Full braille: Line/Bar/Scatter/Histogram |
| Globe / GeoJSON | ❌ | ✅ |

---

## Further reading

- [Getting Started](getting-started.md) — complete beginner tutorial
- [doc/ui-layer.md](ui-layer.md) — full `ftxui::ui` API reference
- [Cookbook](cookbook.md) — copy-paste recipes
