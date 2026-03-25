// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file bind_demo.cpp
/// Demonstrates all 5 data-binding layers:
///
///  Layer 1 – Bind<T>               two-way reactive binding
///  Layer 2 – BindModel / Lens      struct-to-form binding
///  Layer 3 – ReactiveList<T>       observable collection + DataTable
///  Layer 4 – DataContext<T>        context provider/consumer
///  Layer 5 – Reactive decorators   ShowIf, BindText, ForEach
///
/// Press 'q' or Escape to quit.

#include <cstdlib>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Data models ──────────────────────────────────────────────────────────────

struct LoginModel {
  std::string username;
  std::string password;
  bool remember_me = false;
};

struct Employee {
  std::string name;
  std::string dept;
};

// ── AppState for DataContext demo ────────────────────────────────────────────

struct AppState {
  std::string current_user = "guest";
  bool dark_mode = true;
};

int main() {
  SetTheme(Theme::Nord());
  const Theme& t = GetTheme();

  // ── Layer 2: struct-to-form binding ──────────────────────────────────────
  auto login_model = MakeBind<LoginModel>();

  // Layer 2 form
  auto login_form =
      Form()
          .Title("Login")
          .BindField("Username", login_model, &LoginModel::username)
          .BindPassword("Password", login_model, &LoginModel::password)
          .BindCheckbox("Remember me", login_model, &LoginModel::remember_me)
          .Submit("Submit", [] {})
          .Build();

  // ── Layer 3: ReactiveList + DataTable ────────────────────────────────────
  auto employees = MakeReactiveList<Employee>(
      {{"Alice", "Eng"}, {"Bob", "HR"}, {"Carol", "Eng"}});

  auto table = DataTable<Employee>()
                   .Column(
                       "Name", [](const Employee& e) { return e.name; }, 12)
                   .Column(
                       "Dept", [](const Employee& e) { return e.dept; }, 8)
                   .BindList(employees)
                   .Selectable(true)
                   .Build();

  // Buttons for manipulating the list (created OUTSIDE render lambda).
  static int name_counter = 4;
  auto btn_add = Button("+ Add", [&] {
    employees->Push({"Employee " + std::to_string(name_counter++), "Dev"});
  });
  auto btn_remove = Button("- Remove", [&] {
    if (employees->Size() > 0) {
      employees->Remove(employees->Size() - 1);
    }
  });
  auto btn_clear = Button("Clear", [&] { employees->Clear(); });

  auto table_panel = Container::Vertical({
      table,
      Container::Horizontal({btn_add, btn_remove, btn_clear}),
  });

  // ── Layer 5: BindText + ShowIf ───────────────────────────────────────────
  auto is_visible = std::make_shared<Reactive<bool>>(true);

  // Toggle button (created outside render lambda).
  auto toggle_btn =
      Button("Toggle Panel", [&] { is_visible->Set(!is_visible->Get()); });

  // Status label using BindText.
  auto name_reactive = login_model.AsReactive();
  auto status_label = Renderer([&]() -> Element {
    auto data = login_model.Get();
    std::string msg =
        "User: " + (data.username.empty() ? "(none)" : data.username) +
        " | Employees: " + std::to_string(employees->Size()) +
        " | Remember: " + (data.remember_me ? "yes" : "no");
    return text(msg) | color(t.text_muted) | hcenter;
  });

  auto detail_panel = Renderer([&]() -> Element {
    return vbox({
               text(" Details ") | bold | color(t.primary) | hcenter,
               separatorLight(),
               ShowIf(vbox({
                          text("  This panel can be toggled.") | color(t.text),
                          text("  Use ShowIf to show/hide reactively.") | dim,
                      }),
                      is_visible),
           }) |
           borderLight | size(WIDTH, EQUAL, 30);
  });

  // ── Layer 4: DataContext ─────────────────────────────────────────────────
  auto app_ctx = DataContext<AppState>::Create(AppState{});

  auto ctx_consumer = Renderer([&]() -> Element {
    auto ctx = DataContext<AppState>::Use();
    std::string user = ctx ? ctx->Get().current_user : "no context";
    return text("  Context user: " + user) | color(t.accent);
  });

  auto ctx_wrapped = app_ctx->Provide(ctx_consumer);

  // ── Main layout ──────────────────────────────────────────────────────────
  auto right_panel = Container::Vertical({
      toggle_btn,
      detail_panel,
      ctx_wrapped,
  });

  auto main_content = Container::Horizontal({
      login_form,
      table_panel,
      right_panel,
  });

  auto root = Container::Vertical({main_content, status_label});

  root |= CatchEvent([&](Event event) -> bool {
    if (event == Event::Character('q') || event == Event::Escape) {
      if (App* app = App::Active()) {
        app->Exit();
      }
      return true;
    }
    return false;
  });

  auto framed = Renderer(root, [&]() -> Element {
    return vbox({
               text("  Data Binding Demo  ") | bold | color(t.primary) |
                   hcenter,
               separatorLight(),
               hbox({
                   vbox({
                       text(" Layer 2: BindModel Form ") | bold |
                           color(t.secondary) | hcenter,
                       login_form->Render() | border | flex,
                   }) | size(WIDTH, EQUAL, 35),
                   separator(),
                   vbox({
                       text(" Layer 3: ReactiveList + DataTable ") | bold |
                           color(t.secondary) | hcenter,
                       table->Render() | flex,
                       separatorLight(),
                       hbox({btn_add->Render(), text(" "), btn_remove->Render(),
                             text(" "), btn_clear->Render()}) |
                           hcenter,
                   }) | flex,
                   separator(),
                   vbox({
                       text(" Layers 4 & 5 ") | bold | color(t.secondary) |
                           hcenter,
                       toggle_btn->Render() | hcenter,
                       detail_panel->Render(),
                       separatorLight(),
                       ctx_wrapped->Render(),
                   }) | size(WIDTH, EQUAL, 32),
               }) | flex,
               separatorLight(),
               status_label->Render(),
               text(" q/Esc quit ") | color(t.text_muted) | hcenter,
           }) |
           border;
  });

  RunFullscreen(framed);
  return 0;
}
