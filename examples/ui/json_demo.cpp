// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file json_demo.cpp
/// Showcase of the JSON first-class data type: parser, tree view, table view,
/// form generation, JSONPath evaluation, and JSON diff.

#include <memory>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/json.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Sample data
// ───────────────────────────────────────────────────────────────

static const std::string kUserJson = R"({
  "users": [
    {"id": 1, "name": "Alice", "role": "admin", "active": true},
    {"id": 2, "name": "Bob",   "role": "viewer", "active": false},
    {"id": 3, "name": "Carol", "role": "editor", "active": true}
  ],
  "meta": {"total": 3, "page": 1}
})";

static const std::string kServerJson = R"([
  {"host": "web-01", "cpu": 12.5, "mem": 45.2, "status": "ok"},
  {"host": "web-02", "cpu": 87.3, "mem": 78.1, "status": "warn"},
  {"host": "db-01",  "cpu":  3.1, "mem": 22.4, "status": "ok"},
  {"host": "cache",  "cpu":  5.0, "mem": 61.0, "status": "ok"}
])";

// ── Tab 1: Parser
// ─────────────────────────────────────────────────────────────

Component MakeParserTab() {
  auto raw_input =
      std::make_shared<std::string>("{\"hello\": \"world\", \"n\": 42}");
  auto parse_result = std::make_shared<JsonValue>();
  auto error_msg = std::make_shared<std::string>();
  auto parsed = std::make_shared<bool>(false);

  InputOption opt;
  opt.placeholder = "Enter JSON here...";
  opt.on_change = [raw_input, parse_result, error_msg, parsed]() {
    *error_msg = "";
    *parse_result = Json::ParseSafe(*raw_input, error_msg.get());
    *parsed = error_msg->empty();
  };

  // Parse initial value
  *parse_result = Json::ParseSafe(*raw_input, error_msg.get());
  *parsed = error_msg->empty();

  auto input_comp = Input(raw_input.get(), opt);

  return Renderer(input_comp, [=]() -> Element {
    Element result_elem;
    if (!parsed || !(*parsed)) {
      result_elem = text("Parse error: " + *error_msg) | color(Color::Red);
    } else {
      result_elem = JsonElement(*parse_result, 2, 8);
    }
    return vbox({
        text("── Tab 1: JSON Parser ──────────────────────") | bold,
        text("Input:") | color(Color::Blue),
        input_comp->Render() | border,
        text("Result:") | color(Color::Blue),
        result_elem | border | flex,
    });
  });
}

// ── Tab 2: Tree View
// ──────────────────────────────────────────────────────────

Component MakeTreeTab() {
  auto val = Json::ParseSafe(kUserJson);
  auto tree = JsonTreeView(val, "users");

  return Renderer(tree, [tree]() -> Element {
    return vbox({
        text("── Tab 2: JSON Tree View ───────────────────") | bold,
        text("Use arrow keys + Enter to expand/collapse nodes.") |
            color(Color::GrayLight),
        tree->Render() | border | flex,
    });
  });
}

// ── Tab 3: Table View
// ─────────────────────────────────────────────────────────

Component MakeTableTab() {
  auto rj = std::make_shared<ReactiveJson>(Json::ParseSafe(kServerJson));
  auto table = JsonTableView(rj);

  auto refresh_btn = Button("Randomize CPU", [rj]() {
    auto arr = rj->Get();
    if (arr.is_array()) {
      for (size_t i = 0; i < arr.size(); ++i) {
        auto& row = arr[i];
        (void)row;
      }
      // Patch CPU of first entry to simulate update
      auto patched = rj->Get();
      if (patched.is_array() && !patched.as_array().empty()) {
        patched[0u].set(
            "cpu", JsonValue(static_cast<double>(rand() % 100)));  // NOLINT
        rj->Set(patched);
      }
    }
  });

  auto container = Container::Vertical({table, refresh_btn});
  return Renderer(container, [=]() -> Element {
    return vbox({
        text("── Tab 3: JSON Table View ──────────────────") | bold,
        text("Live ReactiveJson — press the button to simulate update.") |
            color(Color::GrayLight),
        table->Render() | flex,
        refresh_btn->Render(),
    });
  });
}

// ── Tab 4: Form
// ───────────────────────────────────────────────────────────────

