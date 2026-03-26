// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/sql.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"

#ifdef BADWOLF_SQLITE
#include <sqlite3.h>
#endif

using namespace ftxui;

namespace ftxui::ui {

// ─────────────────────────────────────────────────────────────────────────────
// SqlValue — constructors
// ─────────────────────────────────────────────────────────────────────────────

SqlValue::SqlValue() : type_(Type::Null) {}
SqlValue::SqlValue(std::nullptr_t) : type_(Type::Null) {}
SqlValue::SqlValue(int64_t v) : type_(Type::Integer), int_(v) {}
SqlValue::SqlValue(double v) : type_(Type::Real), real_(v) {}
SqlValue::SqlValue(std::string v) : type_(Type::Text), text_(std::move(v)) {}
SqlValue::SqlValue(std::vector<uint8_t> v)
    : type_(Type::Blob), blob_(std::move(v)) {}

// ─────────────────────────────────────────────────────────────────────────────
// SqlValue — type queries
// ─────────────────────────────────────────────────────────────────────────────

SqlValue::Type SqlValue::type() const {
  return type_;
}

bool SqlValue::is_null() const {
  return type_ == Type::Null;
}

// ─────────────────────────────────────────────────────────────────────────────
// SqlValue — strict accessors
// ─────────────────────────────────────────────────────────────────────────────

int64_t SqlValue::as_int() const {
  if (type_ == Type::Integer) {
    return int_;
  }
  if (type_ == Type::Real) {
    return static_cast<int64_t>(real_);
  }
  throw std::runtime_error("SqlValue: not an integer");
}

double SqlValue::as_real() const {
  if (type_ == Type::Real) {
    return real_;
  }
  if (type_ == Type::Integer) {
    return static_cast<double>(int_);
  }
  throw std::runtime_error("SqlValue: not a real");
}

const std::string& SqlValue::as_text() const {
  if (type_ != Type::Text) {
    throw std::runtime_error("SqlValue: not text");
  }
  return text_;
}

// ─────────────────────────────────────────────────────────────────────────────
// SqlValue — safe accessors
// ─────────────────────────────────────────────────────────────────────────────

int64_t SqlValue::get_int(int64_t def) const {
  if (type_ == Type::Integer) {
    return int_;
  }
  if (type_ == Type::Real) {
    return static_cast<int64_t>(real_);
  }
  return def;
}

double SqlValue::get_real(double def) const {
  if (type_ == Type::Real) {
    return real_;
  }
  if (type_ == Type::Integer) {
    return static_cast<double>(int_);
  }
  return def;
}

std::string SqlValue::get_text(const std::string& def) const {
  if (type_ == Type::Text) {
    return text_;
  }
  return def;
}

// ─────────────────────────────────────────────────────────────────────────────
// SqlValue — to_string
// ─────────────────────────────────────────────────────────────────────────────

std::string SqlValue::to_string() const {
  switch (type_) {
    case Type::Null:
      return "NULL";
    case Type::Integer:
      return std::to_string(int_);
    case Type::Real: {
      std::ostringstream oss;
      oss << real_;
      return oss.str();
    }
    case Type::Text:
      return text_;
    case Type::Blob:
      return "<blob:" + std::to_string(blob_.size()) + ">";
  }
  return "";
}

// ─────────────────────────────────────────────────────────────────────────────
// SqlValue — comparisons
// ─────────────────────────────────────────────────────────────────────────────

bool SqlValue::operator==(const SqlValue& o) const {
  if (type_ != o.type_) {
    return false;
  }
  switch (type_) {
    case Type::Null:
      return true;
    case Type::Integer:
      return int_ == o.int_;
    case Type::Real:
      return real_ == o.real_;
    case Type::Text:
      return text_ == o.text_;
    case Type::Blob:
      return blob_ == o.blob_;
  }
  return false;
}

bool SqlValue::operator!=(const SqlValue& o) const {
  return !(*this == o);
}

bool SqlValue::operator<(const SqlValue& o) const {
  if (type_ != o.type_) {
    return static_cast<int>(type_) < static_cast<int>(o.type_);
  }
  switch (type_) {
    case Type::Null:
      return false;
    case Type::Integer:
      return int_ < o.int_;
    case Type::Real:
      return real_ < o.real_;
    case Type::Text:
      return text_ < o.text_;
    case Type::Blob:
      return blob_ < o.blob_;
  }
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// SqliteDb — constructor / destructor
// ─────────────────────────────────────────────────────────────────────────────

SqliteDb::SqliteDb(const std::string& path) {
  Open(path);
}

SqliteDb::~SqliteDb() {
  Close();
}

SqliteDb::SqliteDb(SqliteDb&& other) noexcept {
#ifdef BADWOLF_SQLITE
  db_ = other.db_;
  other.db_ = nullptr;
#endif
  last_error_ = std::move(other.last_error_);
}

SqliteDb& SqliteDb::operator=(SqliteDb&& other) noexcept {
  if (this != &other) {
    Close();
#ifdef BADWOLF_SQLITE
    db_ = other.db_;
    other.db_ = nullptr;
#endif
    last_error_ = std::move(other.last_error_);
  }
  return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// SqliteDb — Open / Close / IsOpen
// ─────────────────────────────────────────────────────────────────────────────

#ifdef BADWOLF_SQLITE

bool SqliteDb::Open(const std::string& path) {
  Close();
  sqlite3* db = nullptr;
  int rc = sqlite3_open(path.c_str(), &db);
  if (rc != SQLITE_OK) {
    last_error_ = sqlite3_errmsg(db);
    sqlite3_close(db);
    return false;
  }
  db_ = db;
  return true;
}

void SqliteDb::Close() {
  if (db_) {
    sqlite3_close(static_cast<sqlite3*>(db_));
    db_ = nullptr;
  }
}

bool SqliteDb::IsOpen() const {
  return db_ != nullptr;
}

SqlResult SqliteDb::Execute(const std::string& sql) {
  SqlResult result;
  if (!db_) {
    result.error = "Database not open";
    return result;
  }
  char* errmsg = nullptr;
  int rc = sqlite3_exec(static_cast<sqlite3*>(db_), sql.c_str(), nullptr,
                        nullptr, &errmsg);
  if (rc != SQLITE_OK) {
    result.error = errmsg ? errmsg : "Unknown error";
    sqlite3_free(errmsg);
  }
  return result;
}

SqlResult SqliteDb::Query(const std::string& sql,
                          const std::vector<SqlValue>& params) {
  SqlResult result;
  if (!db_) {
    result.error = "Database not open";
    return result;
  }
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql.c_str(), -1,
                              &stmt, nullptr);
  if (rc != SQLITE_OK) {
    result.error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    return result;
  }
  // Bind parameters
  for (int i = 0; i < static_cast<int>(params.size()); ++i) {
    const SqlValue& v = params[i];
    switch (v.type()) {
      case SqlValue::Type::Null:
        sqlite3_bind_null(stmt, i + 1);
        break;
      case SqlValue::Type::Integer:
        sqlite3_bind_int64(stmt, i + 1, v.get_int());
        break;
      case SqlValue::Type::Real:
        sqlite3_bind_double(stmt, i + 1, v.get_real());
        break;
      case SqlValue::Type::Text:
        sqlite3_bind_text(stmt, i + 1, v.get_text().c_str(), -1,
                          SQLITE_TRANSIENT);
        break;
      case SqlValue::Type::Blob:
        break;
    }
  }
  // Read column names
  int col_count = sqlite3_column_count(stmt);
  for (int c = 0; c < col_count; ++c) {
    const char* name = sqlite3_column_name(stmt, c);
    result.columns.push_back(name ? name : "");
  }
  // Step through rows
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    SqlRow row;
    for (int c = 0; c < col_count; ++c) {
      int col_type = sqlite3_column_type(stmt, c);
      switch (col_type) {
        case SQLITE_NULL:
          row.emplace_back(nullptr);
          break;
        case SQLITE_INTEGER:
          row.emplace_back(sqlite3_column_int64(stmt, c));
          break;
        case SQLITE_FLOAT:
          row.emplace_back(sqlite3_column_double(stmt, c));
          break;
        case SQLITE_TEXT: {
          const char* txt =
              reinterpret_cast<const char*>(sqlite3_column_text(stmt, c));
          row.emplace_back(std::string(txt ? txt : ""));
          break;
        }
        case SQLITE_BLOB: {
          const uint8_t* blob_ptr =
              static_cast<const uint8_t*>(sqlite3_column_blob(stmt, c));
          int bytes = sqlite3_column_bytes(stmt, c);
          std::vector<uint8_t> blob(blob_ptr, blob_ptr + bytes);
          row.emplace_back(std::move(blob));
          break;
        }
        default:
          row.emplace_back(nullptr);
          break;
      }
    }
    result.rows.push_back(std::move(row));
  }
  sqlite3_finalize(stmt);
  return result;
}

bool SqliteDb::TableExists(const std::string& name) const {
  if (!db_) {
    return false;
  }
  std::string sql =
      "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql.c_str(), -1,
                              &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  bool found = (sqlite3_step(stmt) == SQLITE_ROW);
  sqlite3_finalize(stmt);
  return found;
}

std::vector<std::string> SqliteDb::Tables() const {
  std::vector<std::string> tables;
  if (!db_) {
    return tables;
  }
  std::string sql =
      "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql.c_str(), -1,
                              &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return tables;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* name =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (name) {
      tables.emplace_back(name);
    }
  }
  sqlite3_finalize(stmt);
  return tables;
}

