# FTXUI BadWolf API Reference

> Single-include: `#include "ftxui/ui.hpp"` — all components live in the `ftxui::ui` namespace.
> Forward declarations only: `#include "ftxui/ui/fwd.hpp"`

---

## Core Infrastructure

### `App`
Entry-point runner. Wrap any `ftxui::Component` and enter the event loop.

```cpp
#include "ftxui/ui/app.hpp"
ftxui::ui::Run(component);           // fit-component loop
ftxui::ui::RunFullscreen(component); // full-terminal loop
```

Key functions: `Run()`, `RunFullscreen()`, `RunFitComponent()`

---

### `Theme`
Centralized style configuration read by all `ftxui::ui` factory functions.

```cpp
#include "ftxui/ui/theme.hpp"
auto t = ftxui::ui::DarkTheme();
ftxui::ui::SetTheme(t);
```

Key members: `primary`, `secondary`, `accent`, `error_color`, `warning_color`  
Key functions: `SetTheme()`, `GetTheme()`, built-in presets (`DarkTheme`, `LightTheme`, `DraculaTheme`, …)

---

### `State<T>`
Observable value with `Get()`/`Set()` and change callbacks.

```cpp
#include "ftxui/ui/state.hpp"
auto count = ftxui::ui::MakeState(0);
count->Set(42);
count->OnChange([](int v){ /* ... */ });
```

Key methods: `Get()`, `Set()`, `OnChange()`, `Reset()`

---

### `MVU<TModel, TMsg>`
Elm/Bubbletea-style Model-View-Update architecture.

```cpp
#include "ftxui/ui/mvu.hpp"
struct Model { int count = 0; };
enum class Msg { Inc, Dec };
auto app = ftxui::ui::MVU<Model, Msg>(
    Model{},
    [](Model m, Msg msg) -> Model { /* update */ return m; },
    [](const Model& m)  -> ftxui::Element { /* view */ return ftxui::text(""); });
```

Key methods: `Dispatch()`, `Build()`

---

## Reactive State & Data Binding

### `Reactive<T>`
Thread-safe observable value. Notifies subscribers on the FTXUI event thread.

```cpp
#include "ftxui/ui/reactive.hpp"
auto name = std::make_shared<ftxui::ui::Reactive<std::string>>("Alice");
name->Set("Bob");
name->Subscribe([](const std::string& v){ /* ... */ });
```

Key methods: `Get()`, `Set()`, `Subscribe()`, `Unsubscribe()`

---

### `ReactiveList<T>`
Observable `std::vector<T>` with per-element change notifications.

```cpp
#include "ftxui/ui/reactive_list.hpp"
auto items = std::make_shared<ftxui::ui::ReactiveList<std::string>>();
items->Push("hello");
items->OnChange([](){ /* refresh */ });
```

Key methods: `Push()`, `Pop()`, `Insert()`, `Remove()`, `Size()`, `At()`, `OnChange()`

---

### `ReactiveDecorators`
Higher-order reactive utilities: `Computed<T>`, `Debounced<T>`, `Throttled<T>`.

```cpp
#include "ftxui/ui/reactive_decorators.hpp"
auto doubled = ftxui::ui::Computed([&]{ return count->Get() * 2; }, {count});
```

---

### `Bind<T>`
Two-way binding between a `Reactive<T>` and a widget field.

```cpp
#include "ftxui/ui/bind.hpp"
auto b = ftxui::ui::MakeBind<std::string>();
auto lens = ftxui::ui::MakeLens(b, &MyStruct::field);
```

Key functions: `MakeBind<T>()`, `MakeLens()`

---

### `ModelBinding` / `DataContext`
Struct-to-form binding helpers and hierarchical data context.

```cpp
#include "ftxui/ui/model_binding.hpp"
#include "ftxui/ui/data_context.hpp"
```

---

## Data Types

### `JsonValue` / `JsonPath` / `ReactiveJson`
Full JSON type system with RFC 8259 parsing, JSONPath subset, and reactive
JSON state.

