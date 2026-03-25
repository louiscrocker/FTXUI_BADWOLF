// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file mvu_todo.cpp
/// A classic to-do list built with the ftxui::ui MVU pattern.
///
/// Demonstrates:
///  - Non-trivial model (vector of items with filter state)
///  - std::variant message type
///  - CatchEvent + Keymap for keyboard-driven dispatch
///  - Input component kept outside View (mutable state) wired into MVU

#include <string>
#include <variant>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Model
// ─────────────────────────────────────────────────────────────────────

struct TodoItem {
  std::string text;
  bool done = false;
};

enum class Filter { All, Active, Completed };

struct Model {
  std::vector<TodoItem> items;
  Filter filter = Filter::All;
};

// ── Messages
// ──────────────────────────────────────────────────────────────────

struct MsgAddItem {
  std::string text;
};
struct MsgToggle {
  size_t index;
};
struct MsgRemove {
  size_t index;
};
struct MsgSetFilter {
  Filter filter;
};
struct MsgQuit {};

using Msg =
    std::variant<MsgAddItem, MsgToggle, MsgRemove, MsgSetFilter, MsgQuit>;

// ── Update
// ────────────────────────────────────────────────────────────────────

Model Update(Model m, Msg msg) {
  return std::visit(
      [&](auto&& ev) -> Model {
        using T = std::decay_t<decltype(ev)>;
        if constexpr (std::is_same_v<T, MsgAddItem>) {
          if (!ev.text.empty()) {
            m.items.push_back({ev.text, false});
          }
        } else if constexpr (std::is_same_v<T, MsgToggle>) {
          if (ev.index < m.items.size()) {
            m.items[ev.index].done = !m.items[ev.index].done;
          }
        } else if constexpr (std::is_same_v<T, MsgRemove>) {
          if (ev.index < m.items.size()) {
            m.items.erase(m.items.begin() + static_cast<ptrdiff_t>(ev.index));
          }
        } else if constexpr (std::is_same_v<T, MsgSetFilter>) {
          m.filter = ev.filter;
        } else if constexpr (std::is_same_v<T, MsgQuit>) {
          if (App* a = App::Active()) {
            a->Exit();
          }
        }
        return m;
      },
      msg);
}

// ── View
// ──────────────────────────────────────────────────────────────────────

Element View(const Model& m) {
  const Theme& t = GetTheme();

  // Filter bar.
  auto filter_tab = [&](std::string_view label, Filter f) {
    bool active = m.filter == f;
    auto e = text(std::string(label));
    return active ? e | bold | underlined | color(t.primary)
                  : e | color(t.text_muted);
  };
  auto filter_bar = hbox({
      text("  Show: ") | dim,
      filter_tab("All", Filter::All),
      text("  "),
      filter_tab("Active", Filter::Active),
      text("  "),
      filter_tab("Done", Filter::Completed),
      filler(),
      text("q=quit  ") | dim,
  });

  // Item list.
  Elements items;
  size_t visible = 0;
  for (size_t i = 0; i < m.items.size(); ++i) {
    const auto& item = m.items[i];
    if (m.filter == Filter::Active && item.done) {
      continue;
    }
    if (m.filter == Filter::Completed && !item.done) {
      continue;
    }
    ++visible;

    auto chk = item.done ? text("[✔] ") | color(t.accent)
                         : text("[ ] ") | color(t.text_muted);
    auto lbl =
        item.done ? text(item.text) | dim | strikethrough : text(item.text);
    auto idx_hint = text(std::to_string(i + 1) + ".") | color(t.text_muted) |
                    size(WIDTH, EQUAL, 3);

    items.push_back(hbox({idx_hint, chk, lbl | xflex}));
  }

  Element list_section;
  if (items.empty()) {
    list_section = text(visible == 0 && m.items.empty()
                            ? "   No tasks yet – type one above and press Enter"
                            : "   Nothing matches this filter.") |
                   dim | hcenter;
  } else {
    list_section = vbox(std::move(items)) | yframe;
  }

  // Footer stats.
  size_t done_count = 0;
  for (const auto& it : m.items) {
    if (it.done) {
      ++done_count;
    }
  }
  auto footer = hbox({
      text("  " + std::to_string(m.items.size() - done_count) + " remaining"),
      filler(),
      text(std::to_string(done_count) + " done  ") | dim,
  });

  return vbox({
             text(" ✔ Todo (MVU demo) ") | bold | hcenter | color(t.primary),
             separatorLight(),
             text("  Input row (press Enter to add, Tab to navigate)") | dim,
             separatorLight(),
             filter_bar,
             separatorLight(),
             list_section | size(HEIGHT, GREATER_THAN, 4),
             separator(),
             footer,
         }) |
         borderStyled(t.border_style, t.border_color) |
         size(WIDTH, GREATER_THAN, 52) | center;
}

// ── Entry point
// ───────────────────────────────────────────────────────────────

int main() {
  SetTheme(Theme::Nord());

  Model initial;
  initial.items = {
      {"Learn FTXUI", true},
      {"Build something cool", false},
      {"Profit!", false},
  };

  auto model = std::make_shared<Model>(std::move(initial));

  auto dispatch = [model](Msg msg) {
    *model = Update(*model, msg);
    if (App* app = App::Active()) {
      app->PostEvent(Event::Custom);
    }
  };

  // The Input component is stateful (cursor, text buffer) — keep it outside
  // View so it persists across renders.
  auto input_buf = std::make_shared<std::string>();
  InputOption input_opt;
  input_opt.multiline = false;
  input_opt.on_enter = [dispatch, input_buf] {
    dispatch(MsgAddItem{*input_buf});
    input_buf->clear();
  };
  auto input_comp =
      Input(input_buf.get(), "Add a task and press Enter…", input_opt);

  // Content view: Input + MVU-rendered section stacked.
  auto content = Container::Vertical({input_comp});

  auto root = Renderer(content, [model, input_comp]() -> Element {
    const Theme& t = GetTheme();
    auto input_row = hbox({
        text("  + ") | color(t.accent),
        input_comp->Render() | xflex,
    });
    return vbox({View(*model), input_row}) | flex;
  });

  // Keymap for shortcuts.
  root |=
      Keymap()
          .Bind(
              "q", [dispatch] { dispatch(MsgQuit{}); }, "Quit")
          .Bind(
              "1", [dispatch] { dispatch(MsgSetFilter{Filter::All}); },
              "Show all")
          .Bind(
              "2", [dispatch] { dispatch(MsgSetFilter{Filter::Active}); },
              "Show active")
          .Bind(
              "3", [dispatch] { dispatch(MsgSetFilter{Filter::Completed}); },
              "Show done")
          .AsDecorator();

  App::FitComponent().Loop(root);
  return 0;
}