std::vector<std::string> SqliteDb::Columns(const std::string& table) const {
  std::vector<std::string> cols;
  if (!db_) {
    return cols;
  }
  std::string sql = "PRAGMA table_info(" + table + ");";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql.c_str(), -1,
                              &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return cols;
  }
  // PRAGMA table_info columns: cid, name, type, notnull, dflt_value, pk
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* name =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (name) {
      cols.emplace_back(name);
    }
  }
  sqlite3_finalize(stmt);
  return cols;
}

std::string SqliteDb::last_error() const {
  return last_error_;
}

int64_t SqliteDb::last_insert_rowid() const {
  if (!db_) {
    return 0;
  }
  return sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
}

int SqliteDb::changes() const {
  if (!db_) {
    return 0;
  }
  return sqlite3_changes(static_cast<sqlite3*>(db_));
}

#else  // BADWOLF_SQLITE not defined

static const std::string kNoSqlite =
    "SQLite not available \xe2\x80\x94 compile with -DBADWOLF_SQLITE";

bool SqliteDb::Open(const std::string& /*path*/) {
  last_error_ = kNoSqlite;
  return false;
}

void SqliteDb::Close() {}

bool SqliteDb::IsOpen() const {
  return false;
}

SqlResult SqliteDb::Execute(const std::string& /*sql*/) {
  SqlResult r;
  r.error = kNoSqlite;
  return r;
}

