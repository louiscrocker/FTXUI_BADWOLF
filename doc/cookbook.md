# FTXUI Cookbook

Copy-paste recipes for common tasks. All examples use `#include "ftxui/ui.hpp"` and
`using namespace ftxui; using namespace ftxui::ui;` unless noted.

---

## Layout

### How do I split the screen 50/50 horizontally?
```cpp
auto layout = hbox({
  Panel("Left",  left_comp)  | flex,
  Panel("Right", right_comp) | flex,
});
```
Or with a specific pixel split:
```cpp
auto layout = HSplit(left_comp, right_comp, 40); // left gets 40 columns
```

### How do I split vertically?
```cpp
auto layout = vbox({
  top_element    | flex,
  separator(),
  bottom_element,
});
// or:
auto layout = VSplit(top_comp, bottom_comp, 70); // top gets 70% height
```

### How do I make a sidebar + main area?
```cpp
auto root = HSplit(
  Panel("Navigation", sidebar_comp),
  Panel("Content",    main_comp),
  20   // sidebar gets 20 columns
);
```

### How do I center something?
```cpp
text("Hello") | center         // both axes
text("Hello") | hcenter        // horizontal only
text("Hello") | vcenter        // vertical only
```

### How do I add tabs?
```cpp
auto tabs = TabView()
  .Tab("Home",     home_comp)
  .Tab("Settings", settings_comp)
  .Tab("About",    about_comp)
  .Build();
```

### How do I make a status bar?
```cpp
auto root = StatusBar(main_comp, []{ return "Ready  col:1  row:1"; });
```

---

## Text and Styling

### How do I style text?
```cpp
text("bold")      | bold
text("italic")    | italic
text("error")     | color(Color::Red)
text("highlight") | bgcolor(Color::Blue)
text("dim")       | dim
text("blink")     | blink
text("under")     | underlined
text("strike")    | strikethrough
```

### How do I use true color (RGB)?
```cpp
text("custom") | color(Color::RGB(255, 128, 0))
text("bg")     | bgcolor(Color::RGB(30, 30, 50))
```

### How do I make a horizontal separator with a label?
```cpp
LabeledSeparator("Advanced Options")
```

### How do I render a keyboard shortcut hint?
```cpp
hbox({ text("Press "), Kbd("Ctrl+S"), text(" to save") })
```

### How do I show a badge (notification count)?
```cpp
text("Inbox") | Badge(5)                  // red badge
text("Alerts") | Badge(2, Color::Yellow)  // yellow badge
text("New")    | Badge("NEW", Color::Green)
```

---

## Progress and Loading

### How do I make a progress bar?
```cpp
float progress = 0.0f;
return gauge(progress) | color(Color::Cyan); // low-level

// or themed with label:
auto bar = ThemedProgressBar(&progress, "Uploading");
```

### How do I animate a loading spinner?
```cpp
bool loading = false;
my_component |= WithSpinner("Loading data…", &loading);
// loading = true shows the spinner overlay
```

### How do I run work in a background thread and update the UI?
```cpp
State<std::string> result("(pending)");
bool loading = false;

BackgroundTask::RunAsync<std::string>(
  []() -> std::string {
    // runs on a worker thread
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return "done!";
  },
  [&](std::string r) {
    // called on the UI thread
    result.Set(r);
    loading = false;
  }
);
loading = true;
```

---

## Forms and Input

### How do I build a form?
```cpp
std::string name, email, password;
bool tos = false;
int plan = 0;
const std::vector<std::string> plans = {"Free", "Pro"};

auto form = Form()
  .Title("Sign Up")
  .Field("Name",     &name)
  .Field("Email",    &email)
  .Password("Password", &password)
  .Select("Plan",    &plans, &plan)
  .Check("I agree to TOS", &tos)
  .Submit("Create Account", [&]{ /* process... */ App::Active()->Exit(); })
  .Cancel("Cancel", [&]{ App::Active()->Exit(); });

RunFullscreen(form.Build());
```

### How do I add validation to an input?
```cpp
auto field = TextInput()
  .Label("Email")
  .Validator([](const std::string& s){
    return s.find('@') != std::string::npos;
  })
  .ErrorMessage("Must be a valid email")
  .Build();
```

### How do I add an integer spinner?
```cpp
int salary = 50000;
// in a Form:
form.Integer("Salary", &salary, 0, 500000, 1000); // min, max, step
```

---

## Charts

### How do I make a line chart?
```cpp
std::vector<std::pair<float,float>> data = {{0,0},{1,1},{2,0.5f},{3,2}};

auto chart = LineChart()
  .Title("My Data")
  .Series("series1", data, Color::Cyan)
  .Domain(0.f, 3.f)
  .ShowAxes(true).ShowLegend(true)
  .Build();
```

### How do I make a bar chart?
```cpp
auto chart = BarChart()
  .Title("Monthly Revenue")
  .Bar("Jan", 420.f, Color::Cyan)
  .Bar("Feb", 380.f, Color::Yellow)
  .Bar("Mar", 510.f, Color::Green)
  .Orientation(BarChart::Vertical)
  .ShowValues(true)
  .Build();
```

