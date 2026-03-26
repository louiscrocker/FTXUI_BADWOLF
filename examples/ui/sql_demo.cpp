// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file sql_demo.cpp
/// 6-tab interactive showcase for FTXUI's SQL/DataFrame feature:
///   1. In-Memory DB (SQLite, optional)  4. JSON → DataFrame
///   2. DataFrame operations             5. Aggregates
///   3. CSV Import                       6. Reactive (Add Row)

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Sample data
// ───────────────────────────────────────────────────────────────

static const std::string kEmployeeCSV = R"(id,name,dept,salary
1,Alice,Engineering,95000
2,Bob,Marketing,72000
3,Carol,Engineering,102000
4,Dave,HR,65000
5,Eve,Engineering,88000
6,Frank,Marketing,76000
7,Grace,HR,68000
8,Hank,Engineering,115000
)";

static const std::string kUsersJson = R"([
  {"id": 1, "username": "alice",  "age": 30, "score": 88.5},
  {"id": 2, "username": "bob",    "age": 25, "score": 72.0},
  {"id": 3, "username": "carol",  "age": 35, "score": 95.3},
  {"id": 4, "username": "dave",   "age": 28, "score": 61.8},
  {"id": 5, "username": "eve",    "age": 32, "score": 79.4}
])";

// ── Tab 1: In-Memory SQLite DB
// ────────────────────────────────────────────

Component MakeDbTab() {
  auto db = std::make_shared<SqliteDb>(":memory:");

  if (!db->IsOpen()) {
    return Renderer([]() -> Element {
      return vbox({
          text("── Tab 1: In-Memory DB ─────────────────────") | bold,
          text(""),
          text("SQLite not available.") | color(Color::Yellow),
          text("Recompile with -DBADWOLF_SQLITE to enable this tab.") |
              color(Color::GrayLight),
      });
    });
  }

  db->Execute(R"(CREATE TABLE employees (
    id     INTEGER PRIMARY KEY,
    name   TEXT NOT NULL,
    dept   TEXT NOT NULL,
    salary REAL NOT NULL
  ))");
  db->Execute("INSERT INTO employees VALUES (1,'Alice','Engineering',95000)");
  db->Execute("INSERT INTO employees VALUES (2,'Bob','Marketing',72000)");
  db->Execute("INSERT INTO employees VALUES (3,'Carol','Engineering',102000)");
  db->Execute("INSERT INTO employees VALUES (4,'Dave','HR',65000)");
  db->Execute("INSERT INTO employees VALUES (5,'Eve','Engineering',88000)");
  db->Execute("INSERT INTO employees VALUES (6,'Frank','Marketing',76000)");

  auto rq = std::make_shared<ReactiveQuery<DataFrame>>(
      db, "SELECT id, name, dept, salary FROM employees ORDER BY salary DESC");
  auto rdf = std::make_shared<ReactiveDataFrame>(rq->Latest());
  rq->OnChange([rdf](const DataFrame& df) { rdf->Set(df); });

  auto table = DataFrameTable(rdf, "employees");

  auto refresh_btn = Button("Refresh Query", [rq]() { rq->Refresh(); });

  auto container = Container::Vertical({table, refresh_btn});
  return Renderer(container, [=]() -> Element {
    return vbox({
        text("── Tab 1: In-Memory SQLite DB ──────────────") | bold,
        text("Live ReactiveQuery — employees sorted by salary desc.") |
            color(Color::GrayLight),
        table->Render() | flex,
        refresh_btn->Render(),
    });
  });
}

// ── Tab 2: DataFrame operations
// ──────────────────────────────────────────

Component MakeDataFrameTab() {
  auto df_all = DataFrame::FromCSV(kEmployeeCSV);
  auto df_eng =
      df_all.FilterWhere("dept", SqlValue(std::string("Engineering")));
  auto df_top3 = df_all.SortBy("salary", false).Head(3);

  auto t_all = df_all.AsTable("All Employees");
  auto t_eng = df_eng.AsTable("Engineering Only");
  auto t_top3 = df_top3.AsTable("Top 3 Salaries");

  int selected = 0;
  std::vector<std::string> views = {"All", "Filter: Engineering", "Top 3"};

  auto toggle = Toggle(&views, &selected);
  auto tab_content = Container::Tab(
      {
          Renderer([t_all]() { return t_all->Render() | flex; }),
          Renderer([t_eng]() { return t_eng->Render() | flex; }),
          Renderer([t_top3]() { return t_top3->Render() | flex; }),
      },
      &selected);

  auto container = Container::Vertical({toggle, tab_content});
  return Renderer(container, [=]() -> Element {
    return vbox({
        text("── Tab 2: DataFrame Operations ─────────────") | bold,
        text("Filter / SortBy / Head on an in-memory DataFrame.") |
            color(Color::GrayLight),
        toggle->Render(),
        separator(),
        tab_content->Render() | flex,
    });
  });
}

