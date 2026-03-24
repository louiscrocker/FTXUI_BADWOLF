# `ftxui::ui` — High-Level API Layer

> **New in FTXUI!** The `ftxui::ui` namespace provides a batteries-included, fluent-builder layer on top of the core library. Think of it as a Go `tview` or Rust `ratatui` experience, but in C++.

---

## Quick Start (5-line example)

```cpp
#include <ftxui/ui/app.hpp>
#include <ftxui/ui/form.hpp>
#include <string>

int main() {
  std::string name;
  auto form = ftxui::ui::Form()
      .Title("Hello")
      .Field("Name", &name, "Your name")
      .Submit("OK", [&]{ /* use name */ });
  ftxui::ui::RunFullscreen(form.Build());
}
```

---

## Installation / Linking

Add `ftxui::ui` to your CMake target after the standard FTXUI setup:

```cmake
include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v6.1.9
)
FetchContent_MakeAvailable(ftxui)

target_link_libraries(myapp PRIVATE ftxui::ui)
```

`ftxui::ui` already transitively pulls in `ftxui::component`, `ftxui::dom`, and `ftxui::screen`.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                     ftxui::ui                           │
│  Form  DataTable  List  Tree  Router  CommandPalette … │
└────────────────────────┬────────────────────────────────┘
                         │ builds / wraps
┌────────────────────────▼────────────────────────────────┐
│                  ftxui::component                       │
│   Button  Input  Menu  Renderer  CatchEvent  …         │
└────────────────────────┬────────────────────────────────┘
                         │ renders
┌────────────────────────▼────────────────────────────────┐
│                    ftxui::dom                           │
│   text  hbox  vbox  border  flex  color  table  …      │
└────────────────────────┬────────────────────────────────┘
                         │ outputs to
┌────────────────────────▼────────────────────────────────┐
│                   ftxui::screen                         │
│   Screen  Color  Terminal  Event  App                  │
└─────────────────────────────────────────────────────────┘
```

---

## Components

### Theme

**Header:** `<ftxui/ui/theme.hpp>`

A centralized style configuration consumed by all `ftxui::ui` components. Set it once at startup and every subsequent component picks it up automatically.

```cpp
#include <ftxui/ui/theme.hpp>
using namespace ftxui;

// Use a built-in preset:
ui::SetTheme(ui::Theme::Dracula());

// Or build a custom theme with fluent setters:
ui::SetTheme(ui::Theme::Dark()
    .WithPrimary(Color::Magenta)
    .WithBorderStyle(ROUNDED)
    .WithAnimations(false));

// Temporarily override inside a scope:
{
    ui::ScopedTheme scoped(ui::Theme::Light());
    // all components built here use Light theme
}
// previous theme restored here
```

#### Theme presets

| Preset | Description |
|--------|-------------|
| `Theme::Default()` | Cyan accent, dark background |
| `Theme::Dark()` | High-contrast dark |
| `Theme::Light()` | Light background |
| `Theme::Nord()` | Nord color scheme |
| `Theme::Dracula()` | Dracula color scheme |
| `Theme::Monokai()` | Monokai color scheme |

#### Theme color fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `primary` | `Color` | `Cyan` | Accent / focus color |
| `secondary` | `Color` | `Blue` | Secondary highlight |
| `accent` | `Color` | `GreenLight` | Success / positive |
| `error_color` | `Color` | `Red` | Error / danger |
| `warning_color` | `Color` | `Yellow` | Warning / caution |
| `success_color` | `Color` | `Green` | Success indicator |
| `text` | `Color` | `Default` | Normal text |
| `text_muted` | `Color` | `GrayLight` | Dim / secondary text |
| `border_color` | `Color` | `GrayDark` | Border lines |
| `button_bg_normal` | `Color` | `Default` | Button background (normal) |
| `button_fg_normal` | `Color` | `White` | Button foreground (normal) |
| `button_bg_active` | `Color` | `Cyan` | Button background (focused) |
| `button_fg_active` | `Color` | `Black` | Button foreground (focused) |
| `border_style` | `BorderStyle` | `ROUNDED` | Border character style |
| `animations_enabled` | `bool` | `true` | Enable animated transitions |

#### Fluent setters

| Method | Description |
|--------|-------------|
| `WithPrimary(Color)` | Override primary color |
| `WithSecondary(Color)` | Override secondary color |
| `WithAccent(Color)` | Override accent color |
| `WithError(Color)` | Override error color |
| `WithBorderStyle(BorderStyle)` | Override border style |
| `WithBorderColor(Color)` | Override border color |
| `WithAnimations(bool)` | Enable/disable animations |

#### Free functions

| Function | Description |
|----------|-------------|
| `SetTheme(const Theme&)` | Replace the active theme |
| `GetTheme()` | Return the currently active theme |
| `ScopedTheme(const Theme&)` | RAII scope override |

---

### App Runners

**Header:** `<ftxui/ui/app.hpp>`

Convenience wrappers around `App::*` that run a component and block until the user exits.

```cpp
#include <ftxui/ui/app.hpp>

