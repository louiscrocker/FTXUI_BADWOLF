// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ─────────────────────────────────────────────────────────────────────────────
// JsonValue — static sentinel
// ─────────────────────────────────────────────────────────────────────────────

const JsonValue JsonValue::kNull{};

// ─────────────────────────────────────────────────────────────────────────────
// JsonValue — constructors
// ─────────────────────────────────────────────────────────────────────────────

JsonValue::JsonValue() : type_(Type::Null) {}
JsonValue::JsonValue(std::nullptr_t) : type_(Type::Null) {}
JsonValue::JsonValue(bool v) : type_(Type::Bool), bool_(v) {}
JsonValue::JsonValue(int v)
    : type_(Type::Number), number_(static_cast<double>(v)) {}
JsonValue::JsonValue(double v) : type_(Type::Number), number_(v) {}
JsonValue::JsonValue(const std::string& v) : type_(Type::String), string_(v) {}
JsonValue::JsonValue(std::string&& v)
    : type_(Type::String), string_(std::move(v)) {}
JsonValue::JsonValue(const char* v)
    : type_(Type::String), string_(v ? v : "") {}
JsonValue::JsonValue(std::initializer_list<JsonValue> arr)
    : type_(Type::Array), array_(arr) {}

JsonValue JsonValue::Object() {
  JsonValue v;
  v.type_ = Type::Object;
  return v;
}