// ── Tab 3: CSV Import
// ────────────────────────────────────────────────────

Component MakeCsvTab() {
  auto csv_str = std::make_shared<std::string>(kEmployeeCSV);
  auto rdf = std::make_shared<ReactiveDataFrame>(DataFrame::FromCSV(*csv_str));
  auto error_msg = std::make_shared<std::string>();

  auto table = DataFrameTable(rdf, "Parsed CSV");

  InputOption opt;
  opt.multiline = true;
  auto input = Input(csv_str.get(), opt);

  auto parse_btn = Button("Parse CSV", [csv_str, rdf, error_msg]() {
    *error_msg = "";
    DataFrame df = DataFrame::FromCSV(*csv_str);
    if (df.empty()) {
      *error_msg = "CSV produced an empty DataFrame — check format.";
    }
    rdf->Set(df);
  });

  auto container = Container::Vertical({input, parse_btn, table});
  return Renderer(container, [=]() -> Element {
    Elements rows = {
        text("── Tab 3: CSV Import ───────────────────────") | bold,
        text("Edit CSV below, then press Parse.") | color(Color::GrayLight),
        input->Render() | border | size(HEIGHT, LESS_THAN, 10),
        parse_btn->Render(),
    };
    if (!error_msg->empty()) {
      rows.push_back(text(*error_msg) | color(Color::Red));
    }
    rows.push_back(table->Render() | flex);
    return vbox(rows);
  });
}

// ── Tab 4: JSON → DataFrame
// ──────────────────────────────────────────────

Component MakeJsonTab() {
  auto json_val = Json::ParseSafe(kUsersJson);
  auto df = DataFrame::FromJson(json_val);
  auto table = DataFrameTable(df, "JSON Users");

  double sum_score = df.Sum("score");
  double mean_score = df.Mean("score");
  double min_score = df.Min("score");
  double max_score = df.Max("score");
  size_t cnt = df.Count("score");

  auto stats_elem = [=]() -> Element {
    auto row = [](const std::string& label, double val) {
      return hbox({
          text(label) | color(Color::Blue) | size(WIDTH, EQUAL, 8),
          text(std::to_string(val).substr(0, 7)) | bold,
      });
    };
    return vbox({
               text("score stats") | bold | hcenter,
               separator(),
               row("Sum:  ", sum_score),
               row("Mean: ", mean_score),
               row("Min:  ", min_score),
               row("Max:  ", max_score),
               hbox(
                   {text("Count:") | color(Color::Blue) | size(WIDTH, EQUAL, 8),
                    text(std::to_string(cnt)) | bold}),
           }) |
           border;
  };

  return Renderer(table, [=]() -> Element {
    return vbox({
        text("── Tab 4: JSON → DataFrame ─────────────────") | bold,
        text("Parsed from a JSON array; stat panel on the right.") |
            color(Color::GrayLight),
        hbox({
            table->Render() | flex,
            stats_elem() | size(WIDTH, EQUAL, 22),
        }) | flex,
    });
  });
}

// ── Tab 5: Aggregates
// ────────────────────────────────────────────────────

Component MakeAggregatesTab() {
  auto df = DataFrame::FromCSV(kEmployeeCSV);

  std::vector<std::string> numeric_cols = {"salary"};

  struct AggRow {
    std::string col;
    std::string sum, mean, min_v, max_v, count;
  };
  std::vector<AggRow> rows;
  for (const auto& c : numeric_cols) {
    AggRow r;
    r.col = c;
    r.sum = std::to_string(static_cast<int64_t>(df.Sum(c)));
    r.mean = std::to_string(df.Mean(c)).substr(0, 8);
    r.min_v = std::to_string(static_cast<int64_t>(df.Min(c)));
    r.max_v = std::to_string(static_cast<int64_t>(df.Max(c)));
    r.count = std::to_string(df.Count(c));
    rows.push_back(r);
  }

  auto agg_table = DataTable<AggRow>()
                       .Column(
                           "Column", [](const AggRow& r) { return r.col; }, 10)
                       .Column(
                           "Sum", [](const AggRow& r) { return r.sum; }, 12)
                       .Column(
                           "Mean", [](const AggRow& r) { return r.mean; }, 10)
                       .Column(
                           "Min", [](const AggRow& r) { return r.min_v; }, 10)
                       .Column(
                           "Max", [](const AggRow& r) { return r.max_v; }, 10)
                       .Column(
                           "Count", [](const AggRow& r) { return r.count; }, 7)
                       .Data(&rows)
                       .Build();

  auto summary_elem = df.AsSummary();

  return Renderer(agg_table, [=]() mutable -> Element {
    return vbox({
        text("── Tab 5: Aggregates ───────────────────────") | bold,
        text("Sum / Mean / Min / Max / Count on employee salaries.") |
            color(Color::GrayLight),
        agg_table->Render() | flex,
        separator(),
        text("DataFrame::AsSummary():") | color(Color::Blue),
        summary_elem,
    });
  });
}