ui::Run(my_component);             // scrolling terminal output (no alt-screen)
ui::RunFullscreen(my_component);   // fullscreen alternate-screen buffer
ui::RunFitComponent(my_component); // window sized to rendered output
ui::RunFixed(my_component, 80, 24); // fixed-size window
```

| Function | Description |
|----------|-------------|
| `Run(Component)` | Terminal-output mode (scrolling, no alt-screen) |
| `RunFullscreen(Component)` | Fullscreen alternate-screen buffer |
| `RunFitComponent(Component)` | Window sized to fit rendered output |
| `RunFixed(Component, int w, int h)` | Fixed-size window |

---

### State\<T\>

**Header:** `<ftxui/ui/state.hpp>`

A reactive value wrapper. Any mutation automatically posts `Event::Custom` to the active `App`, triggering a redraw. The value is held in a `shared_ptr` so it can be safely captured in lambdas.

```cpp
#include <ftxui/ui/state.hpp>
#include <ftxui/component/component.hpp>

auto count = ftxui::ui::State<int>(0);

auto comp = Renderer([&] {
    return text("Count: " + std::to_string(*count)) | border | center;
});
comp |= CatchEvent([&](Event e) {
    if (e.is_character() && e.character() == "+") {
        count.Mutate([](int& v){ v++; });
        return true;
    }
    return false;
});
ftxui::ui::RunFullscreen(comp);
```

| Method | Description |
|--------|-------------|
| `State<T>(T initial)` | Construct with an initial value |
| `Get()` | Return a copy of the value |
| `Ref()` | Return a `const T&` reference |
| `Ptr()` | Return a raw pointer (`T*` or `const T*`) |
| `operator*()` | Dereference to `T&` or `const T&` |
| `operator->()` | Arrow operator |
| `Set(T)` | Replace value and request redraw |
| `Mutate(fn)` | Apply `fn(T&)` in-place and request redraw |
| `Share()` | Return a `State<T>` that co-owns the same value |

> **Note:** `Share()` is useful for passing a `State` handle to background threads—mutations on the shared handle still trigger UI redraws.

---

### MVU\<Model, Msg\>

**Header:** `<ftxui/ui/mvu.hpp>`

Model-View-Update (Elm / Bubbletea-style) architecture. Write your app as three pure functions and let the runtime manage the event loop.

```cpp
#include <ftxui/ui/mvu.hpp>

struct Model { int count = 0; };
enum Msg { Inc, Dec, Quit };

Model Update(Model m, Msg msg) {
    switch (msg) {
        case Inc:  m.count++; return m;
        case Dec:  m.count--; return m;
        case Quit: ftxui::App::Active()->Exit(); return m;
    }
    return m;
}

ftxui::Element View(const Model& m, std::function<void(Msg)> dispatch) {
    return vbox({
        text("Count: " + std::to_string(m.count)) | bold | center,
        hbox({
            Button("−", [&]{ dispatch(Dec); })->Render(),
            Button("+", [&]{ dispatch(Inc); })->Render(),
            Button("q", [&]{ dispatch(Quit); })->Render(),
        }) | center,
    }) | border | center;
}

int main() {
    ftxui::ui::MVU<Model, Msg>({}, Update, View).RunFullscreen();
}
```

| Method | Description |
|--------|-------------|
| `MVU(initial, update, view)` | Construct with initial model, reducer, and view |
| `Build()` | Return a `Component` driving the MVU loop |
| `Run()` | Build and run in terminal-output mode |
| `RunFullscreen()` | Build and run fullscreen |
| `RunFitComponent()` | Build and run fit-to-component |

Free convenience functions `ftxui::ui::Run(initial, update, view)` and `ftxui::ui::RunFullscreen(...)` are also provided.

---

### BackgroundTask

**Header:** `<ftxui/ui/background_task.hpp>`

Fire-and-forget async tasks that safely marshal their results back to the UI thread.

```cpp
#include <ftxui/ui/background_task.hpp>

bool loading = false;
std::string result;