```cpp
#include "ftxui/ui/json.hpp"
auto doc = ftxui::ui::Json::Parse(R"({"name":"Alice"})");
auto name = ftxui::ui::JsonPath("$.name").Eval(doc);
auto rj = std::make_shared<ftxui::ui::ReactiveJson>(doc);
```

Key types: `JsonValue`, `JsonPath`, `ReactiveJson`  
Key functions: `Json::Parse()`, `Json::Stringify()`  
Key components: `JsonTreeView()`, `JsonTableView()`, `JsonForm()`, `JsonDiff()`

---

## Layout & Structure

### `Layout`
High-level layout primitives: panels, rows, columns, split views, tab views.

```cpp
#include "ftxui/ui/layout.hpp"
auto ui = ftxui::ui::Panel(content, "Title");
auto row = ftxui::ui::Row({left, right});
auto tabs = ftxui::ui::TabView({"Tab1","Tab2"}, {c1, c2});
```

Key functions: `Panel()`, `Row()`, `Column()`, `HSplit()`, `VSplit()`,
`StatusBar()`, `TabView()`

---

### `Grid`
Responsive CSS-grid-like layout.

```cpp
#include "ftxui/ui/grid.hpp"
auto g = ftxui::ui::Grid(3 /*cols*/, items);
```

---

### `Router`
History-based multi-screen navigation.

```cpp
#include "ftxui/ui/router.hpp"
auto r = ftxui::ui::Router();
r->Register("home", homeComponent);
r->Navigate("home");
```

Key methods: `Register()`, `Navigate()`, `Back()`, `Forward()`

---

### `Transitions`
Animated screen transitions: `FadeIn`, `FadeOut`, `SlideIn`, `SlideOut`.

```cpp
#include "ftxui/ui/transitions.hpp"
auto t = ftxui::ui::FadeIn(component);
```

---

## Text & Richness

### `TextStyle`
Fluent builder for all ANSI text attributes and TrueColor.

```cpp
#include "ftxui/ui/rich_text.hpp"
auto s = ftxui::ui::TextStyle().Bold().Fg(ftxui::Color::Red);
auto el = s("Hello, World!");
```

Key methods: `Bold()`, `Italic()`, `Underline()`, `Fg()`, `Bg()`, `Apply()`

---

### `RichText`
Inline markup parser: `[bold red]text[/]`.

```cpp
auto el = ftxui::ui::RichText::Element("[bold]Hello[/] [red]World[/]");
```

Key methods: `Parse()`, `Element()`, `Render()`, `Component()`

---

### `AnsiParser`
Convert ANSI escape sequences to FTXUI Elements.

```cpp
auto el = ftxui::ui::AnsiParser::Element(ansi_string);
```

Key methods: `Parse()`, `Element()`, `Strip()`

---

### `TypewriterText` / `Console` / `ConsolePrompt`
Animated typewriter effect and interactive console.

```cpp
#include "ftxui/ui/typewriter.hpp"
auto tw = ftxui::ui::TypewriterText("Hello...", config);
auto con = ftxui::ui::Console(config);
```

---

### `Markdown`
Render Markdown text as FTXUI Elements.

```cpp
#include "ftxui/ui/markdown.hpp"
auto el = ftxui::ui::MarkdownElement("# Title\n**bold** text");
```

---

## Input Widgets

### `TextInput`
Enhanced single-line text input with validation and placeholder.

```cpp
#include "ftxui/ui/textinput.hpp"
auto input = ftxui::ui::TextInput(&value, "placeholder");
```

---

### `Form`
Fluent multi-field form builder.

```cpp
#include "ftxui/ui/form.hpp"
auto form = ftxui::ui::Form()
    .Title("Login")
    .Field("Username", &username)
    .Field("Password", &password, {.password = true})
    .Submit("OK", onSubmit)
    .Build();
```

Key methods: `Title()`, `Field()`, `Checkbox()`, `Submit()`, `Build()`

