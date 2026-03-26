// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_SQL_HPP
#define FTXUI_UI_SQL_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/json.hpp"

namespace ftxui::ui {

// ── SqlValue — variant for SQL column values ─────────────────────────────────

/// @brief A typed SQL column value (NULL / INTEGER / REAL / TEXT / BLOB).
/// @ingroup ui
class SqlValue {
 public:
  enum class Type { Null, Integer, Real, Text, Blob };

  SqlValue();                // Null
  SqlValue(std::nullptr_t);  // Null
  explicit SqlValue(int64_t v);
  explicit SqlValue(double v);
  explicit SqlValue(std::string v);
  explicit SqlValue(std::vector<uint8_t> v);

  Type type() const;
  bool is_null() const;
  int64_t as_int() const;  // throws std::runtime_error on type mismatch
  double as_real() const;  // throws std::runtime_error on type mismatch
  const std::string& as_text() const;  // throws on mismatch

  int64_t get_int(int64_t def = 0) const;
  double get_real(double def = 0.0) const;
  std::string get_text(const std::string& def = "") const;

  std::string to_string() const;  // display representation

  bool operator==(const SqlValue& o) const;
  bool operator!=(const SqlValue& o) const;
  bool operator<(const SqlValue& o) const;

 private:
  Type type_{Type::Null};
  int64_t int_{0};
  double real_{0.0};
  std::string text_;
  std::vector<uint8_t> blob_;
};

// ── SqlRow / SqlResult
// ────────────────────────────────────────────────────────

using SqlRow = std::vector<SqlValue>;

/// @brief Result of a SQL query: columns + rows + optional error.
/// @ingroup ui
struct SqlResult {
  std::vector<std::string> columns;
  std::vector<SqlRow> rows;
  std::string error;

  bool ok() const { return error.empty(); }
  size_t row_count() const { return rows.size(); }
  size_t col_count() const { return columns.size(); }
};

// ── SqliteDb — lightweight SQLite wrapper
// ─────────────────────────────────────

/// @brief Thin RAII wrapper around sqlite3.
///
/// Requires compile flag -DBADWOLF_SQLITE and linking against sqlite3.
/// Without that flag every method returns a graceful error result.
/// @ingroup ui
class SqliteDb {
 public:
  explicit SqliteDb(const std::string& path = ":memory:");
  ~SqliteDb();

  // Non-copyable, movable
  SqliteDb(const SqliteDb&) = delete;
  SqliteDb& operator=(const SqliteDb&) = delete;
  SqliteDb(SqliteDb&&) noexcept;
  SqliteDb& operator=(SqliteDb&&) noexcept;

  bool Open(const std::string& path = ":memory:");
  void Close();
  bool IsOpen() const;

  /// @brief Execute a statement that returns no rows (CREATE, INSERT, ...).
  SqlResult Execute(const std::string& sql);

  /// @brief Execute a SELECT and return all rows.
  SqlResult Query(const std::string& sql,
                  const std::vector<SqlValue>& params = {});

  bool TableExists(const std::string& name) const;
  std::vector<std::string> Tables() const;
  std::vector<std::string> Columns(const std::string& table) const;

  std::string last_error() const;
  int64_t last_insert_rowid() const;
  int changes() const;

 private:
#ifdef BADWOLF_SQLITE
  void* db_{nullptr};  // sqlite3*
#endif
  std::string last_error_;
};

// ── DataFrame
// ─────────────────────────────────────────────────────────────────

/// @brief Pandas-inspired immutable tabular container with reactive operations.
///
/// DataFrame is entirely independent of SQLite — it works as a pure C++
/// container and can be constructed from CSV text, JsonValue arrays, or
/// SqlResult objects.
/// @ingroup ui
class DataFrame {
 public:
  DataFrame() = default;
  explicit DataFrame(SqlResult result);
  static DataFrame FromCSV(const std::string& csv_text);
  static DataFrame FromJson(const JsonValue& array);

  // ── Dimensions ───────────────────────────────────────────────────────────
  size_t rows() const;
  size_t cols() const;
  const std::vector<std::string>& columns() const;
  bool empty() const;

  // ── Access ───────────────────────────────────────────────────────────────
  SqlValue at(size_t row, size_t col) const;
  SqlValue at(size_t row, const std::string& col) const;
  std::vector<SqlValue> column(const std::string& name) const;
  SqlRow row(size_t idx) const;

  // ── Immutable operations (return new DataFrame) ───────────────────────────
  DataFrame Filter(
      std::function<bool(const SqlRow&, const std::vector<std::string>&)> pred)
      const;
  DataFrame FilterWhere(const std::string& col, const SqlValue& val) const;
  DataFrame SortBy(const std::string& col, bool ascending = true) const;
  DataFrame Select(const std::vector<std::string>& cols) const;
  DataFrame Head(size_t n = 10) const;
  DataFrame Tail(size_t n = 10) const;
  /// Returns a DataFrame with unique values of the given column.
  DataFrame GroupBy(const std::string& col) const;

  // ── Aggregates ───────────────────────────────────────────────────────────
  double Sum(const std::string& col) const;
  double Mean(const std::string& col) const;
  double Min(const std::string& col) const;
  double Max(const std::string& col) const;
  size_t Count(const std::string& col) const;  // non-null count

  // ── Mutation (immutable) ─────────────────────────────────────────────────
  DataFrame AddColumn(const std::string& name,
                      std::function<SqlValue(const SqlRow&)> fn) const;