// Fire and forget:
loading = true;
ftxui::ui::RunAsync<std::string>(
    []() -> std::string {
        return fetch_from_network(); // background thread
    },
    [&](std::string data) {          // UI thread
        loading = false;
        result = std::move(data);
    });

// With a cancellable handle:
auto handle = ftxui::ui::MakeAsync<int>(
    []{ return compute(); },
    [&](int v){ display(v); });

// Cancel before it completes:
handle->Cancel();
```

| Function / Method | Description |
|-------------------|-------------|
| `RunAsync<T>(work, on_complete)` | Run `work()` on background thread; call `on_complete(T)` on UI thread |
| `RunAsync(work, on_complete)` | Void overload; `on_complete` is optional |
| `MakeAsync<T>(work, on_complete)` | Same as `RunAsync` but returns a cancellable `AsyncHandle` |
| `handle->Cancel()` | Suppress the `on_complete` callback |
| `handle->IsCancelled()` | Query cancellation state |

---

### Form

**Header:** `<ftxui/ui/form.hpp>`

Fluent builder that produces a labeled, themed data-entry form component.

```cpp
#include <ftxui/ui/form.hpp>

std::string name, email, password;
bool subscribe = false;
const std::vector<std::string> roles = {"Admin", "User", "Guest"};
int role = 0;

auto form = ftxui::ui::Form()
    .Title("Sign Up")
    .Field("Name",        &name,     "Your full name")
    .Field("Email",       &email,    "you@example.com")
    .Password("Password", &password)
    .Check("Subscribe to newsletter", &subscribe)
    .Select("Role",       &roles,    &role)
    .Separator()
    .Submit("Register",   [&]{ do_register(); })
    .Cancel("Cancel",     [&]{ app->Exit(); });

ftxui::ui::RunFullscreen(form.Build());
```

#### Builder methods

| Method | Description |
|--------|-------------|
| `Title(string_view)` | Optional title displayed above the form |
| `LabelWidth(int)` | Fixed width for all labels (default auto) |
| `Field(label, string*, placeholder)` | Single-line text input |
| `Password(label, string*, placeholder)` | Masked password input |
| `Multiline(label, string*, placeholder)` | Multi-line text area |
| `Check(label, bool*)` | Checkbox |
| `Radio(label, vector<string>*, int*)` | Radio button group |
| `Select(label, vector<string>*, int*)` | Drop-down selector |
| `Integer(label, int*, min, max, step)` | Numeric integer slider |
| `Float(label, float*, min, max, step)` | Numeric float slider |
| `Section(string_view)` | Section header / label |
| `Separator()` | Horizontal separator line |
| `Button(label, on_click)` | Generic action button |
| `Submit(label, on_submit)` | Primary submit button |
| `Cancel(label, on_cancel)` | Cancel button |
| `Build()` | Produce the `ftxui::Component` |

> **Note:** All pointer arguments (`string*`, `bool*`, `int*`, etc.) must remain valid for the lifetime of the built component. `Form` is implicitly convertible to `ftxui::Component`.

---

### Layout

**Header:** `<ftxui/ui/layout.hpp>`

Higher-level layout primitives that compose FTXUI components.

```cpp
#include <ftxui/ui/layout.hpp>

// Titled border panel
auto panel = ftxui::ui::Panel("Server List", server_list_comp);

// Horizontal / vertical stacks
auto row = ftxui::ui::Row({left_comp, right_comp});
auto col = ftxui::ui::Column({top_comp, bottom_comp});

// Resizable split (drag the divider or resize the variable)
int split = 40;
auto split_view = ftxui::ui::HSplit(left_panel, right_panel, &split);

// Tab bar
int tab = 0;
auto tabs = ftxui::ui::TabView({"Files", "Settings", "Log"},
                               {files_comp, settings_comp, log_comp},
                               &tab);

// Scrollable region
auto scroll = ftxui::ui::ScrollView("History", big_component);

// Labeled row
auto row = ftxui::ui::Labeled("Username", input_comp, 12);

// Status bar (fixed 1-row bar at bottom)
auto status = ftxui::ui::StatusBar([&]{ return "Mode: " + mode; });
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `Panel` | `(title, Component)` | Wrap component in a titled bordered panel |
| `Panel` | `(title, function<Element()>)` | Stateless panel from render callback |
| `Row` | `(Components)` | Horizontal equally-growing children |
| `Column` | `(Components)` | Vertical stacked children |
| `StatusBar` | `(function<string()>)` | Fixed 1-row status bar |
| `StatusBar` | `(function<Element()>)` | Status bar from element callback |
| `ScrollView` | `(Component)` | Vertically-scrollable frame |
| `ScrollView` | `(title, Component)` | Scrollable frame with title |
| `Labeled` | `(label, Component, label_width=14)` | `"Label:  [comp]"` row |
| `HSplit` | `(left, right, int*)` | Side-by-side resizable split |
| `VSplit` | `(top, bottom, int*)` | Top/bottom resizable split |
| `TabView` | `(tabs, pages, int*)` | Horizontal tab bar |