SqlResult SqliteDb::Query(const std::string& /*sql*/,
                          const std::vector<SqlValue>& /*params*/) {
  SqlResult r;
  r.error = kNoSqlite;
  return r;
}

bool SqliteDb::TableExists(const std::string& /*name*/) const {
  return false;
}

std::vector<std::string> SqliteDb::Tables() const {
  return {};
}

std::vector<std::string> SqliteDb::Columns(const std::string& /*table*/) const {
  return {};
}

std::string SqliteDb::last_error() const {
  return last_error_;
}

int64_t SqliteDb::last_insert_rowid() const {
  return 0;
}

int SqliteDb::changes() const {
  return 0;
}

#endif  // BADWOLF_SQLITE

// ─────────────────────────────────────────────────────────────────────────────
// DataFrame — private helper
// ─────────────────────────────────────────────────────────────────────────────

size_t DataFrame::ColIndex(const std::string& name) const {
  for (size_t i = 0; i < columns_.size(); ++i) {
    if (columns_[i] == name) {
      return i;
    }
  }
  throw std::runtime_error("DataFrame: column not found: " + name);
}

// ─────────────────────────────────────────────────────────────────────────────
// DataFrame — constructors
// ─────────────────────────────────────────────────────────────────────────────

DataFrame::DataFrame(SqlResult result)
    : columns_(std::move(result.columns)), rows_(std::move(result.rows)) {}

