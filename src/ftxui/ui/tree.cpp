// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/tree.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── TreeNode factories ────────────────────────────────────────────────────────

TreeNode TreeNode::Leaf(std::string_view label,
                        std::function<void()> on_select) {
  return TreeNode{std::string(label), {}, std::move(on_select)};
}

TreeNode TreeNode::Branch(std::string_view label,
                          std::vector<TreeNode> children,
                          std::function<void()> on_select) {
  return TreeNode{std::string(label), std::move(children),
                  std::move(on_select)};
}

// ── Internal recursive builder ────────────────────────────────────────────────

namespace {

Component BuildNode(const TreeNode& node, int depth,
                    const std::function<void(const std::string&)>& global_cb) {
  std::string indent(static_cast<size_t>(depth) * 2, ' ');

  if (node.children.empty()) {
    // Leaf: a selectable row rendered as plain text with focus highlight.
    auto label = node.label;
    auto on_select = node.on_select;
    auto combined = [on_select, label, global_cb]() {
      if (on_select) {
        on_select();
      }
      if (global_cb) {
        global_cb(label);
      }
    };

    ButtonOption opt;
    opt.transform = [indent, label](const EntryState& s) -> Element {
      Element e = text(indent + "  ├─ " + label);
      if (s.focused) {
        e = e | color(GetTheme().primary);
      }
      return e;
    };
    return Button(label, std::move(combined), opt);
  }

  // Branch: use Collapsible, optionally fire callbacks on Enter.
  Components children;
  children.reserve(node.children.size());
  for (const auto& child : node.children) {
    children.push_back(BuildNode(child, depth + 1, global_cb));
  }
  auto child_container = Container::Vertical(std::move(children));
  auto branch_on_select = node.on_select;
  auto label = node.label;

  Component result = Collapsible(indent + label, child_container);

  if (branch_on_select || global_cb) {
    result = CatchEvent(result, [branch_on_select, label,
                                 global_cb](Event e) -> bool {
      if (e == Event::Return) {
        if (branch_on_select) {
          branch_on_select();
        }
        if (global_cb) {
          global_cb(label);
        }
        // Return false so Collapsible can also toggle on Enter.
        return false;
      }
      return false;
    });
  }

  return result;
}

}  // namespace

// ── Tree ──────────────────────────────────────────────────────────────────────

Tree& Tree::Node(TreeNode node) {
  roots_.push_back(std::move(node));
  return *this;
}

Tree& Tree::OnSelect(std::function<void(const std::string& label)> callback) {
  on_select_ = std::move(callback);
  return *this;
}

ftxui::Component Tree::Build() const {
  auto global_cb = on_select_;
  Components components;
  components.reserve(roots_.size());
  for (const auto& root : roots_) {
    components.push_back(BuildNode(root, 0, global_cb));
  }
  return Container::Vertical(std::move(components));
}

}  // namespace ftxui::ui