> **Tip:** `split_pos` defaults to `nullptr` for a static 50/50 split. Pass a pointer to allow runtime resizing.

---

### SimpleTable

**Header:** `<ftxui/ui/simple_table.hpp>`

A themed static table rendered as a DOM `Element` (not a `Component`—use inside a `Renderer`).

```cpp
#include <ftxui/ui/simple_table.hpp>

auto elem = ftxui::ui::SimpleTable(
    {"Name", "Department", "Salary"},
    {{"Alice", "Engineering", "$120k"},
     {"Bob",   "Marketing",   "$95k"},
     {"Carol", "Design",      "$105k"}})
    .AlternateRows(true)
    .ColumnSeparators(true)
    .Build();

// Use inside a Renderer:
auto comp = Renderer([elem]{ return elem; });
```

| Method | Description |
|--------|-------------|
| `SimpleTable(headers, rows)` | Construct with header strings and row data |
| `AlternateRows(bool)` | Alternate row background tinting (default: true) |
| `ColumnSeparators(bool)` | Show vertical column separators (default: true) |
| `ColumnWidths(vector<int>)` | Override column widths (0 = auto) |
| `Build()` | Produce the `ftxui::Element` |

`SimpleTable` is implicitly convertible to `ftxui::Element`.

---

### DataTable\<T\>

**Header:** `<ftxui/ui/datatable.hpp>`

An interactive, sortable, filterable table component backed by a typed data vector.

```cpp
#include <ftxui/ui/datatable.hpp>

struct Employee { std::string name, dept; int salary; };
std::vector<Employee> staff = { {"Alice","Eng",120}, {"Bob","Mkt",95} };
std::string filter;

auto table = ftxui::ui::DataTable<Employee>()
    .Column("Name",   [](const Employee& e){ return e.name; })
    .Column("Dept",   [](const Employee& e){ return e.dept; })
    .Column("Salary", [](const Employee& e){ return std::to_string(e.salary); }, 8)
    .Data(&staff)
    .Selectable(true)
    .Sortable(true)
    .AlternateRows(true)
    .FilterText(&filter)
    .OnSelect([](const Employee& e, size_t){ /* row highlighted */ })
    .OnActivate([](const Employee& e, size_t){ /* Enter pressed */ })
    .Build();
```

| Method | Description |
|--------|-------------|
| `Column(header, getter, width=-1)` | Add a display column; `getter` extracts display string from row |
| `Data(const vector<T>*)` | Bind the data vector (must outlive component) |
| `Selectable(bool)` | Enable arrow-key row selection (default: true) |
| `Sortable(bool)` | Enable `<`/`>` column sorting (default: false) |
| `AlternateRows(bool)` | Alternate row shading (default: true) |
| `FilterText(string*)` | Bind an external filter string |
| `OnSelect(fn)` | Callback `(const T&, size_t index)` on selection change |
| `OnActivate(fn)` | Callback `(const T&, size_t index)` on Enter |
| `Build()` | Produce the `ftxui::Component` |

**Keyboard controls:** `↑`/`↓` navigate, `Enter` activates, `<`/`>` cycle sort column, `s` toggle sort direction.

---

### List\<T\>

**Header:** `<ftxui/ui/list.hpp>`

An interactive, optionally-filterable single-column item list.

```cpp
#include <ftxui/ui/list.hpp>

std::vector<std::string> fruits = {"Apple", "Banana", "Cherry"};

auto list = ftxui::ui::List<std::string>()
    .Data(&fruits)
    .Filterable(true)
    .OnActivate([](const std::string& s, size_t){ /* ... */ })
    .Empty("No matching fruits")
    .Build();
```

For custom types that aren't string-convertible, supply a `Label()` extractor:

```cpp
struct Song { std::string title, artist; };
std::vector<Song> songs = { ... };

auto list = ftxui::ui::List<Song>()
    .Data(&songs)
    .Label([](const Song& s){ return s.artist + " – " + s.title; })
    .Render([](const Song& s, bool sel) -> ftxui::Element {
        auto e = hbox({ text(s.artist) | bold, text(" – "), text(s.title) });
        return sel ? e | color(Color::Cyan) : e;
    })
    .Filterable(true)
    .Build();
```

