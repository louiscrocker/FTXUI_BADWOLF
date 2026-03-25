// BadWolf Component: json-tree v1.0.0
// Install: badwolf install json-tree
//
// Self-contained interactive JSON tree browser — no external dependencies.
// Basic JSON parser supporting objects, arrays, strings, numbers, booleans,
// and null.  Nodes are collapsible; keyboard ← / → or Enter toggles them.
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace badwolf {

// ── Minimal JSON value types ───────────────────────────────────────────────

enum class JsonKind { Object, Array, String, Number, Boolean, Null, Error };

struct JsonValue;
using JsonPtr = std::shared_ptr<JsonValue>;

struct JsonValue {
  JsonKind kind = JsonKind::Null;
  std::string string_value;   // String / Number / Boolean / Error raw text
  std::vector<std::pair<std::string, JsonPtr>> object_entries;  // Object
  std::vector<JsonPtr> array_entries;                           // Array
};

// ── Tokenizer / Parser ────────────────────────────────────────────────────

namespace detail {

inline void SkipWs(const std::string& s, size_t& i) {
  while (i < s.size() &&
         (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
    ++i;
  }
}

inline JsonPtr ParseValue(const std::string& s, size_t& i);  // forward

inline std::string ParseString(const std::string& s, size_t& i) {
  // i points at opening '"'
  ++i;
  std::string result;
  while (i < s.size() && s[i] != '"') {
    if (s[i] == '\\' && i + 1 < s.size()) {
      ++i;
      switch (s[i]) {
        case '"': result += '"'; break;
        case '\\': result += '\\'; break;
        case '/': result += '/'; break;
        case 'n': result += '\n'; break;
        case 't': result += '\t'; break;
        case 'r': result += '\r'; break;
        default: result += s[i]; break;
      }
    } else {
      result += s[i];
    }
    ++i;
  }
  if (i < s.size()) ++i;  // consume closing '"'
  return result;
}

inline JsonPtr ParseObject(const std::string& s, size_t& i) {
  // i points at '{'
  ++i;
  auto obj = std::make_shared<JsonValue>();
  obj->kind = JsonKind::Object;
  SkipWs(s, i);
  if (i < s.size() && s[i] == '}') { ++i; return obj; }
  while (i < s.size()) {
    SkipWs(s, i);
    if (i >= s.size() || s[i] != '"') break;
    std::string key = ParseString(s, i);
    SkipWs(s, i);
    if (i < s.size() && s[i] == ':') ++i;
    SkipWs(s, i);
    auto val = ParseValue(s, i);
    obj->object_entries.emplace_back(std::move(key), std::move(val));
    SkipWs(s, i);
    if (i < s.size() && s[i] == ',') { ++i; continue; }
    break;
  }
  SkipWs(s, i);
  if (i < s.size() && s[i] == '}') ++i;
  return obj;
}

inline JsonPtr ParseArray(const std::string& s, size_t& i) {
  // i points at '['
  ++i;
  auto arr = std::make_shared<JsonValue>();
  arr->kind = JsonKind::Array;
  SkipWs(s, i);
  if (i < s.size() && s[i] == ']') { ++i; return arr; }
  while (i < s.size()) {
    SkipWs(s, i);
    arr->array_entries.push_back(ParseValue(s, i));
    SkipWs(s, i);
    if (i < s.size() && s[i] == ',') { ++i; continue; }
    break;
  }
  SkipWs(s, i);
  if (i < s.size() && s[i] == ']') ++i;
  return arr;
}

inline JsonPtr ParseValue(const std::string& s, size_t& i) {
  SkipWs(s, i);
  if (i >= s.size()) {
    auto e = std::make_shared<JsonValue>();
    e->kind = JsonKind::Error;
    e->string_value = "<unexpected end>";
    return e;
  }
  char c = s[i];
  if (c == '{') return ParseObject(s, i);
  if (c == '[') return ParseArray(s, i);
  if (c == '"') {
    auto v = std::make_shared<JsonValue>();
    v->kind = JsonKind::String;
    v->string_value = ParseString(s, i);
    return v;
  }
  if (c == 't' && s.substr(i, 4) == "true") {
    auto v = std::make_shared<JsonValue>();
    v->kind = JsonKind::Boolean;
    v->string_value = "true";
    i += 4;
    return v;
  }
  if (c == 'f' && s.substr(i, 5) == "false") {
    auto v = std::make_shared<JsonValue>();
    v->kind = JsonKind::Boolean;
    v->string_value = "false";
    i += 5;
    return v;
  }
  if (c == 'n' && s.substr(i, 4) == "null") {
    auto v = std::make_shared<JsonValue>();
    v->kind = JsonKind::Null;
    v->string_value = "null";
    i += 4;
    return v;
  }
  // Number (or error)
  {
    size_t start = i;
    if (c == '-') ++i;
    while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) ||
                             s[i] == '.' || s[i] == 'e' || s[i] == 'E' ||
                             s[i] == '+' || s[i] == '-')) {
      ++i;
    }
    if (i > start) {
      auto v = std::make_shared<JsonValue>();
      v->kind = JsonKind::Number;
      v->string_value = s.substr(start, i - start);
      return v;
    }
  }
  auto e = std::make_shared<JsonValue>();
  e->kind = JsonKind::Error;
  e->string_value = std::string("?") + c;
  ++i;
  return e;
}