// ─────────────────────────────────────────────────────────────────────────────
// DataFrame::FromCSV
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<std::string> ParseCSVLine(const std::string& line) {
  std::vector<std::string> fields;
  std::string field;
  bool in_quotes = false;
  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (in_quotes) {
      if (c == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          field += '"';
          ++i;
        } else {
          in_quotes = false;
        }
      } else {
        field += c;
      }
    } else {
      if (c == '"') {
        in_quotes = true;
      } else if (c == ',') {
        fields.push_back(field);
        field.clear();
      } else {
        field += c;
      }
    }
  }
  fields.push_back(field);
  return fields;
}

DataFrame DataFrame::FromCSV(const std::string& csv_text) {
  DataFrame df;
  if (csv_text.empty()) {
    return df;
  }
  std::istringstream stream(csv_text);
  std::string line;
  bool first = true;
  while (std::getline(stream, line)) {
    // Strip trailing \r
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }
    auto fields = ParseCSVLine(line);
    if (first) {
      df.columns_ = fields;
      first = false;
    } else {
      SqlRow row;
      row.reserve(df.columns_.size());
      for (const auto& f : fields) {
        row.emplace_back(SqlValue(f));
      }
      // Pad if fewer columns
      while (row.size() < df.columns_.size()) {
        row.emplace_back(nullptr);
      }
      df.rows_.push_back(std::move(row));
    }
  }
  return df;
}

// ─────────────────────────────────────────────────────────────────────────────
// DataFrame::FromJson
// ─────────────────────────────────────────────────────────────────────────────

DataFrame DataFrame::FromJson(const JsonValue& array) {
  DataFrame df;
  if (!array.is_array() || array.empty()) {
    return df;
  }
  // Collect columns from keys of first object
  const auto& first_obj = array[static_cast<size_t>(0)];
  if (!first_obj.is_object()) {
    return df;
  }
  df.columns_ = first_obj.keys();

  for (size_t r = 0; r < array.size(); ++r) {
    const auto& obj = array[r];
    if (!obj.is_object()) {
      continue;
    }
    SqlRow row;
    row.reserve(df.columns_.size());
    for (const auto& col : df.columns_) {
      if (!obj.has(col)) {
        row.emplace_back(nullptr);
        continue;
      }
      const auto& jv = obj[col];
      switch (jv.type()) {
        case JsonValue::Type::Null:
          row.emplace_back(nullptr);
          break;
        case JsonValue::Type::Bool:
          row.emplace_back(
              SqlValue(static_cast<int64_t>(jv.as_bool() ? 1 : 0)));
          break;
        case JsonValue::Type::Number: {
          double d = jv.as_number();
          if (d == std::floor(d) && std::abs(d) < 9e18) {
            row.emplace_back(SqlValue(static_cast<int64_t>(d)));
          } else {
            row.emplace_back(SqlValue(d));
          }
          break;
        }
        case JsonValue::Type::String:
          row.emplace_back(SqlValue(jv.as_string()));
          break;
        default:
          row.emplace_back(SqlValue(jv.get_string("")));
          break;
      }
    }
    df.rows_.push_back(std::move(row));
  }
  return df;
}

// ─────────────────────────────────────────────────────────────────────────────
// DataFrame — dimensions and access
// ─────────────────────────────────────────────────────────────────────────────

size_t DataFrame::rows() const {
  return rows_.size();
}

size_t DataFrame::cols() const {
  return columns_.size();
}

const std::vector<std::string>& DataFrame::columns() const {
  return columns_;
}

bool DataFrame::empty() const {
  return rows_.empty();
}

SqlValue DataFrame::at(size_t row, size_t col) const {
  if (row >= rows_.size() || col >= columns_.size()) {
    return SqlValue{};
  }
  const auto& r = rows_[row];
  if (col >= r.size()) {
    return SqlValue{};
  }
  return r[col];
}

SqlValue DataFrame::at(size_t row, const std::string& col) const {
  return at(row, ColIndex(col));
}

std::vector<SqlValue> DataFrame::column(const std::string& name) const {
  size_t idx = ColIndex(name);
  std::vector<SqlValue> result;
  result.reserve(rows_.size());
  for (const auto& r : rows_) {
    if (idx < r.size()) {
      result.push_back(r[idx]);
    } else {
      result.emplace_back(nullptr);
    }
  }
  return result;
}

SqlRow DataFrame::row(size_t idx) const {
  if (idx >= rows_.size()) {
    return {};
  }
  return rows_[idx];
}