| Method | Description |
|--------|-------------|
| `Data(const vector<T>*)` | Bind the data vector (must outlive component) |
| `Render(fn)` | Custom item renderer `(const T&, bool is_selected) → Element` |
| `Label(fn)` | Label extractor `(const T&) → string` for filtering and default rendering |
| `Filterable(bool)` | Show search bar and filter on keystrokes (default: false) |
| `OnSelect(fn)` | Callback `(const T&, size_t)` on selection change |
| `OnActivate(fn)` | Callback `(const T&, size_t)` on Enter |
| `Empty(string_view)` | Message shown when list is empty |
| `Build()` | Produce the `ftxui::Component` |

**Keyboard controls (Filterable):** type to filter, `Backspace` to erase, `Escape` to clear filter.

---

### Tree

**Header:** `<ftxui/ui/tree.hpp>`

A collapsible tree-view component built from `TreeNode` values.

```cpp
#include <ftxui/ui/tree.hpp>

auto tree = ftxui::ui::Tree()
    .Node(ftxui::ui::TreeNode::Branch("src", {
        ftxui::ui::TreeNode::Leaf("main.cpp",  [&]{ open("main.cpp"); }),
        ftxui::ui::TreeNode::Leaf("util.cpp"),
        ftxui::ui::TreeNode::Branch("tests", {
            ftxui::ui::TreeNode::Leaf("main_test.cpp"),
        }),
    }))
    .Node(ftxui::ui::TreeNode::Leaf("CMakeLists.txt"))
    .OnSelect([](const std::string& label){ status = label; })
    .Build();
```

#### TreeNode factory functions

| Factory | Description |
|---------|-------------|
| `TreeNode::Leaf(label, on_select={})` | Leaf node with no children |
| `TreeNode::Branch(label, children, on_select={})` | Collapsible branch node |

#### Tree builder methods

| Method | Description |
|--------|-------------|
| `Node(TreeNode)` | Append a root node |
| `OnSelect(fn)` | Global callback `(const string& label)` for any node selection |
| `Build()` | Produce the `ftxui::Component` |

---

### Router

**Header:** `<ftxui/ui/router.hpp>`

History-based client-side router for multi-screen terminal applications.

```cpp
#include <ftxui/ui/router.hpp>

ftxui::ui::Router router;
router.Route("home",     home_screen)
      .Route("settings", []{ return MakeSettings(); })  // lazy factory
      .Default("home")
      .OnNavigate([](std::string_view from, std::string_view to){
          log("nav: " + std::string(from) + " → " + std::string(to));
      });

auto root = router.Build();

// Inside any button callback:
router.Push("settings");    // push onto history stack
router.Pop();               // go back
router.Replace("home");     // replace current (no extra history entry)
```

| Method | Description |
|--------|-------------|
| `Route(name, Component)` | Register a pre-built component |
| `Route(name, factory)` | Register a lazy `function<Component()>` (created on first visit) |
| `Default(name)` | Set the initial route |
| `OnNavigate(fn)` | Callback `(string_view from, string_view to)` on route change |
| `Push(name)` | Navigate to route, add to history |
| `Pop()` | Navigate back (no-op if history size ≤ 1) |
| `Replace(name)` | Navigate to route without adding history entry |
| `Current()` | Return `string_view` of the active route name |
| `CanGoBack()` | Return `true` if there is a previous route |
| `Build()` | Produce the root `ftxui::Component` |

---

### CommandPalette

**Header:** `<ftxui/ui/command_palette.hpp>`

A searchable VS Code / Telescope-style command palette overlay. Default activation key: `Ctrl+P`.

```cpp
#include <ftxui/ui/command_palette.hpp>

auto root = ftxui::ui::CommandPalette()
    .Add("Quit",     [&]{ ftxui::App::Active()->Exit(); }, "Exit the application")
    .Add("Save",     [&]{ save(); },                       "Save current file",  "File")
    .Add("Settings", [&]{ router.Push("settings"); },      "Open settings",      "Navigation")
    .Bind("Ctrl+p")
    .Wrap(my_component);

// Or use as a ComponentDecorator:
my_component |= ftxui::ui::CommandPalette()
    .Add("Quit", [&]{ /* ... */ })
    .AsDecorator();
```

| Method | Description |
|--------|-------------|
| `Add(name, action, description="", category="")` | Register a command |
| `Bind(key="Ctrl+p")` | Set the activation key string |
| `AsDecorator()` | Return a `ComponentDecorator` for `\|=` operator |
| `Wrap(Component)` | Wrap a parent component with the palette overlay |