  // ── Export ───────────────────────────────────────────────────────────────
  SqlResult ToSqlResult() const;
  JsonValue ToJson() const;
  std::string ToCSV() const;

  // ── UI ───────────────────────────────────────────────────────────────────
  ftxui::Component AsTable(const std::string& title = "") const;
  ftxui::Element AsSummary() const;

 private:
  std::vector<std::string> columns_;
  std::vector<SqlRow> rows_;

  size_t ColIndex(const std::string& name) const;
};

// ── ReactiveDataFrame
// ─────────────────────────────────────────────────────────

/// @brief A reactive holder around DataFrame with change notifications.
/// @ingroup ui
class ReactiveDataFrame {
 public:
  explicit ReactiveDataFrame(DataFrame initial = {});

  void Set(DataFrame df);
  const DataFrame& Get() const;

  int OnChange(std::function<void(const DataFrame&)> cb);
  void RemoveOnChange(int id);

  ReactiveDataFrame Map(std::function<DataFrame(const DataFrame&)> fn) const;
  ReactiveDataFrame Filter(
      std::function<bool(const SqlRow&, const std::vector<std::string>&)> pred)
      const;

 private:
  mutable std::mutex mu_;
  DataFrame df_;
  std::map<int, std::function<void(const DataFrame&)>> listeners_;
  int next_id_{0};

  void Notify();
};

// ── ReactiveQuery<T>
// ──────────────────────────────────────────────────────────

/// @brief Live SQL query that re-runs on Refresh() or at a polling interval.
///
/// T defaults to SqlResult for raw use; supply a Mapper to project rows into
/// a domain type.
///
/// @tparam T  The result type (default SqlResult).
/// @ingroup ui
template <typename T = SqlResult>
class ReactiveQuery {
 public:
  using Mapper = std::function<T(const SqlResult&)>;

  ReactiveQuery(std::shared_ptr<SqliteDb> db,
                std::string sql,
                Mapper mapper = nullptr,
                std::chrono::milliseconds poll = std::chrono::milliseconds(0))
      : db_(std::move(db)),
        sql_(std::move(sql)),
        mapper_(std::move(mapper)),
        poll_(poll) {
    Refresh();
    if (poll_ > std::chrono::milliseconds(0)) {
      StartPolling(poll_);
    }
  }

  ~ReactiveQuery() { StopPolling(); }

  // Non-copyable
  ReactiveQuery(const ReactiveQuery&) = delete;
  ReactiveQuery& operator=(const ReactiveQuery&) = delete;

  void Refresh() {
    if (!db_) {
      return;
    }
    SqlResult res = db_->Query(sql_);
    T val = mapper_ ? mapper_(res) : CastResult(res);
    {
      std::lock_guard<std::mutex> lock(mu_);
      latest_ = std::move(val);
    }
    Notify();
  }

  void StartPolling(
      std::chrono::milliseconds interval = std::chrono::milliseconds(1000)) {
    StopPolling();
    running_ = true;
    poll_thread_ = std::thread([this, interval]() {
      while (running_) {
        std::this_thread::sleep_for(interval);
        if (!running_) {
          break;
        }
        Refresh();
      }
    });
  }

  void StopPolling() {
    running_ = false;
    if (poll_thread_.joinable()) {
      poll_thread_.join();
    }
  }

  const T& Latest() const {
    std::lock_guard<std::mutex> lock(mu_);
    return latest_;
  }

  int OnChange(std::function<void(const T&)> cb) {
    std::lock_guard<std::mutex> lock(mu_);
    int id = next_id_++;
    listeners_[id] = std::move(cb);
    return id;
  }

  void RemoveOnChange(int id) {
    std::lock_guard<std::mutex> lock(mu_);
    listeners_.erase(id);
  }

  ftxui::Component AsTable(const std::string& title = "") const;

 private:
  std::shared_ptr<SqliteDb> db_;
  std::string sql_;
  Mapper mapper_;
  std::chrono::milliseconds poll_;

  mutable std::mutex mu_;
  T latest_{};
  std::map<int, std::function<void(const T&)>> listeners_;
  int next_id_{0};

  std::atomic<bool> running_{false};
  std::thread poll_thread_;

  // Default: T must be constructible from SqlResult.
  // Specialise or provide a Mapper for other types.
  static T CastResult(const SqlResult& r) {
    if constexpr (std::is_same_v<T, SqlResult>) {
      return r;
    } else if constexpr (std::is_same_v<T, DataFrame>) {
      return DataFrame(r);
    } else {
      // Generic fallback: default-construct.
      return T{};
    }
  }

  void Notify() {
    std::map<int, std::function<void(const T&)>> snapshot;
    {
      std::lock_guard<std::mutex> lock(mu_);
      snapshot = listeners_;
    }
    for (auto& [id, cb] : snapshot) {
      cb(latest_);
    }
    if (auto* app = App::Active()) {
      app->PostEvent(Event::Custom);
    }
  }
};

// ── Free functions
// ────────────────────────────────────────────────────────────

/// @brief Render a DataFrame as a sortable FTXUI table component.
ftxui::Component DataFrameTable(const DataFrame& df,
                                const std::string& title = "");

/// @brief Render a ReactiveDataFrame as a live-updating table component.
ftxui::Component DataFrameTable(std::shared_ptr<ReactiveDataFrame> rdf,
                                const std::string& title = "");

}  // namespace ftxui::ui

#endif  // FTXUI_UI_SQL_HPP
