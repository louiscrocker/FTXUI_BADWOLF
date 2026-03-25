// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file ftdb/main.cpp
/// SQLite database browser TUI.
///
/// Layout:
///   Left (20%): Tree of tables/views
///   Right top (75%): Query results as a scrollable table
///   Right bottom: SQL input + execute + error display
///   Status bar: row count, query time, database path
///
/// Usage: ftdb [database.db]
/// Controls: Tab=focus SQL input, Ctrl+E=execute query, q=quit (from table)

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/filepicker.hpp"
#include "ftxui/ui/reactive.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/ui/virtual_list.hpp"

using namespace ftxui;
using namespace ftxui::ui;
using namespace std::chrono;

// ---------------------------------------------------------------------------
// Database wrapper
// ---------------------------------------------------------------------------

struct QueryResult {
  std::vector<std::string> columns;
  std::vector<std::vector<std::string>> rows;
  std::string error;
  double elapsed_ms = 0.0;
};

struct DbState {
  sqlite3* db = nullptr;
  std::string path;

  ~DbState() {
    if (db) { sqlite3_close(db); db = nullptr; }
  }

  bool Open(const std::string& p, std::string& err) {
    if (db) { sqlite3_close(db); db = nullptr; }
    int rc = sqlite3_open(p.c_str(), &db);
    if (rc != SQLITE_OK) {
      err = sqlite3_errmsg(db);
      sqlite3_close(db);
      db = nullptr;
      return false;
    }
    path = p;
    return true;
  }