---

### Wizard

**Header:** `<ftxui/ui/wizard.hpp>`

A multi-step wizard dialog with Next / Back / Cancel navigation.

```cpp
#include <ftxui/ui/wizard.hpp>

auto wizard = ftxui::ui::Wizard("Setup Assistant")
    .Step("Welcome",  welcome_component)
    .Step("Options",  options_component)
    .Step("Confirm",  confirm_component)
    .OnComplete([&]{ ftxui::App::Active()->Exit(); })
    .OnCancel([&]{ ftxui::App::Active()->Exit(); });

ftxui::ui::RunFullscreen(wizard.Build());
```

| Method | Description |
|--------|-------------|
| `Wizard(title="")` | Construct with optional title |
| `Title(string_view)` | Set or update the title |
| `Step(title, Component)` | Append a wizard step |
| `OnComplete(fn)` | Callback when user reaches the end and confirms |
| `OnCancel(fn)` | Callback when user presses Cancel |
| `Build()` | Produce the `ftxui::Component` |

`Wizard` is implicitly convertible to `ftxui::Component`.

---

### LogPanel

**Header:** `<ftxui/ui/log_panel.hpp>`

A thread-safe, auto-scrolling log panel.

> **Important:** Always create `LogPanel` via `LogPanel::Create()`. It is not directly constructible.

```cpp
#include <ftxui/ui/log_panel.hpp>

auto log = ftxui::ui::LogPanel::Create();
log->SetMinLevel(ftxui::ui::LogLevel::Debug);
log->SetMaxLines(500);

// Log from any thread:
log->Info("Server started on port 8080");
log->Warn("Memory usage above 80%");
log->Error("Connection refused: " + addr);

// Build and run:
ftxui::ui::RunFullscreen(log->Build("Application Log"));
```

#### Log levels

| Level | `LogLevel` value |
|-------|-----------------|
| Trace | `LogLevel::Trace` |
| Debug | `LogLevel::Debug` |
| Info  | `LogLevel::Info` |
| Warn  | `LogLevel::Warn` |
| Error | `LogLevel::Error` |

#### Methods

| Method | Description |
|--------|-------------|
| `LogPanel::Create(max_lines=1000)` | Create a new `shared_ptr<LogPanel>` |
| `Log(message, level=Info)` | Append a message at the given level |
| `Trace(message)` | Append at Trace level |
| `Debug(message)` | Append at Debug level |
| `Info(message)` | Append at Info level |
| `Warn(message)` | Append at Warn level |
| `Error(message)` | Append at Error level |
| `Clear()` | Remove all entries |
| `SetMaxLines(n)` | Cap the scrollback buffer size |
| `SetMinLevel(LogLevel)` | Hide entries below this level |
| `Build(title="")` | Produce the `ftxui::Component` (no title = no border) |

---

### Notification / Toast

**Header:** `<ftxui/ui/notification.hpp>`

Non-blocking toast notifications rendered as a stack in the top-right corner.

```cpp
#include <ftxui/ui/notification.hpp>

// Post from anywhere (including background threads):
ftxui::ui::Notify("File saved!", ftxui::ui::Severity::Success);
ftxui::ui::Notify("Low disk space", ftxui::ui::Severity::Warning,
                  std::chrono::milliseconds(5000));
ftxui::ui::Notify("Cannot connect", ftxui::ui::Severity::Error,
                  std::chrono::milliseconds(0)); // persistent

ftxui::ui::ClearNotifications();

// Wrap your root component to render the overlay:
auto root = ftxui::ui::WithNotifications(my_component);

// Or as a decorator:
my_component |= ftxui::ui::WithNotifications();
```

#### Severity values

| Value | Description |
|-------|-------------|
| `Severity::Info` | Neutral information |
| `Severity::Success` | Positive confirmation |
| `Severity::Warning` | Caution |
| `Severity::Error` | Failure |
| `Severity::Debug` | Debug-only messages |

| Function | Description |
|----------|-------------|
| `Notify(message, severity=Info, duration=3000ms)` | Post a notification; `duration=0` = persistent |
| `ClearNotifications()` | Dismiss all active notifications |
| `WithNotifications()` | Return `ComponentDecorator` |
| `WithNotifications(Component)` | Wrap parent component with notification overlay |

---

### Dialog

**Header:** `<ftxui/ui/dialog.hpp>`

Modal overlay dialogs: confirmation, alert, and keyboard-shortcut help.

