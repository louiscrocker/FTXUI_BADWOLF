# Migrating from Ratatui to FTXUI

> You've built terminal UIs in Rust with Ratatui. FTXUI's model is similar —
> a retained-mode render loop, reactive state, and widget composition — but in C++
> with zero dependencies and a higher-level `ftxui::ui` layer.

---

## Concept mapping

| Ratatui | FTXUI |
|---|---|
| `Terminal<B>` | `App::Fullscreen()` / `App::TerminalOutput()` |
| `Frame::render_widget(w, area)` | `element \| ...` (declarative, no area math) |
| `Widget` trait | `Element` (DOM node) |
| `StatefulWidget` | `Component` + `State<T>` |
| `Layout::default().split(area)` | `hbox({...})` / `vbox({...})` / `HSplit(...)` |
| `Constraint::Percentage(50)` | `element \| flex` |
| `Constraint::Length(5)` | `element \| size(WIDTH, EQUAL, 5)` |
| `Paragraph::new(text)` | `text("...")` |
| `Block::default().borders(Borders::ALL)` | `element \| border` |
| `Gauge::default().percent(62)` | `gauge(0.62f)` |
| `List::new(items)` | `Menu(&items, &selected)` / `ui::List<T>` |
| `Table::new(rows, widths)` | `ui::DataTable<T>` |
| `Canvas` widget | `canvas(w, h, fn)` / `ui::BrailleCanvas` |
| `Sparkline` | `ui::Sparkline(data)` |
| `ratatui::crossterm` event loop | `App::Loop(component)` — automatic |

---

## 1. Hello world

**Ratatui (~30 lines):**
```rust
use ratatui::{
    backend::CrosstermBackend,
    widgets::Paragraph,
    Terminal,
};
use std::io;
use crossterm::{event::{self, KeyCode}, execute, terminal::*};

fn main() -> io::Result<()> {
    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    loop {
        terminal.draw(|f| {
            let p = Paragraph::new("Hello, world!");
            f.render_widget(p, f.size());
        })?;
        if let event::Event::Key(key) = event::read()? {
            if key.code == KeyCode::Char('q') { break; }
        }
    }
    disable_raw_mode()?;
    execute!(io::stdout(), LeaveAlternateScreen)?;
    Ok(())
}
```

**FTXUI (~6 lines):**
```cpp
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  auto ui = Renderer([]{ return text("Hello, world!") | center; });
  ui |= CatchEvent([](Event e){ if (e.is_character("q")) { App::Active()->Exit(); return true; } return false; });
  RunFullscreen(ui);
}
```

No `enable_raw_mode()`, no `EnterAlternateScreen`, no manual cleanup — `App` handles all of it.

---

## 2. Layout with constraints

**Ratatui:**
```rust
let chunks = Layout::default()
    .direction(Direction::Horizontal)
    .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
    .split(f.size());

f.render_widget(left_widget, chunks[0]);
f.render_widget(right_widget, chunks[1]);
```

**FTXUI:**
```cpp
auto layout = hbox({
  left_element  | border | flex,   // flex = take equal space
  right_element | border | flex,
});
```

No area objects, no index slicing. `flex` on both children gives 50/50.

---

## 3. Reactive state (`StatefulWidget` equivalent)

**Ratatui** uses `StatefulWidget<State>` with external state structs.

**FTXUI (`State<T>`):**
```cpp
State<int> selected(0);
State<std::vector<std::string>> items({"Alpha", "Beta", "Gamma"});

auto ui = Renderer([&]{
  Elements rows;
  for (int i = 0; i < (int)items->size(); i++) {
    auto row = text((*items)[i]);
    if (i == *selected) row = row | inverted;
    rows.push_back(row);
  }
  return vbox(rows) | border;
});

ui |= CatchEvent([&](Event e) {
  if (e == Event::ArrowDown) { selected.Mutate([&](int& v){ v = std::min(v+1, (int)items->size()-1); }); return true; }
  if (e == Event::ArrowUp)   { selected.Mutate([](int& v){ v = std::max(v-1, 0); }); return true; }
  return false;
});
```

`State<T>::Mutate()` automatically posts a redraw event — no `terminal.draw()` call needed.