### How do I make a scatter plot?
```cpp
auto chart = ScatterPlot()
  .Title("Clusters")
  .Series("A", cluster_a, Color::Cyan)
  .Series("B", cluster_b, Color::Yellow)
  .Build();
```

### How do I make a histogram?
```cpp
std::vector<float> samples = { /* ... */ };
auto hist = Histogram()
  .Title("Distribution")
  .Data(samples)
  .Bins(20)
  .Build();
```

### How do I add a sparkline (inline mini-chart)?
```cpp
std::vector<float> recent = {1,3,2,5,4,6,5,7};
return Sparkline(recent); // returns an Element
```

---

## Maps and Globe

### How do I render a world map?
```cpp
// Using the bundled world data:
auto map = GeoMap()
  .UseBuiltinWorld()
  .Projection(Projection::Equirectangular)
  .ShowGraticule(true)
  .Build();
RunFullscreen(map);
```

### How do I load and render a GeoJSON file?
```cpp
auto geo = LoadGeoJSON("path/to/data.geojson");
if (!geo) { /* handle error */ }

auto map = GeoMap()
  .Data(*geo)
  .Projection(Projection::Mercator)
  .LineColor(Color::GreenLight)
  .Build();
```

### How do I spin a 3D globe?
```cpp
auto globe = GlobeMap()
  .LineColor(Color::GreenLight)
  .RotationSpeed(1.0)
  .ShowGraticule(true)
  .GraticuleStep(30.f)
  .Build();
RunFullscreen(globe);
```

---

## Tables and Lists

### How do I make a sortable table?
```cpp
struct Row { std::string name; int value; };
std::vector<Row> data = { {"Alpha", 3}, {"Beta", 1}, {"Gamma", 2} };

auto table = DataTable<Row>()
  .Column("Name",  [](const Row& r){ return r.name; })
  .Column("Value", [](const Row& r){ return std::to_string(r.value); })
  .Data(&data)
  .Sortable(true)
  .Build();
```

Click a column header or press `<`/`>` to sort.

### How do I make a filterable list?
```cpp
std::vector<std::string> items = {"Apple","Banana","Cherry","Date"};

auto list = List<std::string>()
  .Items(&items)
  .Label([](const std::string& s){ return s; })
  .Filterable(true)
  .OnSelect([](const std::string& s){ /* selected */ })
  .Build();
```

### How do I make a tree view?
```cpp
auto root = TreeNode::Branch("Root", {
  TreeNode::Leaf("Item 1"),
  TreeNode::Branch("Folder", {
    TreeNode::Leaf("Item 2"),
    TreeNode::Leaf("Item 3"),
  }),
});

auto tree = Tree().Root(root).Build();
```

---

## Navigation

### How do I implement multi-screen navigation?
```cpp
auto router = Router();
router.Add("home",     home_comp);
router.Add("settings", settings_comp);
router.Add("about",    about_comp);
router.Push("home");

// Navigate:
router.Push("settings");    // push to history
router.Pop();               // go back
router.Replace("about");    // replace current
```

### How do I add a command palette (Ctrl+P)?
```cpp
bool show_palette = false;

auto palette = CommandPalette()
  .Command("Open File",    [&]{ /* ... */ }, "File")
  .Command("Toggle Theme", [&]{ /* ... */ }, "View")
  .Command("Quit",         [&]{ App::Active()->Exit(); }, "App")
  .Build();

my_component |= WithModal(palette, &show_palette);
my_component |= CatchEvent([&](Event e){
  if (e == Event::CtrlP) { show_palette = true; return true; }
  return false;
});
```

### How do I build a wizard / multi-step flow?
```cpp
auto wiz = Wizard()
  .Step("Welcome",  welcome_comp)
  .Step("Config",   config_comp)
  .Step("Confirm",  confirm_comp)
  .OnFinish([&]{ App::Active()->Exit(); })
  .Build();
```

---

## Dialogs and Overlays

### How do I show a confirmation dialog?
```cpp
bool show = false;

my_comp |= WithConfirm(
  "Are you sure?",
  [&]{ do_it(); show = false; },   // confirmed
  [&]{ show = false; },            // cancelled
  &show
);

// trigger with: show = true;
```

### How do I show an alert?
```cpp
bool show = false;
my_comp |= WithAlert("Operation failed!", [&]{ show = false; }, &show);
```

### How do I show toast notifications?
```cpp
// Wrap your root component once:
root |= WithNotifications();

// Then from anywhere (any thread):
Notify("File saved!",        Severity::Success);
Notify("Low disk space",     Severity::Warning);
Notify("Connection lost",    Severity::Error);
Notify("Update available",   Severity::Info);
```

### How do I show a slide-in drawer?
```cpp
bool open = false;
my_comp |= WithDrawer(DrawerSide::Right, drawer_comp, &open);
// open = true to show, false to hide
```

---

## Theming

### How do I apply a theme?
```cpp
SetTheme(Theme::Nord());     // call before building components
```

Available presets: `Default`, `Dark`, `Light`, `Nord`, `Dracula`, `Monokai`.

