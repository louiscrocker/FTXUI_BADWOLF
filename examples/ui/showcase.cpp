// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file showcase.cpp
/// Kitchen-sink demo — all major ftxui::ui components in one app.
/// Router · CommandPalette · DataTable · Tree · Form ·
/// LogPanel · Notifications · Wizard · Keymap · State<T> · Widgets

#include <atomic>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Shared globals ────────────────────────────────────────────────────────────

struct Metrics { float cpu = 0.42f; float ram = 0.67f; };
auto g_metrics = std::make_shared<State<Metrics>>();

std::shared_ptr<Router>   g_router;
std::shared_ptr<LogPanel> g_log;

// ─────────────────────────────────────────────────────────────────────────────
// Home screen
// ─────────────────────────────────────────────────────────────────────────────
Component MakeHome() {
  auto btn_tasks  = Button("  Tasks   ", []{ g_router->Push("tasks");  });
  auto btn_form   = Button("  Form    ", []{ g_router->Push("form");   });
  auto btn_tree   = Button("  Tree    ", []{ g_router->Push("tree");   });
  auto btn_log    = Button("  Log     ", []{ g_router->Push("log");    });
  auto btn_wizard = Button("  Wizard  ", []{ g_router->Push("wizard"); });
  auto btns = Container::Horizontal({btn_tasks, btn_form, btn_tree, btn_log, btn_wizard});

  return Renderer(btns, [=]() -> Element {
    const Theme& t = GetTheme();
    Metrics m = g_metrics->Get();
    return vbox({
        separatorEmpty(),
        text("  ftxui::ui  Showcase  ") | bold | hcenter | color(t.primary),
        text("  Router · DataTable · Tree · Form · Log · Wizard · Palette  ") | dim | hcenter,
        separatorEmpty(),
        separator(),
        separatorEmpty(),
        // Live metrics
        hbox({ text("  CPU  ") | dim,
               gauge(m.cpu) | color(t.primary) | flex,
               text(std::to_string(int(m.cpu * 100)) + "% ") | size(WIDTH, EQUAL, 6) }),
        hbox({ text("  RAM  ") | dim,
               gauge(m.ram) | color(t.accent)  | flex,
               text(std::to_string(int(m.ram * 100)) + "% ") | size(WIDTH, EQUAL, 6) }),
        separatorEmpty(),
        separator(),
        separatorEmpty(),
        text("  Navigate  ") | bold | dim,
        separatorEmpty(),
        hbox({ btn_tasks->Render(),  text(" "),
               btn_form->Render(),   text(" "),
               btn_tree->Render(),   text(" "),
               btn_log->Render(),    text(" "),
               btn_wizard->Render() }) | hcenter,
        separatorEmpty(),
        separator(),
        separatorEmpty(),
        hbox({
            text("  Ctrl+P") | color(t.accent), text("=palette  "),
            text("?") | color(t.accent),         text("=keys  "),
            text("q") | color(t.accent),         text("=quit  "),
        }) | hcenter | dim,
        separatorEmpty(),
    });
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// Tasks screen — DataTable
// ─────────────────────────────────────────────────────────────────────────────
struct TodoItem { std::string title, priority, status; };

Component MakeTasks() {
  static std::vector<TodoItem> todos = {
      {"Refactor auth module",  "High",   "In Progress"},
      {"Write unit tests",      "High",   "Pending"},
      {"Update documentation",  "Medium", "Pending"},
      {"Performance profiling", "Medium", "Done"},
      {"Deploy to staging",     "High",   "Pending"},
      {"Code review PR #47",    "Low",    "Done"},
      {"Fix CSS regression",    "Medium", "In Progress"},
      {"Add dark mode support", "Low",    "Pending"},
      {"Database migration",    "High",   "Done"},
      {"Security audit",        "High",   "Pending"},
  };

  auto tbl = DataTable<TodoItem>()
    .Column("Title",    [](const TodoItem& t){ return t.title; },    22)
    .Column("Priority", [](const TodoItem& t){ return t.priority; },  9)
    .Column("Status",   [](const TodoItem& t){ return t.status; },   13)
    .Data(&todos)
    .Selectable(true)
    .Sortable(true)
    .AlternateRows(true)
    .OnActivate([](const TodoItem& t, size_t) {
        g_log->Log("Activated: " + t.title, LogLevel::Info);
        Notify(t.title, Severity::Info);
    })
    .Build();

  auto back = Button(" ← Home ", []{ g_router->Pop(); });
  auto lay  = Container::Vertical({tbl, back});
  return Renderer(lay, [=]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        hbox({ text("  Tasks  ") | bold | color(t.primary) | flex,
               back->Render() }),
        separator(),
        tbl->Render() | flex,
        separatorEmpty(),
        text("  ↑↓ move · < > sort · Enter=activate  ") | dim | hcenter,
    });
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// Form screen
// ─────────────────────────────────────────────────────────────────────────────
Component MakeForm() {
  static std::string f_title, f_assignee;
  static std::vector<std::string> prio_opts = {"Critical","High","Medium","Low"};
  static int   prio_sel  = 1;
  static bool  f_notify  = false;

  auto frm = Form()
    .Title("  New Issue  ")
    .Field("Title",     &f_title,    "Short description")
    .Field("Assignee",  &f_assignee, "username")
    .Select("Priority", &prio_opts,  &prio_sel)
    .Check("Notify team", &f_notify)
    .Separator()
    .Submit("Create Issue", []{
        std::string msg = "Created: " + (f_title.empty() ? "(untitled)" : f_title);
        g_log->Log(msg, LogLevel::Info);
        Notify(msg, Severity::Success);
        g_router->Pop();
    })
    .Cancel("Cancel", []{ g_router->Pop(); })
    .Build();

  return frm;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tree screen
// ─────────────────────────────────────────────────────────────────────────────
Component MakeTree() {
  auto nav = [](const std::string& p){
    g_log->Log("Opened: " + p, LogLevel::Debug);
    Notify(p, Severity::Info);
  };

  auto tree = Tree()
    .Node(TreeNode::Branch("include/ftxui/ui/", {
        TreeNode::Leaf("theme.hpp",     [&]{ nav("theme.hpp"); }),
        TreeNode::Leaf("form.hpp",      [&]{ nav("form.hpp"); }),
        TreeNode::Leaf("datatable.hpp", [&]{ nav("datatable.hpp"); }),
        TreeNode::Leaf("router.hpp",    [&]{ nav("router.hpp"); }),
        TreeNode::Leaf("wizard.hpp",    [&]{ nav("wizard.hpp"); }),
        TreeNode::Leaf("log_panel.hpp", [&]{ nav("log_panel.hpp"); }),
    }))
    .Node(TreeNode::Branch("src/ftxui/ui/", {
        TreeNode::Leaf("form.cpp",   [&]{ nav("form.cpp"); }),
        TreeNode::Leaf("theme.cpp",  [&]{ nav("theme.cpp"); }),
        TreeNode::Leaf("router.cpp", [&]{ nav("router.cpp"); }),
    }))
    .Node(TreeNode::Leaf("CMakeLists.txt", [&]{ nav("CMakeLists.txt"); }))
    .Build();

  auto back = Button(" ← Home ", []{ g_router->Pop(); });
  auto lay  = Container::Vertical({tree, back});
  return Renderer(lay, [=]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        hbox({ text("  File Tree  ") | bold | color(t.primary) | flex,
               back->Render() }),
        separator(),
        tree->Render() | yframe | flex,
        separatorEmpty(),
        text("  Tab/↑↓ · Enter=open  ") | dim | hcenter,
    });
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// Log screen
// ─────────────────────────────────────────────────────────────────────────────
Component MakeLog() {
  auto back     = Button(" ← Home ", []{ g_router->Pop(); });
  auto log_comp = g_log->Build("  Application Log  ");
  auto lay      = Container::Vertical({back, log_comp});
  return Renderer(lay, [=]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        hbox({ text("  Log  ") | bold | color(t.primary) | flex, back->Render() }),
        separator(),
        log_comp->Render() | flex,
    });
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// Wizard screen
// ─────────────────────────────────────────────────────────────────────────────
Component MakeWizard() {
  static std::string w_name;
  static std::vector<std::string> w_types = {"Library","Executable"};
  static int  w_type  = 0;
  static bool w_tests = true;

  auto step1 = Renderer([]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        separatorEmpty(),
        text("Create a New Project") | bold | hcenter | color(t.primary),
        separatorEmpty(),
        paragraph("This wizard will scaffold a new FTXUI-based project.") | xflex,
        separatorEmpty(),
    });
  });

  auto inp2  = Input(&w_name, "my-app");
  auto lay2  = Container::Vertical({inp2});
  auto step2 = Renderer(lay2, [&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        text("Project Name") | bold | color(t.primary), separatorEmpty(),
        hbox({ text("  Name: ") | dim, inp2->Render() | flex }),
    });
  });

  auto radio3 = Radiobox(&w_types, &w_type);
  auto chk3   = Checkbox("Include tests", &w_tests);
  auto lay3   = Container::Vertical({radio3, chk3});
  auto step3  = Renderer(lay3, [&]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
        text("Options") | bold | color(t.primary), separatorEmpty(),
        radio3->Render(), separatorEmpty(), chk3->Render(),
    });
  });

  auto step4 = Renderer([&]() -> Element {
    const Theme& t = GetTheme();
    std::string n = w_name.empty() ? "my-app" : w_name;
    return vbox({
        text("Review") | bold | color(t.primary), separatorEmpty(),
        hbox({ text("  Name:   ") | dim, text(n) | bold }),
        hbox({ text("  Type:   ") | dim, text(w_types[w_type]) | bold }),
        hbox({ text("  Tests:  ") | dim, text(w_tests ? "Yes" : "No") | bold }),
        separatorEmpty(),
        text("  Press Finish to confirm.") | color(t.accent),
    });
  });

  return Wizard("  New Project  ")
    .Step("Welcome", step1)
    .Step("Name",    step2)
    .Step("Options", step3)
    .Step("Review",  step4)
    .OnComplete([]{
        std::string n = w_name.empty() ? "my-app" : w_name;
        g_log->Log("Project created: " + n, LogLevel::Info);
        Notify("'" + n + "' created!", Severity::Success);
        g_router->Pop();
    })
    .OnCancel([]{ g_router->Pop(); })
    .Build();
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
  SetTheme(Theme::Nord());

  g_log = LogPanel::Create(500);
  g_log->Log("Showcase started (theme: Nord)", LogLevel::Info);

  // Background metric animation
  std::atomic<bool> stop{false};
  std::thread sim([&] {
    int tick = 0;
    while (!stop) {
      std::this_thread::sleep_for(std::chrono::milliseconds(700));
      g_metrics->Mutate([&](Metrics& m) {
        m.cpu = 0.25f + 0.40f * std::abs(std::sin(tick * 0.28f));
        m.ram = 0.50f + 0.25f * std::abs(std::sin(tick * 0.13f));
      });
      if (tick % 8 == 0)
        g_log->Log("Metric update #" + std::to_string(tick), LogLevel::Trace);
      ++tick;
    }
  });

  // Router
  g_router = std::make_shared<Router>();
  g_router->Route("home",   MakeHome)
           .Route("tasks",  MakeTasks)
           .Route("form",   MakeForm)
           .Route("tree",   MakeTree)
           .Route("log",    MakeLog)
           .Route("wizard", MakeWizard)
           .Default("home");

  auto router_comp = g_router->Build();

  // Shell / frame
  auto root = Renderer(router_comp, [&]() -> Element {
    const Theme& t = GetTheme();
    std::string route(g_router->Current());
    std::string crumb = route == "home" ? "home" : "home › " + route;
    return vbox({
               hbox({
                   text(" ◈ ui::showcase ") | bold | color(t.primary),
                   text(crumb) | dim | flex,
                   StatusDot(t.success_color, "live"),
                   text("  "),
               }),
               separator(),
               router_comp->Render() | flex,
           }) | flex;
  });

  // Command Palette
  root = CommandPalette()
    .Bind("Ctrl+P")
    .Add("Home",         []{ g_router->Push("home"); },   "", "Navigate")
    .Add("Tasks",        []{ g_router->Push("tasks"); },  "", "Navigate")
    .Add("Form",         []{ g_router->Push("form"); },   "", "Navigate")
    .Add("Tree",         []{ g_router->Push("tree"); },   "", "Navigate")
    .Add("Log",          []{ g_router->Push("log"); },    "", "Navigate")
    .Add("Wizard",       []{ g_router->Push("wizard"); }, "", "Navigate")
    .Add("Theme: Default", []{ SetTheme(Theme::Default()); Notify("Default", Severity::Info); }, "", "Theme")
    .Add("Theme: Dark",    []{ SetTheme(Theme::Dark());    Notify("Dark",    Severity::Info); }, "", "Theme")
    .Add("Theme: Light",   []{ SetTheme(Theme::Light());   Notify("Light",   Severity::Info); }, "", "Theme")
    .Add("Theme: Nord",    []{ SetTheme(Theme::Nord());    Notify("Nord",    Severity::Info); }, "", "Theme")
    .Add("Theme: Dracula", []{ SetTheme(Theme::Dracula()); Notify("Dracula", Severity::Info); }, "", "Theme")
    .Add("Theme: Monokai", []{ SetTheme(Theme::Monokai()); Notify("Monokai",Severity::Info); }, "", "Theme")
    .Add("Toast: Info",    []{ Notify("Info notification",    Severity::Info); },    "", "Test")
    .Add("Toast: Success", []{ Notify("Build passed!",        Severity::Success); }, "", "Test")
    .Add("Toast: Warning", []{ Notify("Disk usage high",      Severity::Warning); }, "", "Test")
    .Add("Toast: Error",   []{ Notify("Connection refused",   Severity::Error); },   "", "Test")
    .Add("Quit",           [&]{ stop=true; if (App* a = App::Active()) a->Exit(); }, "Exit", "")
    .Wrap(root);

  // Toast overlay
  root = WithNotifications(root);

  // Keymap
  root |= Keymap()
    .Bind("q",      [&]{ stop=true; if (App* a = App::Active()) a->Exit(); }, "Quit")
    .Bind("h",      []{ g_router->Push("home"); },   "Go home")
    .Bind("Escape", []{ g_router->Pop(); },           "Go back")
    .AsDecorator();

  App::Fullscreen().Loop(root);
  stop = true;
  sim.join();
  return 0;
}