// ── Tree node for display ────────────────────────────────────────────────

struct TreeNode {
  std::string label;   // displayed key or index
  JsonPtr value;
  bool collapsed = false;
  std::vector<TreeNode> children;
};

inline void BuildTree(const JsonPtr& v, const std::string& label,
                      TreeNode& node) {
  node.label = label;
  node.value = v;
  if (v->kind == JsonKind::Object) {
    for (const auto& [k, child] : v->object_entries) {
      TreeNode c;
      BuildTree(child, k, c);
      node.children.push_back(std::move(c));
    }
  } else if (v->kind == JsonKind::Array) {
    for (size_t idx = 0; idx < v->array_entries.size(); ++idx) {
      TreeNode c;
      BuildTree(v->array_entries[idx], "[" + std::to_string(idx) + "]", c);
      node.children.push_back(std::move(c));
    }
  }
}

}  // namespace detail

// ── JsonTree Component ────────────────────────────────────────────────────

/// @brief Interactive JSON tree browser component.
///
/// Parse @p json_text and present it as a collapsible tree.
/// Use ↑/↓ to move the cursor and ←/→ or Enter to collapse/expand nodes.
///
/// @param json_text  Raw JSON string (UTF-8).
/// @returns An ftxui::Component ready to embed in any layout.
inline ftxui::Component JsonTree(const std::string& json_text) {
  using namespace ftxui;

  struct State {
    detail::TreeNode root;
    int cursor = 0;

    // Flatten the visible tree into a list for cursor movement.
    struct FlatNode {
      detail::TreeNode* node;
      int depth;
    };
    std::vector<FlatNode> flat;

    void Flatten(detail::TreeNode& n, int depth) {
      flat.push_back({&n, depth});
      if (!n.collapsed) {
        for (auto& child : n.children) Flatten(child, depth + 1);
      }
    }

    void Rebuild() {
      flat.clear();
      Flatten(root, 0);
    }
  };

  auto state = std::make_shared<State>();

  // Parse
  size_t pos = 0;
  auto parsed = detail::ParseValue(json_text, pos);
  detail::BuildTree(parsed, "root", state->root);
  state->Rebuild();

  class JsonTreeComponent : public ComponentBase {
   public:
    explicit JsonTreeComponent(std::shared_ptr<State> s) : state_(std::move(s)) {}

    Element Render() override {
      state_->Rebuild();
      std::vector<Element> rows;
      for (int i = 0; i < (int)state_->flat.size(); ++i) {
        const auto& fn = state_->flat[i];
        const auto& node = *fn.node;
        std::string indent(static_cast<size_t>(fn.depth * 2), ' ');

        std::string marker;
        if (!node.children.empty()) {
          marker = node.collapsed ? "▶ " : "▼ ";
        } else {
          marker = "  ";
        }

        std::string value_str;
        Color val_color = Color::Default;
        switch (node.value->kind) {
          case JsonKind::String:
            value_str = ": \"" + node.value->string_value + "\"";
            val_color = Color::GreenLight;
            break;
          case JsonKind::Number:
            value_str = ": " + node.value->string_value;
            val_color = Color::Yellow;
            break;
          case JsonKind::Boolean:
            value_str = ": " + node.value->string_value;
            val_color = Color::Cyan;
            break;
          case JsonKind::Null:
            value_str = ": null";
            val_color = Color::GrayLight;
            break;
          case JsonKind::Object:
            value_str = " {" + std::to_string(node.children.size()) + "}";
            val_color = Color::Blue;
            break;
          case JsonKind::Array:
            value_str = " [" + std::to_string(node.children.size()) + "]";
            val_color = Color::Magenta;
            break;
          case JsonKind::Error:
            value_str = ": !" + node.value->string_value;
            val_color = Color::Red;
            break;
        }

        Element row = hbox({
            text(indent + marker) | dim,
            text(node.label) | bold,
            text(value_str) | color(val_color),
        });
        if (i == state_->cursor) row = row | inverted;
        rows.push_back(row);
      }
      return vbox(rows) | vscroll_indicator | frame | border;
    }

    bool OnEvent(Event event) override {
      if (event == Event::ArrowUp || event == Event::Character('k')) {
        state_->cursor = std::max(0, state_->cursor - 1);
        return true;
      }
      if (event == Event::ArrowDown || event == Event::Character('j')) {
        state_->cursor =
            std::min((int)state_->flat.size() - 1, state_->cursor + 1);
        return true;
      }
      if (event == Event::ArrowRight || event == Event::Return) {
        if (state_->cursor < (int)state_->flat.size()) {
          state_->flat[state_->cursor].node->collapsed = false;
        }
        return true;
      }
      if (event == Event::ArrowLeft) {
        if (state_->cursor < (int)state_->flat.size()) {
          state_->flat[state_->cursor].node->collapsed = true;
        }
        return true;
      }
      return false;
    }

   private:
    std::shared_ptr<State> state_;
  };

  return std::make_shared<JsonTreeComponent>(state);
}

}  // namespace badwolf