### How do I create a custom theme?
```cpp
SetTheme(Theme::Dark()
  .WithPrimary(Color::Magenta)
  .WithSecondary(Color::Blue)
  .WithBorderStyle(ROUNDED)
  .WithAnimations(true));
```

### How do I temporarily override the theme?
```cpp
{
  ScopedTheme scoped(Theme::Light());
  // components built here use Light theme
} // restored after scope
```

### How do I save and load theme preferences?
```cpp
SaveTheme("~/.config/myapp/theme");   // saves as JSON
LoadTheme("~/.config/myapp/theme");   // restores on next launch
```

---

## Keyboard Shortcuts

### How do I add keyboard shortcuts with auto-help?
```cpp
auto kb = Keymap()
  .Bind("q",      [&]{ App::Active()->Exit(); }, "Quit")
  .Bind("s",      [&]{ save(); },               "Save")
  .Bind("?",      [&]{ show_help = true; },     "Help")
  .Bind("Ctrl+f", [&]{ open_search(); },        "Find");

my_component |= kb.AsDecorator();

// Render the help table (e.g. inside WithHelp):
bool show_help = false;
my_component |= WithHelp("Keyboard Shortcuts", kb.HelpElement(), &show_help);
```

---

## Filesystem

### How do I show a file picker?
```cpp
std::string selected_path;
bool show = false;

auto picker = FilePicker()
  .Root("/home/user")
  .Filter([](const std::string& p){ return p.ends_with(".txt"); })
  .ShowHidden(false)
  .OnSelect([&](const std::string& path){
    selected_path = path;
    show = false;
  })
  .Build();

my_comp |= WithModal(picker, &show);
```

---

## Configuration

### How do I persist user settings?
```cpp
auto cfg = ConfigEditor()
  .File("~/.config/myapp/settings.json")
  .String("username", "default_user")
  .Int("timeout",     30)
  .Float("scale",     1.0f)
  .Bool("dark_mode",  true)
  .Build();   // auto-loads on build, auto-saves on change
```

---

## Markdown

### How do I render Markdown?
```cpp
std::string md = "# Title\n\nSome **bold** and *italic* text.\n\n- item 1\n- item 2";
auto view = MarkdownComponent(md);
RunFullscreen(view);
```

### How do I make a live Markdown editor+preview?
```cpp
std::string source;
auto input   = Input(&source, "# Write Markdown here...");
auto preview = Renderer([&]{ return MarkdownElement(source); });

auto layout = hbox({
  input->Render()   | border | flex,
  separator(),
  preview->Render() | border | flex,
});
```

---

## Process Runner

### How do I run a subprocess and show output?
```cpp
#include "ftxui/ui/process_panel.hpp"

auto panel = ProcessPanel::Create();
panel->SetCommand("cmake --build . --parallel 8");
panel->Start();

RunFullscreen(panel->AsComponent());
```

### How do I stream subprocess output into a log panel?
```cpp
#include "ftxui/ui/log_panel.hpp"

auto log = LogPanel::Create();
log->AppendLine("Starting build...");

// from a background thread:
std::thread([&]{
  // ... run process, append lines ...
  log->AppendLine("Build complete.");
}).detach();

RunFullscreen(log->AsComponent());
```

---

## App Runners

### Which `Run*` function should I use?

| Function | When to use |
|---|---|
| `RunFullscreen(comp)` | Full alternate-screen TUI (most common) |
| `Run(comp)` | Scrolling terminal output, no alt-screen |
| `RunFitComponent(comp)` | Window sized to rendered output |
| `RunFixed(comp, w, h)` | Fixed-size window |
| `App::Fullscreen().Loop(comp)` | Same as `RunFullscreen` but returns exit code |

---

## Low-Level DOM

### How do I draw a custom braille canvas?
```cpp
return canvas(200, 50, [](Canvas& c) {
  // Draw a diagonal line in braille pixels
  c.DrawPointLine(0, 0, 199, 49, Color::Cyan);
  // Draw a circle
  for (int i = 0; i < 360; i++) {
    float rad = i * 3.14159f / 180.f;
    c.DrawPoint(100 + (int)(80 * std::cos(rad)),
                25 + (int)(20 * std::sin(rad)),
                Color::White);
  }
  c.DrawText(90, 24, "center");
});
```

### How do I make a table with the DOM API?
```cpp
auto t = Table({
  {"Name",  "Score", "Grade"},
  {"Alice", "98",    "A+"},
  {"Bob",   "72",    "C"},
});
t.SelectAll().Border(LIGHT);
t.SelectRow(0).Decorate(bold);
t.SelectColumn(2).DecorateCells(color(Color::Cyan));
return t.Render();
```

---

## Tips

- **Auto-redraw:** returning `true` from `CatchEvent` triggers a redraw.
- **Thread safety:** `State<T>::Set()` / `Mutate()` and `Notify()` are safe to call from any thread.
- **Exit from anywhere:** `App::Active()->Exit()` works from any callback.
- **Compose components:** use `|=` to stack decorators: `comp |= CatchEvent(...); comp |= WithSpinner(...);`
