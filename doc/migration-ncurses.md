# Migrating from ncurses to FTXUI

> You know ncurses. You're tired of `initscr()`, `refresh()`, and manual coordinate math.
> This guide shows you how to do the same 10 tasks in FTXUI — always shorter, always cleaner.

---

## Concepts mapping

| ncurses | FTXUI |
|---|---|
| `WINDOW*` | `Element` (DOM node) or `Component` (interactive) |
| `mvprintw(y, x, ...)` | `text("...") \| hcenter` / layout with `hbox`/`vbox` |
| `refresh()` | Automatic — `App` handles redraws |
| `initscr()` + `endwin()` | `App::TerminalOutput().Loop(component)` |
| `getch()` event loop | `CatchEvent([](Event e){ ... })` |
| `attron(COLOR_PAIR(...))` | `text(...) \| color(Color::Cyan)` |
| `noecho()` / `cbreak()` | Automatic — `App` configures the terminal |
| `wborder(win, ...)` | `element \| border` |

---

## 1. Hello world

**ncurses (~35 lines):**
```c
#include <ncurses.h>

int main() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  int y = LINES / 2;
  int x = (COLS - 13) / 2;
  mvprintw(y, x, "Hello, world!");
  refresh();

  getch();
  endwin();
  return 0;
}
```

**FTXUI (~6 lines):**
```cpp
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  auto ui = Renderer([]{ return text("Hello, world!") | center; });
  RunFullscreen(ui);
}
```

---

## 2. Draw a border

**ncurses:**
```c
WINDOW* win = newwin(10, 40, 5, 10);
box(win, 0, 0);
mvwprintw(win, 1, 2, "Content");
wrefresh(win);
```

**FTXUI:**
```cpp
auto ui = Renderer([]{
  return text("Content") | border;
});
```

Or with a title:
```cpp
return text("Content") | border | borderStyled(ROUNDED)
     | vbox({ text("title") | bold, separator(), text("Content") });
```

---

## 3. Colored text

**ncurses:**
```c
start_color();
init_pair(1, COLOR_CYAN, COLOR_BLACK);
attron(COLOR_PAIR(1));
mvprintw(5, 10, "Colored text");
attroff(COLOR_PAIR(1));
```

**FTXUI:**
```cpp
text("Colored text") | color(Color::Cyan)
```

True-color support (no palette limit):
```cpp
text("Custom color") | color(Color::RGB(255, 128, 0))
```

---

## 4. Text at a specific position

ncurses uses absolute coordinates. FTXUI uses layout — no coordinates needed.

**ncurses:**
```c
mvprintw(5, 10, "Top-left area");
mvprintw(20, 40, "Bottom-right area");
```

**FTXUI:**
```cpp
return vbox({
  filler(),
  hbox({ filler(), text("center") | border, filler() }),
  filler(),
});
// or simply:
return text("center") | center;
```

---

## 5. Keyboard input

**ncurses:**
```c
int ch;
while ((ch = getch()) != 'q') {
  switch (ch) {
    case KEY_UP:    y--; break;
    case KEY_DOWN:  y++; break;
    case KEY_LEFT:  x--; break;
    case KEY_RIGHT: x++; break;
  }
  mvprintw(y, x, "*");
  refresh();
}
```

**FTXUI:**
```cpp
int y = 10, x = 10;
auto ui = Renderer([&]{ return text("*") | hcenter; });
ui |= CatchEvent([&](Event e) {
  if (e == Event::ArrowUp)    { y--; return true; }
  if (e == Event::ArrowDown)  { y++; return true; }
  if (e == Event::ArrowLeft)  { x--; return true; }
  if (e == Event::ArrowRight) { x++; return true; }
  if (e.is_character("q"))    { App::Active()->Exit(); return true; }
  return false;
});
RunFullscreen(ui);
```

Redraws happen automatically whenever you return `true` from `CatchEvent`.

---

## 6. Mouse support

**ncurses:**
```c
mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
MEVENT event;
if (ch == KEY_MOUSE && getmouse(&event) == OK) {
  if (event.bstate & BUTTON1_CLICKED) {
    mvprintw(event.y, event.x, "X");
  }
}
```

**FTXUI:**
```cpp
ui |= CatchEvent([](Event e) {
  if (e.is_mouse() && e.mouse().button == Mouse::Left
      && e.mouse().motion == Mouse::Pressed) {
    // e.mouse().x, e.mouse().y
    return true;
  }
  return false;
});
```

---

## 7. Progress bar

**ncurses:**
```c
int width = 40;
int filled = (int)(progress * width);
mvprintw(y, x, "[");
for (int i = 0; i < width; i++)
  addch(i < filled ? '#' : ' ');
addch(']');
```

**FTXUI:**
```cpp
float progress = 0.62f;
return gauge(progress) | color(Color::Cyan);
```

Or with the high-level API (animated, themed):
```cpp
auto bar = ThemedProgressBar(&progress, "Uploading");
RunFitComponent(bar);
```

---

## 8. Menus and selection lists

**ncurses** — dozens of lines of manual highlight/scroll tracking.

**FTXUI:**
```cpp
std::vector<std::string> items = {"Option A", "Option B", "Option C"};
int selected = 0;
auto menu = Menu(&items, &selected);
RunFullscreen(menu);
```

Or with the high-level filterable list:
```cpp
auto list = List<std::string>()
  .Items(&items)
  .Label([](const std::string& s){ return s; })
  .Build();
```

---

## 9. Text input field

**ncurses:**
```c
echo();
char buf[256];
mvprintw(5, 2, "Name: ");
getnstr(buf, sizeof(buf) - 1);
noecho();
```

**FTXUI:**
```cpp
std::string input;
auto field = Input(&input, "Type here...");
RunFullscreen(field);
```

---

## 10. Split-pane layout

**ncurses:**
```c
WINDOW* left  = newwin(LINES, COLS/2, 0, 0);
WINDOW* right = newwin(LINES, COLS/2, 0, COLS/2);
box(left, 0, 0);
box(right, 0, 0);
wrefresh(left);
wrefresh(right);
```

**FTXUI:**
```cpp
auto layout = hbox({
  vbox({ text("Left panel") | bold, separator(), text("content A") }) | border | flex,
  vbox({ text("Right panel") | bold, separator(), text("content B") }) | border | flex,
});
```

Or with the high-level `HSplit`:
```cpp
auto layout = HSplit(
  Panel("Left",  left_component),
  Panel("Right", right_component)
);
```

---

## Summary

Every ncurses task is shorter in FTXUI, and:

- No manual `refresh()` or `wrefresh()` calls
- No coordinate arithmetic  
- Layout adapts automatically to terminal size changes
- Colors don't require `init_pair` or a palette limit
- Event handling is structured, not a `getch()` `switch` cascade

See the [Getting Started guide](getting-started.md) for a complete tutorial.