  std::vector<std::string> GetTableNames() {
    std::vector<std::string> names;
    if (!db) return names;
    const char* sql = "SELECT name FROM sqlite_master "
                      "WHERE type IN ('table','view') ORDER BY name;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
      return names;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      if (n) names.emplace_back(n);
    }
    sqlite3_finalize(stmt);
    return names;
  }

  QueryResult Execute(const std::string& sql_str) {
    QueryResult res;
    if (!db) { res.error = "No database open."; return res; }

    auto t0 = steady_clock::now();
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql_str.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
      res.error = sqlite3_errmsg(db);
      return res;
    }

    int cols = sqlite3_column_count(stmt);
    for (int i = 0; i < cols; ++i) {
      const char* n = sqlite3_column_name(stmt, i);
      res.columns.push_back(n ? n : "");
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      std::vector<std::string> row;
      for (int i = 0; i < cols; ++i) {
        const char* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
        row.push_back(v ? v : "NULL");
      }
      res.rows.push_back(std::move(row));
      if (res.rows.size() >= 1000) break;  // safety cap
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
      res.error = sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);

    auto t1 = steady_clock::now();
    res.elapsed_ms = duration<double, std::milli>(t1 - t0).count();
    return res;
  }
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
  SetTheme(Theme::Dark());
  const Theme& theme = GetTheme();

  auto db = std::make_shared<DbState>();
  std::string open_error;

  // Determine which database to open
  std::string db_path;
  if (argc > 1) {
    db_path = argv[1];
  }

  // ── Reactive state ────────────────────────────────────────────────────────
  auto table_names = std::make_shared<std::vector<std::string>>();
  Reactive<QueryResult> query_result;
  Reactive<std::string> status_msg{std::string("No database open.")};
  Reactive<std::string> error_msg{std::string("")};
  Reactive<bool> show_picker{db_path.empty()};

  int result_scroll = 0;
  int table_selected = 0;

  std::string sql_input;
  bool sql_focused = false;

  auto open_db = [&](const std::string& path) {
    if (db->Open(path, open_error)) {
      auto names = db->GetTableNames();
      *table_names = names;
      status_msg.Set("Opened: " + path + " (" + std::to_string(names.size()) + " tables)");
      error_msg.Set("");
      show_picker.Set(false);
    } else {
      error_msg.Set("Cannot open " + path + ": " + open_error);
    }
  };

  auto run_query = [&](const std::string& sql_str) {
    if (!db->db) { error_msg.Set("No database open."); return; }
    QueryResult res = db->Execute(sql_str);
    query_result.Set(res);
    if (!res.error.empty()) {
      error_msg.Set("Error: " + res.error);
      status_msg.Set("Query failed.");
    } else {
      error_msg.Set("");
      char buf[128];
      snprintf(buf, sizeof(buf), "%zu row(s) — %.1f ms | %s",
               res.rows.size(), res.elapsed_ms, db->path.c_str());
      status_msg.Set(buf);
    }
  };

  auto select_table = [&](const std::string& name) {
    sql_input = "SELECT * FROM \"" + name + "\" LIMIT 100;";
    run_query(sql_input);
  };

  if (!db_path.empty()) {
    open_db(db_path);
  }

  // ── Table list (left panel) ───────────────────────────────────────────────
  auto table_list =
      VirtualList<std::string>()
          .Items(table_names)
          .RowHeight(1)
          .Render([&](const std::string& name, bool sel) -> Element {
            Element e = hbox({text(" "), text(name), filler()});
            return sel ? (e | bgcolor(theme.secondary) | color(Color::White)) : e;
          })
          .OnSelect([&](size_t /*i*/, const std::string& name) {
            select_table(name);
          })
          .ShowSearch(true)
          .Build();

  // ── Query result renderer ─────────────────────────────────────────────────
  auto result_renderer = Renderer([&]() -> Element {
    const auto& qr = *query_result;
    if (qr.columns.empty() && qr.rows.empty()) {
      if (!qr.error.empty())
        return text(" Error: " + qr.error) | color(theme.error_color);
      return text("  Run a query to see results.") | color(theme.text_muted);
    }

    // Build table data
    std::vector<std::vector<std::string>> tdata;
    tdata.push_back(qr.columns);
    for (const auto& row : qr.rows)
      tdata.push_back(row);

    auto tbl = Table(tdata);
    tbl.SelectAll().Border(LIGHT);
    tbl.SelectRow(0).Decorate(bold);
    tbl.SelectRow(0).DecorateCells(color(theme.primary));
    tbl.SelectColumns(0, static_cast<int>(qr.columns.size()) - 1)
        .DecorateCells(size(WIDTH, LESS_THAN, 30));

    return tbl.Render() | frame | flex_grow;
  });

  // ── SQL input area ────────────────────────────────────────────────────────
  auto sql_input_comp = Input(&sql_input, "SQL query…");
  sql_input_comp |= CatchEvent([&](Event e) -> bool {
    // Ctrl+E executes
    if (e == Event::Special("\x05")) {  // Ctrl+E
      run_query(sql_input);
      return true;
    }
    return false;
  });

  auto exec_button = Button("Execute (Ctrl+E)", [&] { run_query(sql_input); });

  auto sql_area = Container::Horizontal({sql_input_comp, exec_button});

  auto sql_renderer = Renderer(sql_area, [&]() -> Element {
    const std::string& err = *error_msg;
    Elements elems;
    elems.push_back(hbox({
        text(" SQL: ") | color(theme.text_muted),
        sql_input_comp->Render() | flex,
        text("  "),
        exec_button->Render(),
        text(" "),
    }));
    if (!err.empty()) {
      elems.push_back(hbox({
          text(" ") ,
          text(err) | color(theme.error_color),
      }));
    }
    return vbox(std::move(elems));
  });

  // ── Status bar ────────────────────────────────────────────────────────────
  auto statusbar = Renderer([&]() -> Element {
    return hbox({
        text(" ftdb ") | bold | color(theme.primary),
        separatorLight(),
        text(" ") ,
        text(*status_msg) | color(theme.text_muted),
        filler(),
        text(" Tab") | color(theme.accent), text(":SQL "),
        text("Ctrl+E") | color(theme.accent), text(":run "),
        text("q") | color(theme.accent), text(":quit "),
    });
  });

  // ── File picker (shown when no db arg) ────────────────────────────────────
  auto file_picker_comp = FilePicker()
      .StartDir(".")
      .Filter([](const std::filesystem::path& p) {
        auto ext = p.extension().string();
        return ext == ".db" || ext == ".sqlite" || ext == ".sqlite3";
      })
      .OnConfirm([&](const std::filesystem::path& p) {
        open_db(p.string());
      })
      .Build();

  auto picker_renderer = Renderer(file_picker_comp, [&]() -> Element {
    return window(
        text(" Open SQLite Database "),
        vbox({
            file_picker_comp->Render() | flex,
            separator(),
            text(" Press Enter to open, or pass a path as argument.") |
                color(theme.text_muted),
        })) | flex;
  });

  // ── Main layout ───────────────────────────────────────────────────────────
  // Left: table list  |  Right: results + sql input
  auto left_pane = Container::Vertical({table_list});
  auto right_pane = Container::Vertical({result_renderer, sql_renderer});

  auto left_r = Renderer(left_pane, [&]() -> Element {
    return window(text(" Tables "), table_list->Render() | flex) | flex;
  });

  auto right_r = Renderer(right_pane, [&]() -> Element {
    return vbox({
        result_renderer->Render() | flex,
        separator(),
        sql_renderer->Render(),
    }) | flex;
  });

  auto split = Container::Horizontal({left_r, right_r});
  auto body = Renderer(split, [&]() -> Element {
    return hbox({
        left_r->Render() | size(WIDTH, EQUAL, 22),
        right_r->Render() | flex,
    }) | flex;
  });

  // Top-level: picker OR main body
  auto picker_container = Container::Vertical({picker_renderer});
  auto main_container = Container::Vertical({body, statusbar});

  auto root_body = Renderer([&]() -> Element {
    if (*show_picker) {
      return picker_renderer->Render() | flex;
    }
    return vbox({
        body->Render() | flex,
        separator(),
        statusbar->Render(),
    }) | flex;
  });

  auto all_container = Container::Tab(
      {picker_renderer, split},
      nullptr  // no tab index needed; we render manually
  );

  auto root = Renderer(all_container, [&]() -> Element {
    if (*show_picker) {
      return picker_renderer->Render() | flex;
    }
    return vbox({
        body->Render() | flex,
        separator(),
        statusbar->Render(),
    }) | flex;
  });

  root |= CatchEvent([&](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      if (!sql_focused) {
        if (App* a = App::Active()) a->Exit();
        return true;
      }
    }
    return false;
  });

  RunFullscreen(root);
  return 0;
}