```cpp
#include <ftxui/ui/dialog.hpp>

bool show_confirm = false;
bool show_alert   = false;
bool show_help    = false;

// Confirmation dialog:
my_comp |= ftxui::ui::WithConfirm(
    "Delete this file?",
    [&]{ do_delete(); show_confirm = false; },
    [&]{ show_confirm = false; },
    &show_confirm);

// Alert dialog:
my_comp |= ftxui::ui::WithAlert(
    "Error", "File not found.",
    &show_alert,
    [&]{ show_alert = false; });

// Keyboard shortcuts help:
my_comp |= ftxui::ui::WithHelp(
    "Keyboard Shortcuts",
    {{"q",      "Quit"},
     {"Ctrl+s", "Save"},
     {"?",      "Show help"}},
    &show_help,
    [&]{ show_help = false; });
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `WithConfirm` | `(message, on_yes, on_no, const bool*)` | Yes/No confirmation dialog decorator |
| `WithConfirm` | `(Component, message, on_yes, on_no, const bool*)` | Wrap component with confirmation dialog |
| `WithAlert` | `(title, message, const bool*, on_close={})` | Informational alert dialog decorator |
| `WithAlert` | `(Component, title, message, const bool*, on_close={})` | Wrap component with alert dialog |
| `WithHelp` | `(title, bindings, const bool*, on_close={})` | Key-binding help panel decorator |
| `WithHelp` | `(Component, title, bindings, const bool*, on_close={})` | Wrap component with help panel |

`bindings` is a `vector<pair<string, string>>` of `{key_display, description}`.

---

### Keymap

**Header:** `<ftxui/ui/keymap.hpp>`

A registry of keyboard shortcuts with automatic help rendering.

```cpp
#include <ftxui/ui/keymap.hpp>

auto kb = ftxui::ui::Keymap()
    .Bind("q",      [&]{ ftxui::App::Active()->Exit(); }, "Quit")
    .Bind("Ctrl+s", [&]{ save(); },                       "Save")
    .Bind("?",      [&]{ show_help = true; },             "Show help")
    .Bind("F1",     [&]{ show_help = true; },             "Show help");

// Apply to a component:
my_component |= kb.AsDecorator();

// Render the help table inside a dialog:
bool show_help = false;
my_component |= ftxui::ui::WithHelp("Shortcuts",
    {/* ... */}, &show_help);
```

#### Supported key string formats

| Format | Examples |
|--------|---------|
| Single printable character | `"q"`, `"?"`, `"/"` |
| Named keys | `"Enter"`, `"Escape"`, `"Tab"`, `"Backspace"`, `"Delete"`, `"Up"`, `"Down"`, `"Left"`, `"Right"` |
| Function keys | `"F1"` … `"F12"` |
| Control combos | `"Ctrl+a"` … `"Ctrl+z"` (letter is case-insensitive) |

| Method | Description |
|--------|-------------|
| `Bind(key, action, description="")` | Register a keybinding |
| `Wrap(Component)` | Return wrapped component that intercepts registered keys |
| `AsDecorator()` | Return `ComponentDecorator` for `\|=` operator |
| `HelpElement()` | Render a two-column help table `Element` |

`Keymap` is implicitly convertible to `ComponentDecorator`.

---

### Progress

**Header:** `<ftxui/ui/progress.hpp>`

Themed progress bars and animated loading spinners.

```cpp
#include <ftxui/ui/progress.hpp>

float progress = 0.0f;
bool loading = false;

// Progress bar component:
auto bar = ftxui::ui::ThemedProgressBar(&progress, "Uploading");
ftxui::App::FitComponent().Loop(bar);

// Progress bar from callback:
auto bar2 = ftxui::ui::ThemedProgressBar([&]{ return bytes_done / bytes_total; },
                                         "Downloading");

// Spinner overlay:
my_component |= ftxui::ui::WithSpinner("Loading data…", &loading);

// Spinner with callback:
my_component |= ftxui::ui::WithSpinner("Processing…", [&]{ return is_busy; });
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `ThemedProgressBar` | `(float*, label="")` | Progress bar component; value in `[0.0, 1.0]` |
| `ThemedProgressBar` | `(function<float()>, label="")` | Progress bar driven by callback |
| `WithSpinner` | `(message, const bool*)` | Spinner decorator; shows when `*show == true` |
| `WithSpinner` | `(Component, message, const bool*)` | Wrap component with spinner overlay |
| `WithSpinner` | `(message, function<bool()>)` | Spinner driven by callback |
| `WithSpinner` | `(Component, message, function<bool()>)` | Wrap component with callback-driven spinner |