---

### `Keymap`
Keybinding registry with auto-generated help overlay.

```cpp
#include "ftxui/ui/keymap.hpp"
auto km = ftxui::ui::Keymap();
km.Bind("q", "Quit", []{ exit(0); });
```

---

### `FilePicker`
Interactive file-system browser.

```cpp
#include "ftxui/ui/filepicker.hpp"
auto fp = ftxui::ui::FilePicker(&selected_path);
```

---

### `ConfigEditor`
Auto-generated editor for key/value configuration maps.

```cpp
#include "ftxui/ui/config_editor.hpp"
auto ed = ftxui::ui::ConfigEditor(&config_map);
```

---

## Data Display

### `DataTable<T>`
Interactive sortable/filterable table bound to a `std::vector<T>`.

```cpp
#include "ftxui/ui/datatable.hpp"
auto table = ftxui::ui::DataTable<Row>(rows, columns);
```

Key methods: `AddColumn()`, `SetData()`, `OnSelect()`, `Build()`

---

### `SimpleTable` / `SortableTable`
Lightweight table components for static or sortable data sets.

```cpp
#include "ftxui/ui/simple_table.hpp"
#include "ftxui/ui/sortable_table.hpp"
```

---

### `VirtualList`
Virtualized list for large data sets (only renders visible rows).

```cpp
#include "ftxui/ui/virtual_list.hpp"
auto vl = ftxui::ui::VirtualList(item_count, renderer);
```

---

### `List<T>`
Interactive item list with optional fuzzy filter.

```cpp
#include "ftxui/ui/list.hpp"
auto list = ftxui::ui::List<std::string>(items, [](const std::string& s){
    return ftxui::text(s);
});
```

---

### `Tree`
Collapsible tree-view component.

```cpp
#include "ftxui/ui/tree.hpp"
auto tree = ftxui::ui::Tree(root_node);
```

---

## Charts & Visualization

### `LineChart` / `BarChart` / `ScatterPlot` / `Histogram` / `Sparkline`
Braille-canvas data visualization.

```cpp
#include "ftxui/ui/charts.hpp"
auto lc = ftxui::ui::LineChart(data_series, title);
auto bc = ftxui::ui::BarChart(categories, values);
auto sp = ftxui::ui::Sparkline(values);
```

---

### `BrailleCanvas`
Low-level 2×4 braille-dot canvas for custom pixel graphics.

```cpp
#include "ftxui/ui/canvas.hpp"
auto canvas = ftxui::ui::BrailleCanvas(width, height);
canvas->DrawLine(x0, y0, x1, y1);
```

---

## Geospatial & Maps

### `GeoJSON` types
`GeoPoint`, `GeoRing`, `GeoGeometry`, `GeoFeature`, `GeoCollection`.

```cpp
#include "ftxui/ui/geojson.hpp"
auto geo = ftxui::ui::ParseGeoJSON(json_string);
auto geo2 = ftxui::ui::LoadGeoJSON("world.geojson");
```

---

### `GeoMap`
Interactive world map with mouse pan/zoom, powered by NE110m data.

```cpp
#include "ftxui/ui/geomap.hpp"
auto map = ftxui::ui::GeoMap();
```

---

### `GlobeMap`
Spinning 3D orthographic globe projection.

```cpp
#include "ftxui/ui/globe.hpp"
auto globe = ftxui::ui::GlobeMap();
```

---

### `GalaxyMap`
3D star-field renderer (StarWars and StarTrek star catalogs).

```cpp
#include "ftxui/ui/galaxy_map.hpp"
auto galaxy = ftxui::ui::GalaxyMap();
```

---

### `WorldMapGeoJSON()`
Built-in Natural Earth 110m world map data (public domain).

```cpp
#include "ftxui/ui/world_map_data.hpp"
std::string geojson = ftxui::ui::WorldMapGeoJSON();
```

---

## Animation & Effects

### `AnimationLoop` / `Tween`
60fps singleton animation loop with easing functions.

