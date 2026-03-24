// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_TREE_HPP
#define FTXUI_UI_TREE_HPP

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief A node in a Tree component hierarchy.
/// @ingroup ui
struct TreeNode {
  std::string label;
  std::vector<TreeNode> children;
  std::function<void()> on_select;

  /// @brief Create a leaf node (no children).
  static TreeNode Leaf(std::string_view label,
                       std::function<void()> on_select = {});

  /// @brief Create a branch node (has children).
  static TreeNode Branch(std::string_view label,
                         std::vector<TreeNode> children,
                         std::function<void()> on_select = {});
};

/// @brief Fluent builder for a collapsible tree-view component.
///
/// @code
/// auto tree = ftxui::ui::Tree()
///     .Node(TreeNode::Branch("src", {
///         TreeNode::Leaf("main.cpp", [&]{ open("main.cpp"); }),
///         TreeNode::Leaf("util.cpp"),
///     }))
///     .OnSelect([](const std::string& label){ ... })
///     .Build();
/// @endcode
///
/// @ingroup ui
class Tree {
 public:
  /// @brief Append a root node.
  Tree& Node(TreeNode node);

  /// @brief Set a global callback fired whenever any node is selected.
  Tree& OnSelect(std::function<void(const std::string& label)> callback);

  /// @brief Build the FTXUI component.
  ftxui::Component Build() const;

 private:
  std::vector<TreeNode> roots_;
  std::function<void(const std::string&)> on_select_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_TREE_HPP