---

### Widgets

**Header:** `<ftxui/ui/widgets.hpp>`

Small, standalone DOM `Element` helpers.

```cpp
#include <ftxui/ui/widgets.hpp>

// Badge (Decorator — use with | operator):
text("Messages") | ftxui::ui::Badge(3)
text("Alerts")   | ftxui::ui::Badge(12, ftxui::Color::Red)
text("Status")   | ftxui::ui::Badge("NEW", ftxui::Color::Green)

// Badge as wrapping function:
auto elem = ftxui::ui::WithBadge(text("Inbox"), 5);

// Empty state placeholder:
if (items.empty()) {
    return ftxui::ui::EmptyState("🗒", "No items", "Add one to get started.");
}

// Horizontal separator with label:
ftxui::ui::LabeledSeparator("Advanced Options")

// Keyboard shortcut hint:
hbox({ text("Press "), ftxui::ui::Kbd("Ctrl+S"), text(" to save") })

// Colored status dot:
ftxui::ui::StatusDot(ftxui::Color::Green, "Online")
ftxui::ui::StatusDot(ftxui::Color::Red)   // dot only, no label
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `Badge` | `(int count, Color=Red)` | `Decorator` — numeric badge overlay |
| `Badge` | `(string_view label, Color=Red)` | `Decorator` — text badge overlay |
| `WithBadge` | `(Element base, int count, Color=Red)` | Wrap element with a numeric badge |
| `EmptyState` | `(icon, title, subtitle="")` | Centered empty-state placeholder `Element` |
| `LabeledSeparator` | `(string_view label)` | Horizontal separator with centered label `Element` |
| `Kbd` | `(string_view key)` | Keyboard shortcut hint box `Element` |
| `StatusDot` | `(Color, string_view label="")` | Colored status dot `●` with optional label `Element` |

> **Note:** `Badge(int, Color)` and `Badge(string_view, Color)` return a `Decorator` (use with the `|` pipe operator), not an `Element`. Use `WithBadge(element, count)` when you already have the base element.

---

## Full Example

The following example demonstrates a small employee management app that uses several `ftxui::ui` features together: theme, layout, form, data table, notifications, and dialogs.

```cpp
#include <string>
#include <vector>
#include <ftxui/ui/app.hpp>
#include <ftxui/ui/theme.hpp>
#include <ftxui/ui/form.hpp>
#include <ftxui/ui/datatable.hpp>
#include <ftxui/ui/layout.hpp>
#include <ftxui/ui/notification.hpp>
#include <ftxui/ui/dialog.hpp>
#include <ftxui/ui/widgets.hpp>
#include <ftxui/component/component.hpp>

struct Employee { std::string name, dept; int salary; };

int main() {
    ftxui::ui::SetTheme(ftxui::ui::Theme::Nord());

    std::vector<Employee> staff = {
        {"Alice", "Engineering", 120000},
        {"Bob",   "Marketing",   95000},
    };

    // ── Add-employee form ───────────────────────────────────────────
    std::string new_name, new_dept;
    int salary = 80000;
    bool show_confirm = false;

    auto form = ftxui::ui::Form()
        .Title("Add Employee")
        .Field("Name",       &new_name, "Full name")
        .Field("Department", &new_dept, "Team")
        .Integer("Salary",   &salary,   0, 500000, 5000)
        .Submit("Add", [&]{
            if (!new_name.empty()) {
                staff.push_back({new_name, new_dept, salary});
                ftxui::ui::Notify("Added " + new_name, ftxui::ui::Severity::Success);
                new_name.clear(); new_dept.clear();
            }
        });

    // ── Employee table ──────────────────────────────────────────────
    auto table = ftxui::ui::DataTable<Employee>()
        .Column("Name",   [](const Employee& e){ return e.name; })
        .Column("Dept",   [](const Employee& e){ return e.dept; })
        .Column("Salary", [](const Employee& e){ return "$" + std::to_string(e.salary); }, 10)
        .Data(&staff)
        .Selectable(true)
        .Sortable(true)
        .Build();

    // ── Root layout ─────────────────────────────────────────────────
    auto root = ftxui::ui::HSplit(
        ftxui::ui::Panel("Employees", table),
        ftxui::ui::Panel("Add New",   form));

    root |= ftxui::ui::WithNotifications();
    root |= ftxui::ui::WithConfirm(
        "Quit without saving?",
        [&]{ ftxui::App::Active()->Exit(); },
        [&]{ show_confirm = false; },
        &show_confirm);

    ftxui::ui::RunFullscreen(root);
}
```