```cpp
#include "ftxui/ui/animation.hpp"
ftxui::ui::AnimationLoop::Get().Add(tween);
auto t = ftxui::ui::Tween(0.0, 1.0, duration, ftxui::ui::Easing::EaseInOut);
```

Key easing functions: `Linear`, `EaseIn`, `EaseOut`, `EaseInOut`, `Bounce`, `Elastic`, `Back`

---

### `ParticleSystem`
Configurable particle effects: Explosion, WarpStreaks, Rain, Sparkle.

```cpp
#include "ftxui/ui/particles.hpp"
auto ps = ftxui::ui::ParticleSystem(ftxui::ui::ParticlePreset::Rain);
```

---

## Overlays & Dialogs

### `Dialog`
Modal dialog overlays: confirm, alert, input, help.

```cpp
#include "ftxui/ui/dialog.hpp"
auto ui = ftxui::ui::WithConfirm(base, "Are you sure?", onYes, onNo);
auto ui = ftxui::ui::WithAlert(base, "Error!", onClose);
```

Key functions: `WithConfirm()`, `WithAlert()`, `WithHelp()`, `WithInput()`

---

### `Notification`
Toast notification overlay system.

```cpp
#include "ftxui/ui/notification.hpp"
ftxui::ui::Notify("Saved!", ftxui::ui::Severity::Success);
auto ui = ftxui::ui::WithNotifications()(base_component);
```

Key functions: `Notify()`, `ClearNotifications()`, `WithNotifications()`

---

### `Progress`
Progress bar and spinner components.

```cpp
#include "ftxui/ui/progress.hpp"
auto bar = ftxui::ui::ProgressBar(&progress_0_to_1);
auto spinner = ftxui::ui::Spinner();
```

---

### `Widgets`
Miscellaneous pre-built widgets: `Badge`, `Tag`, `Separator`, `Card`, etc.

```cpp
#include "ftxui/ui/widgets.hpp"
auto badge = ftxui::ui::Badge("NEW", ftxui::Color::Green);
```

---

### `Wizard`
Multi-step form/onboarding wizard.

```cpp
#include "ftxui/ui/wizard.hpp"
auto wiz = ftxui::ui::Wizard()
    .Step("Name", nameStep)
    .Step("Email", emailStep)
    .OnFinish(onDone)
    .Build();
```

---

### `AlertSystem`
LCARS-style tiered alert overlay (Red/Yellow/Blue/AllClear).

```cpp
#include "ftxui/ui/alert.hpp"
ftxui::ui::RedAlert("Warp core breach!");
ftxui::ui::AllClear();
auto ui = ftxui::ui::WithAlertOverlay(base);
```

---

## Navigation & Commands

### `CommandPalette`
VS Code-style searchable command overlay (Ctrl+P).

```cpp
#include "ftxui/ui/command_palette.hpp"
auto ui = ftxui::ui::WithCommandPalette(base, commands);
```

---

### `LogPanel`
Thread-safe scrolling log panel.

```cpp
#include "ftxui/ui/log_panel.hpp"
auto panel = ftxui::ui::LogPanel();
panel->Log("message", ftxui::ui::LogLevel::Info);
```

---

## Theming & Branding

### `LCARS`
Star Trek LCARS layout engine.

```cpp
#include "ftxui/ui/lcars.hpp"
auto screen = ftxui::ui::LCARSScreen(main_content);
auto panel  = ftxui::ui::LCARSPanel(content, "SYSTEM STATUS");
```

Key components: `LCARSPanel()`, `LCARSBar()`, `LCARSScreen()`  
Key themes: Imperial, Rebel, Enterprise, Matrix (via `theme.hpp`)

---

## Networking & Collaboration

### `NetworkReactive`
TCP-based shared reactive state between processes.

```cpp
#include "ftxui/ui/network_reactive.hpp"
auto server = ftxui::ui::NetworkReactiveServer(port, state);
auto client = ftxui::ui::NetworkReactiveClient<MyState>(host, port);
```