// ─────────────────────────────────────────────────────────────────────────────
// DataFrame — immutable operations
// ─────────────────────────────────────────────────────────────────────────────

DataFrame DataFrame::Filter(
    std::function<bool(const SqlRow&, const std::vector<std::string>&)> pred)
    const {
  DataFrame result;
  result.columns_ = columns_;
  for (const auto& r : rows_) {
    if (pred(r, columns_)) {
      result.rows_.push_back(r);
    }
  }
  return result;
}

DataFrame DataFrame::FilterWhere(const std::string& col,
                                 const SqlValue& val) const {
  size_t idx = ColIndex(col);
  DataFrame result;
  result.columns_ = columns_;
  for (const auto& r : rows_) {
    if (idx < r.size() && r[idx] == val) {
      result.rows_.push_back(r);
    }
  }
  return result;
}

DataFrame DataFrame::SortBy(const std::string& col, bool ascending) const {
  size_t idx = ColIndex(col);
  DataFrame result;
  result.columns_ = columns_;
  result.rows_ = rows_;
  std::stable_sort(
      result.rows_.begin(), result.rows_.end(),
      [idx, ascending](const SqlRow& a, const SqlRow& b) {
        const SqlValue& av = (idx < a.size()) ? a[idx] : SqlValue{};
        const SqlValue& bv = (idx < b.size()) ? b[idx] : SqlValue{};
        return ascending ? av < bv : bv < av;
      });
  return result;
}

DataFrame DataFrame::Select(const std::vector<std::string>& cols) const {
  std::vector<size_t> indices;
  indices.reserve(cols.size());
  for (const auto& c : cols) {
    indices.push_back(ColIndex(c));
  }
  DataFrame result;
  result.columns_ = cols;
  for (const auto& r : rows_) {
    SqlRow new_row;
    new_row.reserve(indices.size());
    for (size_t idx : indices) {
      new_row.push_back((idx < r.size()) ? r[idx] : SqlValue{});
    }
    result.rows_.push_back(std::move(new_row));
  }
  return result;
}

DataFrame DataFrame::Head(size_t n) const {
  DataFrame result;
  result.columns_ = columns_;
  size_t limit = std::min(n, rows_.size());
  result.rows_.assign(rows_.begin(),
                      rows_.begin() + static_cast<ptrdiff_t>(limit));
  return result;
}

DataFrame DataFrame::Tail(size_t n) const {
  DataFrame result;
  result.columns_ = columns_;
  size_t limit = std::min(n, rows_.size());
  result.rows_.assign(rows_.end() - static_cast<ptrdiff_t>(limit), rows_.end());
  return result;
}