---

## 4. Charts

**Ratatui (BarChart):**
```rust
let barchart = BarChart::default()
    .block(Block::default().title("Data").borders(Borders::ALL))
    .data(&[("B0", 0), ("B1", 2), ("B2", 4)])
    .bar_width(3);
f.render_widget(barchart, chunks[1]);
```

**FTXUI:**
```cpp
auto chart = BarChart()
  .Title("Data")
  .Bar("B0", 0.f,  Color::Cyan)
  .Bar("B1", 2.f,  Color::Yellow)
  .Bar("B2", 4.f,  Color::Green)
  .Build();
```

FTXUI uses braille rendering — sub-character resolution, no blocky bars.

**FTXUI line chart:**
```cpp
auto line = LineChart()
  .Title("CPU")
  .Series("core0", data, Color::Cyan)
  .ShowAxes(true).ShowLegend(true)
  .Build();
```

---

## 5. Canvas

**Ratatui:**
```rust
let canvas = Canvas::default()
    .block(Block::default().borders(Borders::ALL))
    .x_bounds([-180.0, 180.0])
    .y_bounds([-90.0, 90.0])
    .paint(|ctx| {
        ctx.draw(&Map { color: Color::White, resolution: MapResolution::High });
    });
```

**FTXUI:**
```cpp
// Raw canvas with braille painting:
auto c = canvas(200, 100, [](Canvas& c) {
  c.DrawPointLine(0, 0, 200, 100, Color::White);
  c.DrawText(10, 10, "label");
});

// Or high-level BrailleCanvas:
auto bc = BrailleCanvas()
  .Plot("curve", points, Color::Cyan)
  .Axes(true).Grid(true)
  .Build();
```

---

## 6. Tables

**Ratatui:**
```rust
let rows = vec![
    Row::new(vec!["Alice", "Eng", "$120k"]),
    Row::new(vec!["Bob",   "Mkt", "$95k"]),
];
let table = Table::new(rows, [Constraint::Length(10), Constraint::Length(10), Constraint::Length(8)])
    .header(Row::new(vec!["Name", "Dept", "Salary"]).style(Style::default().bold()));
```

**FTXUI:**
```cpp
struct Employee { std::string name, dept; int salary; };
std::vector<Employee> staff = { {"Alice","Eng",120000}, {"Bob","Mkt",95000} };

auto table = DataTable<Employee>()
  .Column("Name",   [](auto& e){ return e.name; })
  .Column("Dept",   [](auto& e){ return e.dept; })
  .Column("Salary", [](auto& e){ return "$" + std::to_string(e.salary); })
  .Data(&staff).Sortable(true).Build();
```

---

## 7. Event handling

**Ratatui / crossterm:**
```rust
if let Event::Key(key) = event::read()? {
    match key.code {
        KeyCode::Char('q') => break,
        KeyCode::Up        => state.scroll_up(),
        KeyCode::Down      => state.scroll_down(),
        _                  => {}
    }
}
```

**FTXUI:**
```cpp
component |= CatchEvent([&](Event e) {
  if (e.is_character("q")) { App::Active()->Exit(); return true; }
  if (e == Event::ArrowUp)   { state.scroll_up();   return true; }
  if (e == Event::ArrowDown) { state.scroll_down(); return true; }
  return false;
});
```

---

## Key differences

| | Ratatui | FTXUI |
|---|---|---|
| Language | Rust | C++ |
| Backend setup | Manual (crossterm/termion) | Automatic |
| Layout | `Constraint` + `Layout::split()` | `hbox`/`vbox` + `flex` |
| Reactive | `StatefulWidget` + manual redraw | `State<T>` auto-redraws |
| Canvas | Coordinate-based | Braille + coordinate-based |
| Globe / GeoJSON | ❌ | ✅ |
| Markdown | ❌ | ✅ |
| Built-in forms | ❌ | ✅ (`ui::Form`) |
| High-level API | ❌ | ✅ (`ftxui::ui`) |

---

## Further reading

- [Getting Started](getting-started.md)
- [doc/ui-layer.md](ui-layer.md) — full API reference
- [Cookbook](cookbook.md)