---

### `Collab`
Multi-cursor collaborative TUI sessions.

```cpp
#include "ftxui/ui/collab.hpp"
auto server = ftxui::ui::CollabServer(port);
auto client = ftxui::ui::CollabClient(host, port, component);
```

---

### `LiveSource<T>`
Background-thread data polling with reactive binding.

```cpp
#include "ftxui/ui/live_source.hpp"
auto src = std::make_shared<ftxui::ui::HttpJsonSource>("api.example.com", 80, "/data");
auto reactive = ftxui::ui::BindLiveSource(src);
```

Concrete sources: `HttpJsonSource`, `PrometheusSource`, `FileTailSource`,
`StdinSource`  
Widget factories: `LiveLogPanel()`, `LiveMetricsTable()`, `LiveLineChart()`,
`LiveJsonViewer()`

---

## AI & Intelligence

### `NLParser` / `UIGenerator` / `LLMAdapter`
Natural-language → UI intent engine with optional LLM backend.

```cpp
#include "ftxui/ui/llm_bridge.hpp"
ftxui::ui::NLParser parser;
auto intent = parser.Parse("show me a bar chart of sales");
auto component = ftxui::ui::UIGenerator().Generate(intent);
```

Key types: `NLParser`, `UIGenerator`, `LLMAdapter`, `UIIntent`

---

### `Registry`
C++ component marketplace and `badwolf install` CLI integration.

```cpp
#include "ftxui/ui/registry.hpp"
auto& reg = ftxui::ui::Registry::Get();
reg.Register(meta, factory);
auto comp = reg.Create("my-widget", options);
```

Key methods: `Register()`, `Create()`, `List()`, `Find()`

---

## System & Process

### `ProcessPanel`
Spawn and monitor child processes with live stdout/stderr streaming.

```cpp
#include "ftxui/ui/process_panel.hpp"
auto pp = ftxui::ui::ProcessPanel("ls", {"-la"});
pp->Start();
```

---

### `BackgroundTask`
Thread-pool backed async task runner with progress reporting.

```cpp
#include "ftxui/ui/background_task.hpp"
auto task = ftxui::ui::BackgroundTask([](auto& progress){
    progress.Report(0.5f, "Halfway...");
});
task->Run();
```

---

## Developer Tools

### `WithDebugOverlay`
Ctrl+D overlay showing FPS, event log, component tree.

```cpp
#include "ftxui/ui/debug_overlay.hpp"
auto ui = ftxui::ui::WithDebugOverlay(base);
```

---

### `WithInspector`
Ctrl+I overlay for live component tree inspection.

```cpp
#include "ftxui/ui/inspector.hpp"
auto ui = ftxui::ui::WithInspector(base);
```

---

### `EventRecorder`
Record and replay input event sequences.

```cpp
#include "ftxui/ui/event_recorder.hpp"
auto recorder = ftxui::ui::EventRecorder();
recorder->StartRecording();
// ... interact ...
recorder->StopRecording();
recorder->Replay(component);
```

---

### `UIBuilder`
Visual TUI composer — drag-and-drop component assembly at runtime.

```cpp
#include "ftxui/ui/ui_builder.hpp"
auto builder = ftxui::ui::UIBuilder();
auto ui = ftxui::ui::WithUIBuilder(base, builder);
```

---

## WebAssembly & Platform

### `WasmBridge`
Unified native/WASM app runner (Emscripten support).

```cpp
#include "ftxui/ui/wasm_bridge.hpp"
ftxui::ui::WasmBridge::Run(component);  // works on native + WASM
```

---

### `WebGLRenderer`
GPU-accelerated braille canvas via WebGL (WASM only).

```cpp
#include "ftxui/ui/webgl_renderer.hpp"
auto canvas = ftxui::ui::WebGLCanvas(width, height);
```

---

*Generated for FTXUI BadWolf 1.0.0 — https://louiscrocker.github.io/FTXUI_BADWOLF/*