JsonValue JsonValue::Array() {
  JsonValue v;
  v.type_ = Type::Array;
  return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonValue — type queries
// ─────────────────────────────────────────────────────────────────────────────

JsonValue::Type JsonValue::type() const {
  return type_;
}
bool JsonValue::is_null() const {
  return type_ == Type::Null;
}
bool JsonValue::is_bool() const {
  return type_ == Type::Bool;
}
bool JsonValue::is_number() const {
  return type_ == Type::Number;
}
bool JsonValue::is_string() const {
  return type_ == Type::String;
}
bool JsonValue::is_array() const {
  return type_ == Type::Array;
}
bool JsonValue::is_object() const {
  return type_ == Type::Object;
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonValue — value access
// ─────────────────────────────────────────────────────────────────────────────

bool JsonValue::as_bool() const {
  if (type_ != Type::Bool) {
    throw std::runtime_error("JsonValue: not a bool");
  }
  return bool_;
}

double JsonValue::as_number() const {
  if (type_ != Type::Number) {
    throw std::runtime_error("JsonValue: not a number");
  }
  return number_;
}

int JsonValue::as_int() const {
  if (type_ != Type::Number) {
    throw std::runtime_error("JsonValue: not a number");
  }
  return static_cast<int>(number_);
}

const std::string& JsonValue::as_string() const {
  if (type_ != Type::String) {
    throw std::runtime_error("JsonValue: not a string");
  }
  return string_;
}

const std::vector<JsonValue>& JsonValue::as_array() const {
  if (type_ != Type::Array) {
    throw std::runtime_error("JsonValue: not an array");
  }
  return array_;
}

const std::map<std::string, JsonValue>& JsonValue::as_object() const {
  if (type_ != Type::Object) {
    throw std::runtime_error("JsonValue: not an object");
  }
  return object_;
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonValue — safe access
// ─────────────────────────────────────────────────────────────────────────────

bool JsonValue::get_bool(bool def) const {
  return type_ == Type::Bool ? bool_ : def;
}
double JsonValue::get_number(double def) const {
  return type_ == Type::Number ? number_ : def;
}
int JsonValue::get_int(int def) const {
  return type_ == Type::Number ? static_cast<int>(number_) : def;
}
std::string JsonValue::get_string(const std::string& def) const {
  return type_ == Type::String ? string_ : def;
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonValue — object access
// ─────────────────────────────────────────────────────────────────────────────

JsonValue& JsonValue::operator[](const std::string& key) {
  if (type_ == Type::Null) {
    type_ = Type::Object;
  }
  if (type_ != Type::Object) {
    throw std::runtime_error("JsonValue: not an object");
  }
  return object_[key];
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
  if (type_ != Type::Object) {
    return kNull;
  }
  auto it = object_.find(key);
  return it == object_.end() ? kNull : it->second;
}

bool JsonValue::has(const std::string& key) const {
  return type_ == Type::Object && object_.find(key) != object_.end();
}

void JsonValue::set(const std::string& key, JsonValue val) {
  if (type_ == Type::Null) {
    type_ = Type::Object;
  }
  if (type_ != Type::Object) {
    throw std::runtime_error("JsonValue: not an object");
  }
  object_[key] = std::move(val);
}

void JsonValue::erase(const std::string& key) {
  if (type_ == Type::Object) {
    object_.erase(key);
  }
}

std::vector<std::string> JsonValue::keys() const {
  std::vector<std::string> result;
  if (type_ != Type::Object) {
    return result;
  }
  for (const auto& kv : object_) {
    result.push_back(kv.first);
  }
  return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonValue — array access
// ─────────────────────────────────────────────────────────────────────────────

JsonValue& JsonValue::operator[](size_t idx) {
  if (type_ != Type::Array) {
    throw std::runtime_error("JsonValue: not an array");
  }
  if (idx >= array_.size()) {
    throw std::out_of_range("JsonValue: index out of range");
  }
  return array_[idx];
}

const JsonValue& JsonValue::operator[](size_t idx) const {
  if (type_ != Type::Array || idx >= array_.size()) {
    return kNull;
  }
  return array_[idx];
}

void JsonValue::push(JsonValue val) {
  if (type_ == Type::Null) {
    type_ = Type::Array;
  }
  if (type_ != Type::Array) {
    throw std::runtime_error("JsonValue: not an array");
  }
  array_.push_back(std::move(val));
}

void JsonValue::pop() {
  if (type_ == Type::Array && !array_.empty()) {
    array_.pop_back();
  }
}

size_t JsonValue::size() const {
  if (type_ == Type::Array) {
    return array_.size();
  }
  if (type_ == Type::Object) {
    return object_.size();
  }
  if (type_ == Type::String) {
    return string_.size();
  }
  return 0;
}

bool JsonValue::empty() const {
  return size() == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonValue — equality
// ─────────────────────────────────────────────────────────────────────────────

bool JsonValue::operator==(const JsonValue& o) const {
  if (type_ != o.type_) {
    return false;
  }
  switch (type_) {
    case Type::Null:
      return true;
    case Type::Bool:
      return bool_ == o.bool_;
    case Type::Number:
      return number_ == o.number_;
    case Type::String:
      return string_ == o.string_;
    case Type::Array:
      return array_ == o.array_;
    case Type::Object:
      return object_ == o.object_;
  }
  return false;
}

bool JsonValue::operator!=(const JsonValue& o) const {
  return !(*this == o);
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// ── Number serialization
// ──────────────────────────────────────────────────────

std::string num_to_str(double v) {
  if (std::isinf(v) || std::isnan(v)) {
    return "null";
  }
  if (v == std::floor(v) && std::abs(v) < 1e15) {
    std::ostringstream oss;
    oss << static_cast<long long>(v);
    return oss.str();
  }
  std::ostringstream oss;
  oss << std::setprecision(17) << v;
  std::string s = oss.str();
  if (s.find('.') != std::string::npos && s.find('e') == std::string::npos) {
    size_t last = s.find_last_not_of('0');
    if (last != std::string::npos && s[last] == '.') {
      ++last;
    }
    if (last != std::string::npos) {
      s = s.substr(0, last + 1);
    }
  }
  return s;
}

// ── String escaping
// ───────────────────────────────────────────────────────────

std::string escape_str(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 2);
  out += '"';
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      default:
        if (c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  out += '"';
  return out;
}

// ── Stringify
// ─────────────────────────────────────────────────────────────────

void stringify_impl(const JsonValue& val, std::string& out, int ind, int d) {
  switch (val.type()) {
    case JsonValue::Type::Null:
      out += "null";
      return;
    case JsonValue::Type::Bool:
      out += val.as_bool() ? "true" : "false";
      return;
    case JsonValue::Type::Number:
      out += num_to_str(val.as_number());
      return;
    case JsonValue::Type::String:
      out += escape_str(val.as_string());
      return;
    case JsonValue::Type::Array: {
      const auto& arr = val.as_array();
      out += '[';
      if (ind > 0 && !arr.empty()) {
        std::string pad(static_cast<size_t>((d + 1) * ind), ' ');
        std::string cpad(static_cast<size_t>(d * ind), ' ');
        out += '\n';
        for (size_t i = 0; i < arr.size(); ++i) {
          out += pad;
          stringify_impl(arr[i], out, ind, d + 1);
          if (i + 1 < arr.size()) {
            out += ',';
          }
          out += '\n';
        }
        out += cpad;
      } else {
        for (size_t i = 0; i < arr.size(); ++i) {
          stringify_impl(arr[i], out, ind, d + 1);
          if (i + 1 < arr.size()) {
            out += ',';
          }
        }
      }
      out += ']';
      return;
    }
    case JsonValue::Type::Object: {
      const auto& obj = val.as_object();
      out += '{';
      if (ind > 0 && !obj.empty()) {
        std::string pad(static_cast<size_t>((d + 1) * ind), ' ');
        std::string cpad(static_cast<size_t>(d * ind), ' ');
        out += '\n';
        size_t i = 0;
        for (const auto& kv : obj) {
          out += pad;
          out += escape_str(kv.first);
          out += ": ";
          stringify_impl(kv.second, out, ind, d + 1);
          if (i + 1 < obj.size()) {
            out += ',';
          }
          out += '\n';
          ++i;
        }
        out += cpad;
      } else {
        size_t i = 0;
        for (const auto& kv : obj) {
          out += escape_str(kv.first);
          out += ':';
          stringify_impl(kv.second, out, ind, d + 1);
          if (i + 1 < obj.size()) {
            out += ',';
          }
          ++i;
        }
      }
      out += '}';
      return;
    }
  }
}

// ── Recursive descent parser
// ──────────────────────────────────────────────────

struct Parser {
  const std::string& src;
  size_t pos = 0;

  void skip_ws() {
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' ||
                                src[pos] == '\n' || src[pos] == '\r')) {
      ++pos;
    }
  }

  char peek() const { return pos < src.size() ? src[pos] : '\0'; }

  char consume() {
    if (pos >= src.size()) {
      throw std::runtime_error("JSON parse error: unexpected end of input");
    }
    return src[pos++];
  }

  void expect(char c) {
    char got = consume();
    if (got != c) {
      throw std::runtime_error(std::string("JSON parse error at pos ") +
                               std::to_string(pos - 1) + ": expected '" + c +
                               "' got '" + got + "'");
    }
  }

  JsonValue parse_value() {
    skip_ws();
    char c = peek();
    if (c == '"') {
      return parse_string_val();
    }
    if (c == '{') {
      return parse_object();
    }
    if (c == '[') {
      return parse_array();
    }
    if (c == 't') {
      return parse_true();
    }
    if (c == 'f') {
      return parse_false();
    }
    if (c == 'n') {
      return parse_null();
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      return parse_number();
    }
    throw std::runtime_error(std::string("JSON parse error at pos ") +
                             std::to_string(pos) + ": unexpected '" + c + "'");
  }

  JsonValue parse_null() {
    expect('n');
    expect('u');
    expect('l');
    expect('l');
    return JsonValue();
  }
  JsonValue parse_true() {
    expect('t');
    expect('r');
    expect('u');
    expect('e');
    return JsonValue(true);
  }
  JsonValue parse_false() {
    expect('f');
    expect('a');
    expect('l');
    expect('s');
    expect('e');
    return JsonValue(false);
  }

  JsonValue parse_number() {
    size_t start = pos;
    if (peek() == '-') {
      ++pos;
    }
    while (pos < src.size() &&
           std::isdigit(static_cast<unsigned char>(src[pos]))) {
      ++pos;
    }
    if (pos < src.size() && src[pos] == '.') {
      ++pos;
      while (pos < src.size() &&
             std::isdigit(static_cast<unsigned char>(src[pos]))) {
        ++pos;
      }
    }
    if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
      ++pos;
      if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) {
        ++pos;
      }
      while (pos < src.size() &&
             std::isdigit(static_cast<unsigned char>(src[pos]))) {
        ++pos;
      }
    }
    try {
      return JsonValue(std::stod(src.substr(start, pos - start)));
    } catch (...) {
      throw std::runtime_error("JSON parse error: invalid number '" +
                               src.substr(start, pos - start) + "'");
    }
  }

  uint32_t parse_hex4() {
    uint32_t val = 0;
    for (int i = 0; i < 4; ++i) {
      char c = consume();
      val <<= 4;
      if (c >= '0' && c <= '9') {
        val |= static_cast<uint32_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        val |= static_cast<uint32_t>(c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        val |= static_cast<uint32_t>(c - 'A' + 10);
      } else {
        throw std::runtime_error("JSON parse error: invalid hex digit");
      }
    }
    return val;
  }

  void encode_utf8(uint32_t cp, std::string& out) {
    if (cp < 0x80) {
      out += static_cast<char>(cp);
    } else if (cp < 0x800) {
      out += static_cast<char>(0xC0 | (cp >> 6));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out += static_cast<char>(0xE0 | (cp >> 12));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      out += static_cast<char>(0xF0 | (cp >> 18));
      out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }

  std::string parse_raw_string() {
    expect('"');
    std::string result;
    while (true) {
      if (pos >= src.size()) {
        throw std::runtime_error("JSON parse error: unterminated string");
      }
      char c = src[pos++];
      if (c == '"') {
        break;
      }
      if (c != '\\') {
        result += c;
        continue;
      }
      char esc = consume();
      switch (esc) {
        case '"':
          result += '"';
          break;
        case '\\':
          result += '\\';
          break;
        case '/':
          result += '/';
          break;
        case 'n':
          result += '\n';
          break;
        case 'r':
          result += '\r';
          break;
        case 't':
          result += '\t';
          break;
        case 'b':
          result += '\b';
          break;
        case 'f':
          result += '\f';
          break;
        case 'u': {
          uint32_t cp = parse_hex4();
          if (cp >= 0xD800 && cp <= 0xDBFF && pos + 1 < src.size() &&
              src[pos] == '\\' && src[pos + 1] == 'u') {
            pos += 2;
            uint32_t low = parse_hex4();
            if (low >= 0xDC00 && low <= 0xDFFF) {
              cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            }
          }
          encode_utf8(cp, result);
          break;
        }
        default:
          result += esc;
          break;
      }
    }
    return result;
  }

  JsonValue parse_string_val() { return JsonValue(parse_raw_string()); }

  JsonValue parse_array() {
    expect('[');
    skip_ws();
    JsonValue arr = JsonValue::Array();
    if (peek() == ']') {
      ++pos;
      return arr;
    }
    while (true) {
      arr.push(parse_value());
      skip_ws();
      char c = peek();
      if (c == ']') {
        ++pos;
        break;
      }
      if (c == ',') {
        ++pos;
        continue;
      }
      throw std::runtime_error(std::string("JSON parse error at pos ") +
                               std::to_string(pos) + ": expected ',' or ']'");
    }
    return arr;
  }

  JsonValue parse_object() {
    expect('{');
    skip_ws();
    JsonValue obj = JsonValue::Object();
    if (peek() == '}') {
      ++pos;
      return obj;
    }
    while (true) {
      skip_ws();
      if (peek() != '"') {
        throw std::runtime_error(std::string("JSON parse error at pos ") +
                                 std::to_string(pos) + ": expected string key");
      }
      std::string key = parse_raw_string();
      skip_ws();
      expect(':');
      obj.set(key, parse_value());
      skip_ws();
      char c = peek();
      if (c == '}') {
        ++pos;
        break;
      }
      if (c == ',') {
        ++pos;
        continue;
      }
      throw std::runtime_error(std::string("JSON parse error at pos ") +
                               std::to_string(pos) + ": expected ',' or '}'");
    }
    return obj;
  }
};

// ── Rendering helpers
// ─────────────────────────────────────────────────────────

Element render_json(const JsonValue& val, int ind, int max_d, int d);

Element render_json_inline(const JsonValue& val) {
  switch (val.type()) {
    case JsonValue::Type::Null:
      return text("null") | color(Color::GrayDark);
    case JsonValue::Type::Bool:
      return text(val.as_bool() ? "true" : "false") | color(Color::Yellow);
    case JsonValue::Type::Number:
      return text(num_to_str(val.as_number())) | color(Color::Cyan);
    case JsonValue::Type::String:
      return text("\"" + val.as_string() + "\"") | color(Color::Green);
    case JsonValue::Type::Array:
      return text("[...]") | color(Color::White);
    case JsonValue::Type::Object:
      return text("{...}") | color(Color::White);
  }
  return text("null");
}

Element render_json(const JsonValue& val, int ind, int max_d, int d) {
  if (d >= max_d) {
    return render_json_inline(val);
  }
  std::string pad(static_cast<size_t>(d * ind), ' ');
  std::string ipad(static_cast<size_t>((d + 1) * ind), ' ');
  switch (val.type()) {
    case JsonValue::Type::Null:
      return text("null") | color(Color::GrayDark);
    case JsonValue::Type::Bool:
      return text(val.as_bool() ? "true" : "false") | color(Color::Yellow);
    case JsonValue::Type::Number:
      return text(num_to_str(val.as_number())) | color(Color::Cyan);
    case JsonValue::Type::String:
      return text("\"" + val.as_string() + "\"") | color(Color::Green);
    case JsonValue::Type::Array: {
      const auto& arr = val.as_array();
      if (arr.empty()) {
        return text("[]") | color(Color::White);
      }
      Elements lines;
      lines.push_back(text("[") | color(Color::White));
      for (size_t i = 0; i < arr.size(); ++i) {
        std::string comma = (i + 1 < arr.size()) ? "," : "";
        lines.push_back(hbox(
            {text(ipad), render_json(arr[i], ind, max_d, d + 1), text(comma)}));
      }
      lines.push_back(text(pad + "]") | color(Color::White));
      return vbox(std::move(lines));
    }
    case JsonValue::Type::Object: {
      const auto& obj = val.as_object();
      if (obj.empty()) {
        return text("{}") | color(Color::White);
      }
      Elements lines;
      lines.push_back(text("{") | color(Color::White));
      size_t i = 0;
      for (const auto& kv : obj) {
        std::string comma = (i + 1 < obj.size()) ? "," : "";
        lines.push_back(hbox({
            text(ipad),
            text("\"" + kv.first + "\"") | color(Color::Blue),
            text(": ") | color(Color::White),
            render_json(kv.second, ind, max_d, d + 1),
            text(comma),
        }));
        ++i;
      }
      lines.push_back(text(pad + "}") | color(Color::White));
      return vbox(std::move(lines));
    }
  }
  return text("null");
}

// ── JSON Tree view internals
// ──────────────────────────────────────────────────

Component build_json_tree(const std::string& lbl, const JsonValue& val, int d);

Component build_json_tree(const std::string& lbl, const JsonValue& val, int d) {
  bool is_leaf = !val.is_object() && !val.is_array();
  std::string istr(static_cast<size_t>(d * 2), ' ');

  if (is_leaf) {
    std::string display = istr + lbl + ": ";
    Color vc = Color::White;
    switch (val.type()) {
      case JsonValue::Type::Null:
        display += "null";
        vc = Color::GrayDark;
        break;
      case JsonValue::Type::Bool:
        display += val.as_bool() ? "true" : "false";
        vc = Color::Yellow;
        break;
      case JsonValue::Type::Number:
        display += num_to_str(val.as_number());
        vc = Color::Cyan;
        break;
      case JsonValue::Type::String:
        display += "\"" + val.as_string() + "\"";
        vc = Color::Green;
        break;
      default:
        break;
    }
    return Renderer(
        [display, vc]() -> Element { return text(display) | color(vc); });
  }

  std::string type_hint = val.is_object() ? "{}" : "[]";
  std::string branch_lbl = istr + lbl + ": " + type_hint;

  Components children;
  if (val.is_object()) {
    for (const auto& kv : val.as_object()) {
      children.push_back(build_json_tree(kv.first, kv.second, d + 1));
    }
  } else {
    const auto& arr = val.as_array();
    for (size_t i = 0; i < arr.size(); ++i) {
      children.push_back(
          build_json_tree("[" + std::to_string(i) + "]", arr[i], d + 1));
    }
  }

  if (children.empty()) {
    return Renderer([branch_lbl]() -> Element {
      return text(branch_lbl + " (empty)") | color(Color::GrayDark);
    });
  }

  auto child_cont = Container::Vertical(std::move(children));
  return Collapsible(branch_lbl, child_cont);
}

// ── JSON Table view internals
// ─────────────────────────────────────────────────

Element build_json_table_elem(const JsonValue& arr_val) {
  if (!arr_val.is_array()) {
    return text("(not an array)") | color(Color::Red);
  }
  const auto& arr = arr_val.as_array();
  if (arr.empty()) {
    return text("(empty)") | color(Color::GrayDark);
  }

  std::vector<std::string> cols;
  if (arr[0].is_object()) {
    cols = arr[0].keys();
  }

  Elements header_cells;
  for (const auto& col : cols) {
    header_cells.push_back(text(" " + col + " ") | bold | color(Color::Blue) |
                           flex);
  }

  Elements rows;
  rows.push_back(hbox(std::move(header_cells)));
  rows.push_back(separator());

  for (const auto& row_val : arr) {
    Elements cells;
    for (const auto& col : cols) {
      std::string ct = "-";
      if (row_val.is_object() && row_val.has(col)) {
        const auto& v = row_val[col];
        switch (v.type()) {
          case JsonValue::Type::Null:
            ct = "null";
            break;
          case JsonValue::Type::Bool:
            ct = v.as_bool() ? "true" : "false";
            break;
          case JsonValue::Type::Number:
            ct = num_to_str(v.as_number());
            break;
          case JsonValue::Type::String:
            ct = v.as_string();
            break;
          default:
            ct = "...";
            break;
        }
      }
      cells.push_back(text(" " + ct + " ") | flex);
    }
    rows.push_back(hbox(std::move(cells)));
  }
  return vbox(std::move(rows)) | border;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Json::Parse / ParseSafe
// ─────────────────────────────────────────────────────────────────────────────

namespace Json {

JsonValue Parse(const std::string& text) {
  Parser p{text, 0};
  JsonValue result = p.parse_value();
  p.skip_ws();
  if (p.pos != text.size()) {
    throw std::runtime_error(std::string("JSON parse error at pos ") +
                             std::to_string(p.pos) +
                             ": unexpected trailing content");
  }
  return result;
}

JsonValue ParseSafe(const std::string& text, std::string* error) {
  try {
    return Parse(text);
  } catch (const std::exception& e) {
    if (error) {
      *error = e.what();
    }
    return {};
  }
}

std::string Stringify(const JsonValue& val, int indent) {
  std::string out;
  stringify_impl(val, out, indent, 0);
  return out;
}

std::string StringifyCompact(const JsonValue& val) {
  return Stringify(val, 0);
}

std::string StringifyPretty(const JsonValue& val, int indent) {
  return Stringify(val, indent);
}

JsonValue Merge(JsonValue a, const JsonValue& b) {
  if (!a.is_object() || !b.is_object()) {
    return b;
  }
  for (const auto& key : b.keys()) {
    a.set(key, b[key]);
  }
  return a;
}

JsonValue Clone(const JsonValue& val) {
  return val;  // value semantics = deep copy
}

}  // namespace Json

// ─────────────────────────────────────────────────────────────────────────────
// JsonPath
// ─────────────────────────────────────────────────────────────────────────────

JsonPath::JsonPath(const std::string& path) : path_(path) {
  Parse(path);
}

void JsonPath::Parse(const std::string& path) {
  segments_.clear();
  valid_ = true;
  size_t i = 0;
  if (path.empty() || path[0] != '$') {
    valid_ = false;
    return;
  }
  i = 1;
  while (i < path.size()) {
    if (path[i] == '.') {
      ++i;
      size_t start = i;
      while (i < path.size() && path[i] != '.' && path[i] != '[') {
        ++i;
      }
      std::string key = path.substr(start, i - start);
      if (key.empty()) {
        valid_ = false;
        return;
      }
      segments_.push_back({false, 0, key, false});
    } else if (path[i] == '[') {
      ++i;
      if (i < path.size() && path[i] == '*') {
        ++i;
        if (i >= path.size() || path[i] != ']') {
          valid_ = false;
          return;
        }
        ++i;
        segments_.push_back({true, 0, "", true});
      } else {
        size_t start = i;
        while (i < path.size() && path[i] != ']') {
          ++i;
        }
        if (i >= path.size()) {
          valid_ = false;
          return;
        }
        try {
          size_t idx =
              static_cast<size_t>(std::stoul(path.substr(start, i - start)));
          ++i;
          segments_.push_back({true, idx, "", false});
        } catch (...) {
          valid_ = false;
          return;
        }
      }
    } else {
      valid_ = false;
      return;
    }
  }
}

JsonValue JsonPath::GetImpl(const JsonValue& node,
                            size_t si,
                            bool collect,
                            std::vector<JsonValue>* out) const {
  if (si == segments_.size()) {
    if (collect && out) {
      out->push_back(node);
    }
    return node;
  }
  const Segment& seg = segments_[si];
  if (seg.wildcard) {
    if (!node.is_array()) {
      return {};
    }
    for (const auto& item : node.as_array()) {
      GetImpl(item, si + 1, true, out);
    }
    return {};
  }
  if (seg.is_index) {
    if (!node.is_array() || seg.index >= node.as_array().size()) {
      return {};
    }
    return GetImpl(node.as_array()[seg.index], si + 1, collect, out);
  }
  if (!node.is_object() || !node.has(seg.key)) {
    return {};
  }
  return GetImpl(node[seg.key], si + 1, collect, out);
}

JsonValue JsonPath::Get(const JsonValue& root) const {
  if (!valid_) {
    return {};
  }
  return GetImpl(root, 0, false, nullptr);
}

void JsonPath::Set(JsonValue& root, JsonValue val) const {
  if (!valid_ || segments_.empty()) {
    root = std::move(val);
    return;
  }
  JsonValue* cur = &root;
  for (size_t i = 0; i + 1 < segments_.size(); ++i) {
    const Segment& seg = segments_[i];
    if (seg.wildcard) {
      return;
    }
    if (seg.is_index) {
      if (!cur->is_array() || seg.index >= cur->size()) {
        return;
      }
      cur = &((*cur)[seg.index]);
    } else {
      if (cur->is_null()) {
        *cur = JsonValue::Object();
      }
      if (!cur->is_object()) {
        return;
      }
      cur = &((*cur)[seg.key]);
    }
  }
  const Segment& last = segments_.back();
  if (last.wildcard) {
    return;
  }
  if (last.is_index) {
    if (!cur->is_array() || last.index >= cur->size()) {
      return;
    }
    (*cur)[last.index] = std::move(val);
  } else {
    if (cur->is_null()) {
      *cur = JsonValue::Object();
    }
    if (!cur->is_object()) {
      return;
    }
    cur->set(last.key, std::move(val));
  }
}

std::vector<JsonValue> JsonPath::GetAll(const JsonValue& root) const {
  std::vector<JsonValue> result;
  if (!valid_) {
    return result;
  }
  GetImpl(root, 0, true, &result);
  return result;
}

bool JsonPath::valid() const {
  return valid_;
}
const std::string& JsonPath::path() const {
  return path_;
}

// ─────────────────────────────────────────────────────────────────────────────
// ReactiveJson
// ─────────────────────────────────────────────────────────────────────────────

ReactiveJson::ReactiveJson(JsonValue initial)
    : reactive_(std::make_shared<Reactive<JsonValue>>(std::move(initial))) {}

const JsonValue& ReactiveJson::Get() const {
  return reactive_->Get();
}

void ReactiveJson::Set(JsonValue val) {
  reactive_->Set(std::move(val));
}

void ReactiveJson::Set(const std::string& path, JsonValue val) {
  reactive_->Update([&](JsonValue& root) {
    JsonPath jp(path);
    jp.Set(root, std::move(val));
  });
}

int ReactiveJson::OnChange(std::function<void(const JsonValue&)> cb) {
  return reactive_->OnChange(std::move(cb));
}

void ReactiveJson::RemoveOnChange(int id) {
  reactive_->RemoveListener(id);
}

void ReactiveJson::Patch(const JsonValue& patch) {
  reactive_->Update(
      [&](JsonValue& root) { root = Json::Merge(std::move(root), patch); });
}

std::shared_ptr<Reactive<JsonValue>> ReactiveJson::AsReactive() const {
  return reactive_;
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonElement / JsonDiff
// ─────────────────────────────────────────────────────────────────────────────

ftxui::Element JsonElement(const JsonValue& val, int indent, int max_depth) {
  return render_json(val, indent > 0 ? indent : 2, max_depth, 0);
}

ftxui::Element JsonDiff(const JsonValue& a, const JsonValue& b) {
  if (a.type() != b.type()) {
    return hbox({
               vbox({text("A:"), JsonElement(a, 2, 5)}) | flex,
               separator(),
               vbox({text("B:"), JsonElement(b, 2, 5)}) | flex,
           }) |
           border;
  }
  if (a.is_object() && b.is_object()) {
    std::vector<std::string> all_keys;
    for (const auto& k : a.keys()) {
      all_keys.push_back(k);
    }
    for (const auto& k : b.keys()) {
      if (!a.has(k)) {
        all_keys.push_back(k);
      }
    }
    std::sort(all_keys.begin(), all_keys.end());

    Elements rows;
    for (const auto& k : all_keys) {
      bool ia = a.has(k), ib = b.has(k);
      if (ia && ib) {
        if (a[k] == b[k]) {
          rows.push_back(
              hbox({text("  " + k + ": "), render_json(a[k], 2, 3, 0)}));
        } else {
          rows.push_back(hbox({
              text("~ " + k + ": ") | color(Color::Yellow),
              render_json(a[k], 2, 3, 0) | color(Color::Red),
              text(" → "),
              render_json(b[k], 2, 3, 0) | color(Color::Green),
          }));
        }
      } else if (ia) {
        rows.push_back(hbox({text("- " + k + ": ") | color(Color::Red),
                             render_json(a[k], 2, 3, 0)}));
      } else {
        rows.push_back(hbox({text("+ " + k + ": ") | color(Color::Green),
                             render_json(b[k], 2, 3, 0)}));
      }
    }
    return vbox(std::move(rows)) | border;
  }
  if (a == b) {
    return hbox({text("(identical) "), render_json(a, 2, 5, 0)});
  }

  return hbox({
             vbox({text("A:"), JsonElement(a, 2, 5)}) | flex,
             separator(),
             vbox({text("B:"), JsonElement(b, 2, 5)}) | flex,
         }) |
         border;
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonTreeView
// ─────────────────────────────────────────────────────────────────────────────

ftxui::Component JsonTreeView(const JsonValue& val,
                              const std::string& root_name) {
  return build_json_tree(root_name, val, 0);
}

ftxui::Component JsonTreeView(std::shared_ptr<ReactiveJson> reactive) {
  auto comp =
      std::make_shared<ftxui::Component>(JsonTreeView(reactive->Get(), "root"));

  reactive->OnChange(
      [comp](const JsonValue& v) { *comp = JsonTreeView(v, "root"); });

  return Renderer(*comp, [comp]() -> Element { return (*comp)->Render(); });
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonTableView
// ─────────────────────────────────────────────────────────────────────────────

ftxui::Component JsonTableView(const JsonValue& array_val) {
  return Renderer(
      [array_val]() -> Element { return build_json_table_elem(array_val); });
}

ftxui::Component JsonTableView(std::shared_ptr<ReactiveJson> reactive) {
  auto val = std::make_shared<JsonValue>(reactive->Get());
  reactive->OnChange([val](const JsonValue& v) { *val = v; });
  return Renderer([val]() -> Element { return build_json_table_elem(*val); });
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonForm
// ─────────────────────────────────────────────────────────────────────────────

ftxui::Component JsonForm(JsonValue schema,
                          std::function<void(JsonValue)> on_submit) {
  if (!schema.is_object()) {
    return Renderer([]() -> Element {
      return text("JsonForm: schema must be an object") | color(Color::Red);
    });
  }

  struct FormField {
    std::string key;
    JsonValue::Type type;
    std::string str_val;
    bool bool_val = false;
  };

  // Build per-field state in a shared vector (stable addresses after build)
  auto state = std::make_shared<std::vector<FormField>>();
  for (const auto& k : schema.keys()) {
    const auto& v = schema[k];
    FormField f;
    f.key = k;
    f.type = v.type();
    if (v.type() == JsonValue::Type::Bool) {
      f.bool_val = v.get_bool(false);
    } else if (v.type() == JsonValue::Type::Number) {
      f.str_val = num_to_str(v.as_number());
    } else {
      f.str_val = v.get_string("");
    }
    state->push_back(std::move(f));
  }

  Components comps;
  for (auto& f : *state) {
    if (f.type == JsonValue::Type::Bool) {
      comps.push_back(Checkbox(f.key, &f.bool_val));
    } else {
      InputOption opt;
      opt.placeholder = f.key;
      comps.push_back(Input(&f.str_val, opt));
    }
  }

  auto submit_cb = on_submit;
  auto btn = Button("Submit", [state, submit_cb]() {
    if (!submit_cb) {
      return;
    }
    JsonValue result = JsonValue::Object();
    for (const auto& f : *state) {
      if (f.type == JsonValue::Type::Bool) {
        result.set(f.key, JsonValue{f.bool_val});
      } else if (f.type == JsonValue::Type::Number) {
        try {
          result.set(f.key, JsonValue{std::stod(f.str_val)});
        } catch (...) {
          result.set(f.key, JsonValue{0.0});
        }
      } else {
        result.set(f.key, JsonValue{f.str_val});
      }
    }
    submit_cb(std::move(result));
  });
  comps.push_back(btn);

  auto container = Container::Vertical(comps);
  return Renderer(container, [container]() -> Element {
    return vbox({
               text("JSON Form") | bold | color(Color::Blue),
               separator(),
               container->Render(),
           }) |
           border;
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonPathInput
// ─────────────────────────────────────────────────────────────────────────────

ftxui::Component JsonPathInput(std::shared_ptr<ReactiveJson> reactive,
                               const std::string& path,
                               const std::string& placeholder) {
  auto sv = std::make_shared<std::string>();

  JsonPath jp(path);
  if (jp.valid()) {
    const auto& cur = jp.Get(reactive->Get());
    if (cur.is_string()) {
      *sv = cur.as_string();
    } else if (!cur.is_null()) {
      *sv = Json::StringifyCompact(cur);
    }
  }

  auto r2 = reactive;
  auto p2 = path;
  InputOption opt;
  opt.placeholder = placeholder.empty() ? path : placeholder;
  opt.on_change = [sv, r2, p2]() {
    JsonPath jp2(p2);
    if (jp2.valid()) {
      r2->Set(p2, JsonValue{*sv});
    }
  };
  return Input(sv.get(), opt);
}

}  // namespace ftxui::ui