DataFrame DataFrame::GroupBy(const std::string& col) const {
  size_t idx = ColIndex(col);
  DataFrame result;
  result.columns_ = columns_;
  std::map<std::string, bool> seen;
  for (const auto& r : rows_) {
    const SqlValue& v = (idx < r.size()) ? r[idx] : SqlValue{};
    std::string key = v.to_string();
    if (!seen[key]) {
      seen[key] = true;
      result.rows_.push_back(r);
    }
  }
  return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// DataFrame — aggregates
// ─────────────────────────────────────────────────────────────────────────────

double DataFrame::Sum(const std::string& col) const {
  size_t idx = ColIndex(col);
  double total = 0.0;
  for (const auto& r : rows_) {
    if (idx < r.size()) {
      total += r[idx].get_real(0.0);
    }
  }
  return total;
}

double DataFrame::Mean(const std::string& col) const {
  size_t idx = ColIndex(col);
  double total = 0.0;
  size_t count = 0;
  for (const auto& r : rows_) {
    if (idx < r.size() && !r[idx].is_null()) {
      total += r[idx].get_real(0.0);
      ++count;
    }
  }
  return count > 0 ? total / static_cast<double>(count) : 0.0;
}

double DataFrame::Min(const std::string& col) const {
  size_t idx = ColIndex(col);
  bool found = false;
  double min_val = 0.0;
  for (const auto& r : rows_) {
    if (idx < r.size() && !r[idx].is_null()) {
      double v = r[idx].get_real(0.0);
      if (!found || v < min_val) {
        min_val = v;
        found = true;
      }
    }
  }
  return min_val;
}

double DataFrame::Max(const std::string& col) const {
  size_t idx = ColIndex(col);
  bool found = false;
  double max_val = 0.0;
  for (const auto& r : rows_) {
    if (idx < r.size() && !r[idx].is_null()) {
      double v = r[idx].get_real(0.0);
      if (!found || v > max_val) {
        max_val = v;
        found = true;
      }
    }
  }
  return max_val;
}

size_t DataFrame::Count(const std::string& col) const {
  size_t idx = ColIndex(col);
  size_t count = 0;
  for (const auto& r : rows_) {
    if (idx < r.size() && !r[idx].is_null()) {
      ++count;
    }
  }
  return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// DataFrame — mutation (immutable)
// ─────────────────────────────────────────────────────────────────────────────

DataFrame DataFrame::AddColumn(
    const std::string& name,
    std::function<SqlValue(const SqlRow&)> fn) const {
  DataFrame result;
  result.columns_ = columns_;
  result.columns_.push_back(name);
  for (const auto& r : rows_) {
    SqlRow new_row = r;
    new_row.push_back(fn(r));
    result.rows_.push_back(std::move(new_row));
  }
  return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// DataFrame — export
// ─────────────────────────────────────────────────────────────────────────────

SqlResult DataFrame::ToSqlResult() const {
  SqlResult result;
  result.columns = columns_;
  result.rows = rows_;
  return result;
}

JsonValue DataFrame::ToJson() const {
  auto arr = JsonValue::Array();
  for (const auto& r : rows_) {
    auto obj = JsonValue::Object();
    for (size_t c = 0; c < columns_.size(); ++c) {
      const SqlValue& v = (c < r.size()) ? r[c] : SqlValue{};
      switch (v.type()) {
        case SqlValue::Type::Null:
          obj.set(columns_[c], JsonValue{nullptr});
          break;
        case SqlValue::Type::Integer:
          obj.set(columns_[c], JsonValue(static_cast<int>(v.get_int())));
          break;
        case SqlValue::Type::Real:
          obj.set(columns_[c], JsonValue(v.get_real()));
          break;
        case SqlValue::Type::Text:
          obj.set(columns_[c], JsonValue(v.get_text()));
          break;
        case SqlValue::Type::Blob:
          obj.set(columns_[c], JsonValue(v.to_string()));
          break;
      }
    }
    arr.push(std::move(obj));
  }
  return arr;
}

std::string DataFrame::ToCSV() const {
  std::ostringstream oss;
  // Header
  for (size_t c = 0; c < columns_.size(); ++c) {
    if (c > 0) {
      oss << ',';
    }
    const std::string& col = columns_[c];
    bool needs_quote = col.find(',') != std::string::npos ||
                       col.find('"') != std::string::npos ||
                       col.find('\n') != std::string::npos;
    if (needs_quote) {
      oss << '"';
      for (char ch : col) {
        if (ch == '"') {
          oss << "\"\"";
        } else {
          oss << ch;
        }
      }
      oss << '"';
    } else {
      oss << col;
    }
  }
  oss << '\n';
  // Rows
  for (const auto& r : rows_) {
    for (size_t c = 0; c < columns_.size(); ++c) {
      if (c > 0) {
        oss << ',';
      }
      std::string val = (c < r.size()) ? r[c].to_string() : "";
      bool needs_quote = val.find(',') != std::string::npos ||
                         val.find('"') != std::string::npos ||
                         val.find('\n') != std::string::npos;
      if (needs_quote) {
        oss << '"';
        for (char ch : val) {
          if (ch == '"') {
            oss << "\"\"";
          } else {
            oss << ch;
          }
        }
        oss << '"';
      } else {
        oss << val;
      }
    }
    oss << '\n';
  }
  return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// DataFrame — UI
// ─────────────────────────────────────────────────────────────────────────────

ftxui::Component DataFrame::AsTable(const std::string& title) const {
  auto cols = columns_;
  auto rows = rows_;
  auto t = title;
  return ftxui::Renderer([cols, rows, t]() -> ftxui::Element {
    std::vector<std::vector<ftxui::Element>> cells;
    // Header row
    std::vector<ftxui::Element> header;
    header.reserve(cols.size());
    for (const auto& c : cols) {
      header.push_back(ftxui::text(c) | ftxui::bold);
    }
    cells.push_back(std::move(header));
    // Data rows
    for (const auto& r : rows) {
      std::vector<ftxui::Element> row_elems;
      row_elems.reserve(cols.size());
      for (size_t i = 0; i < cols.size(); ++i) {
        std::string val = (i < r.size()) ? r[i].to_string() : "";
        row_elems.push_back(ftxui::text(val));
      }
      cells.push_back(std::move(row_elems));
    }
    auto tbl = ftxui::Table(std::move(cells));
    tbl.SelectAll().Border(ftxui::LIGHT);
    tbl.SelectRow(0).Decorate(ftxui::bold);
    tbl.SelectRow(0).SeparatorVertical(ftxui::LIGHT);
    tbl.SelectRow(0).Border(ftxui::LIGHT);
    ftxui::Element result = tbl.Render();
    if (!t.empty()) {
      result =
          ftxui::vbox({ftxui::text(t) | ftxui::bold | ftxui::center, result});
    }
    return result;
  });
}

ftxui::Element DataFrame::AsSummary() const {
  ftxui::Elements summary;
  summary.push_back(
      ftxui::hbox({ftxui::text("Rows: "),
                   ftxui::text(std::to_string(rows_.size())) | ftxui::bold}));
  summary.push_back(ftxui::hbox(
      {ftxui::text("Cols: "),
       ftxui::text(std::to_string(columns_.size())) | ftxui::bold}));
  summary.push_back(ftxui::separator());

  size_t preview = std::min(size_t(3), rows_.size());
  for (size_t r = 0; r < preview; ++r) {
    std::string line;
    for (size_t c = 0; c < columns_.size(); ++c) {
      if (c > 0) {
        line += " | ";
      }
      line += (c < rows_[r].size()) ? rows_[r][c].to_string() : "";
    }
    summary.push_back(ftxui::text(line));
  }
  return ftxui::vbox(std::move(summary));
}

// ─────────────────────────────────────────────────────────────────────────────
// ReactiveDataFrame
// ─────────────────────────────────────────────────────────────────────────────

ReactiveDataFrame::ReactiveDataFrame(DataFrame initial)
    : df_(std::move(initial)) {}

void ReactiveDataFrame::Set(DataFrame df) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    df_ = std::move(df);
  }
  Notify();
}

const DataFrame& ReactiveDataFrame::Get() const {
  std::lock_guard<std::mutex> lock(mu_);
  return df_;
}

int ReactiveDataFrame::OnChange(std::function<void(const DataFrame&)> cb) {
  std::lock_guard<std::mutex> lock(mu_);
  int id = next_id_++;
  listeners_[id] = std::move(cb);
  return id;
}

void ReactiveDataFrame::RemoveOnChange(int id) {
  std::lock_guard<std::mutex> lock(mu_);
  listeners_.erase(id);
}

ReactiveDataFrame ReactiveDataFrame::Map(
    std::function<DataFrame(const DataFrame&)> fn) const {
  std::lock_guard<std::mutex> lock(mu_);
  return ReactiveDataFrame(fn(df_));
}

ReactiveDataFrame ReactiveDataFrame::Filter(
    std::function<bool(const SqlRow&, const std::vector<std::string>&)> pred)
    const {
  std::lock_guard<std::mutex> lock(mu_);
  return ReactiveDataFrame(df_.Filter(pred));
}

void ReactiveDataFrame::Notify() {
  std::map<int, std::function<void(const DataFrame&)>> snapshot;
  DataFrame df_copy;
  {
    std::lock_guard<std::mutex> lock(mu_);
    snapshot = listeners_;
    df_copy = df_;
  }
  for (auto& [id, cb] : snapshot) {
    cb(df_copy);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Free functions
// ─────────────────────────────────────────────────────────────────────────────

ftxui::Component DataFrameTable(const DataFrame& df, const std::string& title) {
  return df.AsTable(title);
}

ftxui::Component DataFrameTable(std::shared_ptr<ReactiveDataFrame> rdf,
                                const std::string& title) {
  return ftxui::Renderer([rdf, title]() -> ftxui::Element {
    return rdf->Get().AsTable(title)->Render();
  });
}

}  // namespace ftxui::ui