// ── Tab 6: Reactive (Add Row)
// ─────────────────────────────────────────────

Component MakeReactiveTab() {
  auto rdf = std::make_shared<ReactiveDataFrame>();
  auto counter = std::make_shared<int>(0);

  // Seed with a few initial rows.
  SqlResult seed;
  seed.columns = {"id", "event", "value"};
  for (int i = 1; i <= 3; ++i) {
    seed.rows.push_back({SqlValue(int64_t{i}),
                         SqlValue(std::string("event_" + std::to_string(i))),
                         SqlValue(int64_t{i * 10})});
  }
  *counter = 3;
  rdf->Set(DataFrame(seed));

  auto table = DataFrameTable(rdf, "Reactive Events");

  auto add_btn = Button("Add Row", [rdf, counter]() {
    (*counter)++;
    SqlResult sr = rdf->Get().ToSqlResult();
    sr.rows.push_back(
        {SqlValue(int64_t{*counter}),
         SqlValue(std::string("event_" + std::to_string(*counter))),
         SqlValue(int64_t{*counter * 10})});
    rdf->Set(DataFrame(sr));
  });

  auto clear_btn = Button("Clear", [rdf, counter]() {
    *counter = 0;
    SqlResult empty;
    empty.columns = {"id", "event", "value"};
    rdf->Set(DataFrame(empty));
  });

  auto btns = Container::Horizontal({add_btn, clear_btn});
  auto container = Container::Vertical({table, btns});

  return Renderer(container, [=]() -> Element {
    size_t row_count = rdf->Get().rows();
    return vbox({
        text("── Tab 6: Reactive DataFrame ───────────────") | bold,
        text("Press \"Add Row\" to append a row to the live "
             "ReactiveDataFrame.") |
            color(Color::GrayLight),
        table->Render() | flex,
        hbox({
            add_btn->Render(),
            text("  "),
            clear_btn->Render(),
            text("  Rows: " + std::to_string(row_count)) | color(Color::Blue),
        }),
    });
  });
}

// ── Main
// ──────────────────────────────────────────────────────────────────────

int main() {
  int selected_tab = 0;
  std::vector<std::string> tab_names = {"DB",      "DataFrame",  "CSV",
                                        "JSON→DF", "Aggregates", "Reactive"};

  auto db_tab = MakeDbTab();
  auto df_tab = MakeDataFrameTab();
  auto csv_tab = MakeCsvTab();
  auto json_tab = MakeJsonTab();
  auto agg_tab = MakeAggregatesTab();
  auto reactive_tab = MakeReactiveTab();

  auto tab_content =
      Container::Tab({db_tab, df_tab, csv_tab, json_tab, agg_tab, reactive_tab},
                     &selected_tab);

  auto tab_toggle = Toggle(&tab_names, &selected_tab);

  auto main_container = Container::Vertical({tab_toggle, tab_content});

  auto renderer = Renderer(main_container, [&]() -> Element {
    return vbox({
               text("FTXUI SQL / DataFrame Demo") | bold | color(Color::Cyan) |
                   hcenter,
               separator(),
               tab_toggle->Render(),
               separator(),
               tab_content->Render() | flex,
               separator(),
               text("q: quit  ↑↓: navigate rows  <>: sort columns") |
                   color(Color::GrayDark) | hcenter,
           }) |
           border | flex;
  });

  auto with_quit = CatchEvent(renderer, [&](Event e) {
    if (e == Event::Character('q')) {
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    return false;
  });

  ui::RunFullscreen(with_quit);
  return 0;
}