Component MakeFormTab() {
  auto schema =
      Json::Parse(R"({"name":"","email":"","age":0,"newsletter":false})");
  auto last_submitted = std::make_shared<JsonValue>();

  auto form = JsonForm(schema, [last_submitted](JsonValue v) {
    *last_submitted = std::move(v);
  });

  return Renderer(form, [form, last_submitted]() -> Element {
    Element submitted_elem =
        text("(not yet submitted)") | color(Color::GrayDark);
    if (!last_submitted->is_null()) {
      submitted_elem = JsonElement(*last_submitted, 2, 5);
    }
    return vbox({
        text("── Tab 4: JSON Form ────────────────────────") | bold,
        hbox({
            form->Render() | flex,
            vbox({
                text("Last submitted:") | bold | color(Color::Blue),
                submitted_elem | border,
            }) | flex,
        }),
    });
  });
}

// ── Tab 5: JSONPath
// ───────────────────────────────────────────────────────────

Component MakePathTab() {
  auto root = Json::ParseSafe(kUserJson);
  auto path_str = std::make_shared<std::string>("$.users[0].name");
  auto result_val = std::make_shared<JsonValue>();

  auto eval = [root, path_str, result_val]() {
    JsonPath jp(*path_str);
    if (jp.valid()) {
      *result_val = jp.Get(root);
    } else {
      *result_val = JsonValue{};
    }
  };
  eval();

  InputOption opt;
  opt.placeholder = "$.path.expression";
  opt.on_change = eval;

  auto input = Input(path_str.get(), opt);
  return Renderer(input, [=]() -> Element {
    std::string validity =
        JsonPath(*path_str).valid() ? " ✓ valid" : " ✗ invalid";
    Color vc = JsonPath(*path_str).valid() ? Color::Green : Color::Red;
    return vbox({
        text("── Tab 5: JSONPath Evaluator ───────────────") | bold,
        text("Data:") | color(Color::Blue),
        JsonElement(root, 2, 4) | border,
        hbox({text("Path: "), input->Render() | flex}),
        text(validity) | color(vc),
        text("Result:") | color(Color::Blue),
        JsonElement(*result_val, 2, 5) | border,
    });
  });
}

// ── Tab 6: Diff
// ───────────────────────────────────────────────────────────────

Component MakeDiffTab() {
  auto str_a =
      std::make_shared<std::string>(R"({"x":1,"y":2,"shared":"same"})");
  auto str_b =
      std::make_shared<std::string>(R"({"x":99,"z":3,"shared":"same"})");
  auto val_a = std::make_shared<JsonValue>(Json::ParseSafe(*str_a));
  auto val_b = std::make_shared<JsonValue>(Json::ParseSafe(*str_b));

  InputOption opta;
  opta.on_change = [str_a, val_a]() { *val_a = Json::ParseSafe(*str_a); };
  InputOption optb;
  optb.on_change = [str_b, val_b]() { *val_b = Json::ParseSafe(*str_b); };

  auto input_a = Input(str_a.get(), opta);
  auto input_b = Input(str_b.get(), optb);
  auto container = Container::Vertical({input_a, input_b});

  return Renderer(container, [=]() -> Element {
    return vbox({
        text("── Tab 6: JSON Diff ────────────────────────") | bold,
        hbox({
            vbox({text("A:") | color(Color::Red), input_a->Render() | border}) |
                flex,
            vbox({text("B:") | color(Color::Green),
                  input_b->Render() | border}) |
                flex,
        }),
        text("Diff:") | bold | color(Color::Blue),
        JsonDiff(*val_a, *val_b) | flex,
    });
  });
}

// ── Main
// ──────────────────────────────────────────────────────────────────────

int main() {
  auto screen = ScreenInteractive::Fullscreen();

  int selected_tab = 0;
  std::vector<std::string> tab_names = {"Parser", "Tree", "Table",
                                        "Form",   "Path", "Diff"};

  auto parser_tab = MakeParserTab();
  auto tree_tab = MakeTreeTab();
  auto table_tab = MakeTableTab();
  auto form_tab = MakeFormTab();
  auto path_tab = MakePathTab();
  auto diff_tab = MakeDiffTab();

  auto tab_content = Container::Tab(
      {
          parser_tab,
          tree_tab,
          table_tab,
          form_tab,
          path_tab,
          diff_tab,
      },
      &selected_tab);

  auto tab_toggle = Toggle(&tab_names, &selected_tab);

  auto main_container = Container::Vertical({
      tab_toggle,
      tab_content,
  });

  auto renderer = Renderer(main_container, [&]() -> Element {
    return vbox({
               text("FTXUI JSON Demo") | bold | color(Color::Blue) | hcenter,
               separator(),
               tab_toggle->Render(),
               separator(),
               tab_content->Render() | flex,
               separator(),
               text("q: quit") | color(Color::GrayDark) | hcenter,
           }) |
           border;
  });

  auto with_quit = CatchEvent(renderer, [&](Event e) {
    if (e == Event::Character('q')) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });

  screen.Loop(with_quit);
  return 0;
}
