// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_JSON_HPP
#define FTXUI_UI_JSON_HPP

#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/reactive.hpp"

namespace ftxui::ui {

// ── JsonValue
// ─────────────────────────────────────────────────────────────────

/// @brief A JSON value that can hold null, bool, number, string, array, or
/// object.
/// @ingroup ui
class JsonValue {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  // Constructors
  JsonValue();                                      // null
  JsonValue(std::nullptr_t);                        // null
  JsonValue(bool v);                                // NOLINT
  JsonValue(int v);                                 // NOLINT
  JsonValue(double v);                              // NOLINT
  JsonValue(const std::string& v);                  // NOLINT
  JsonValue(std::string&& v);                       // NOLINT
  JsonValue(const char* v);                         // NOLINT
  JsonValue(std::initializer_list<JsonValue> arr);  // array

  static JsonValue Object();  // empty object
  static JsonValue Array();   // empty array

  // Type queries
  Type type() const;
  bool is_null() const;
  bool is_bool() const;
  bool is_number() const;
  bool is_string() const;
  bool is_array() const;
  bool is_object() const;

  // Value access (throws std::runtime_error on type mismatch)
  bool as_bool() const;
  double as_number() const;
  int as_int() const;
  const std::string& as_string() const;
  const std::vector<JsonValue>& as_array() const;
  const std::map<std::string, JsonValue>& as_object() const;

  // Safe access (returns default on mismatch)
  bool get_bool(bool def = false) const;
  double get_number(double def = 0.0) const;
  int get_int(int def = 0) const;
  std::string get_string(const std::string& def = "") const;

  // Object access
  JsonValue& operator[](const std::string& key);
  const JsonValue& operator[](const std::string& key) const;
  bool has(const std::string& key) const;
  void set(const std::string& key, JsonValue val);
  void erase(const std::string& key);
  std::vector<std::string> keys() const;

  // Array access
  JsonValue& operator[](size_t idx);
  const JsonValue& operator[](size_t idx) const;
  void push(JsonValue val);
  void pop();
  size_t size() const;
  bool empty() const;

  // Equality
  bool operator==(const JsonValue& other) const;
  bool operator!=(const JsonValue& other) const;

  static const JsonValue kNull;

 private:
  Type type_{Type::Null};
  bool bool_{false};
  double number_{0.0};
  std::string string_;
  std::vector<JsonValue> array_;
  std::map<std::string, JsonValue> object_;
};

// ── Json namespace
// ────────────────────────────────────────────────────────────

namespace Json {

/// @brief Parse JSON string → JsonValue. Throws std::runtime_error on invalid
/// JSON.
JsonValue Parse(const std::string& text);

/// @brief Parse with error reporting (returns null + sets error on failure).
JsonValue ParseSafe(const std::string& text, std::string* error = nullptr);

/// @brief Serialize JsonValue → JSON string.
std::string Stringify(const JsonValue& val, int indent = 0);

/// @brief Minified stringify (no whitespace).
std::string StringifyCompact(const JsonValue& val);

/// @brief Pretty-print with indent.
std::string StringifyPretty(const JsonValue& val, int indent = 2);

/// @brief Merge two objects (b overwrites a's keys).
JsonValue Merge(JsonValue a, const JsonValue& b);

/// @brief Deep clone.
JsonValue Clone(const JsonValue& val);

}  // namespace Json

// ── JsonPath
// ──────────────────────────────────────────────────────────────────

/// @brief Subset of JSONPath: $.key, $.arr[0], $.arr[*], $.a.b.c, $[0].name
/// @ingroup ui
class JsonPath {
 public:
  explicit JsonPath(const std::string& path);

  /// @brief Get value at path (returns null if not found).
  JsonValue Get(const JsonValue& root) const;

  /// @brief Set value at path (creates intermediate objects as needed).
  void Set(JsonValue& root, JsonValue val) const;

  /// @brief Get all matches for wildcard paths (e.g. "$.users[*].name").
  std::vector<JsonValue> GetAll(const JsonValue& root) const;

  bool valid() const;
  const std::string& path() const;

 private:
  struct Segment {
    bool is_index;
    size_t index;
    std::string key;
    bool wildcard;
  };
  std::vector<Segment> segments_;
  std::string path_;
  bool valid_ = true;

  void Parse(const std::string& path);
  JsonValue GetImpl(const JsonValue& node,
                    size_t seg_idx,
                    bool collect,
                    std::vector<JsonValue>* out) const;
};

// ── Reactive JSON
// ─────────────────────────────────────────────────────────────

/// @brief A reactive wrapper around JsonValue with path-based mutation.
/// @ingroup ui
class ReactiveJson {
 public:
  explicit ReactiveJson(JsonValue initial = JsonValue::Object());

  const JsonValue& Get() const;
  void Set(JsonValue val);
  void Set(const std::string& path, JsonValue val);

  int OnChange(std::function<void(const JsonValue&)> cb);
  void RemoveOnChange(int id);

  void Patch(const JsonValue& patch);

  std::shared_ptr<Reactive<JsonValue>> AsReactive() const;

 private:
  std::shared_ptr<Reactive<JsonValue>> reactive_;
};

// ── JSON UI Components
// ────────────────────────────────────────────────────────

/// @brief Render a JSON value as an interactive expandable tree.
ftxui::Component JsonTreeView(const JsonValue& val,
                              const std::string& root_name = "root");

/// @brief Render a JSON value as a tree, bound to a ReactiveJson (live
/// updates).
ftxui::Component JsonTreeView(std::shared_ptr<ReactiveJson> reactive);

/// @brief Render a JSON array as a DataTable (auto-detects columns from first
/// object).
ftxui::Component JsonTableView(const JsonValue& array_val);
ftxui::Component JsonTableView(std::shared_ptr<ReactiveJson> reactive);

/// @brief Auto-generate a Form from a JSON object (string→Input,
/// bool→Checkbox, number→Input).
ftxui::Component JsonForm(JsonValue schema,
                          std::function<void(JsonValue)> on_submit = {});

/// @brief Bind a TextInput to a JSON path in a ReactiveJson.
ftxui::Component JsonPathInput(std::shared_ptr<ReactiveJson> reactive,
                               const std::string& path,
                               const std::string& placeholder = "");

/// @brief Display a JSON value as a formatted, syntax-highlighted Element.
ftxui::Element JsonElement(const JsonValue& val,
                           int indent = 0,
                           int max_depth = 10);

/// @brief JSON diff — show two JsonValues side by side with differences
/// highlighted.
ftxui::Element JsonDiff(const JsonValue& a, const JsonValue& b);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_JSON_HPP
