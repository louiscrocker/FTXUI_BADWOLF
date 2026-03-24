// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file tree_demo.cpp
/// Demonstrates ui::Tree with collapsible file-system-style nodes.

#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  SetTheme(Theme::Nord());

  std::string selected_path;
  std::string status = "Navigate with Tab/↑↓, Enter to select";

  auto on_file = [&](const std::string& path) {
    selected_path = path;
    status = "Opened: " + path;
  };

  auto tree = Tree()
    .Node(TreeNode::Branch("src/", {
        TreeNode::Branch("ftxui/", {
            TreeNode::Branch("component/", {
                TreeNode::Leaf("app.cpp",       [&]{ on_file("src/ftxui/component/app.cpp"); }),
                TreeNode::Leaf("button.cpp",    [&]{ on_file("src/ftxui/component/button.cpp"); }),
                TreeNode::Leaf("input.cpp",     [&]{ on_file("src/ftxui/component/input.cpp"); }),
                TreeNode::Leaf("menu.cpp",      [&]{ on_file("src/ftxui/component/menu.cpp"); }),
            }),
            TreeNode::Branch("dom/", {
                TreeNode::Leaf("border.cpp",    [&]{ on_file("src/ftxui/dom/border.cpp"); }),
                TreeNode::Leaf("elements.cpp",  [&]{ on_file("src/ftxui/dom/elements.cpp"); }),
                TreeNode::Leaf("flex.cpp",      [&]{ on_file("src/ftxui/dom/flex.cpp"); }),
            }),
            TreeNode::Branch("ui/", {
                TreeNode::Leaf("form.cpp",      [&]{ on_file("src/ftxui/ui/form.cpp"); }),
                TreeNode::Leaf("theme.cpp",     [&]{ on_file("src/ftxui/ui/theme.cpp"); }),
                TreeNode::Leaf("wizard.cpp",    [&]{ on_file("src/ftxui/ui/wizard.cpp"); }),
            }),
        }),
    }))
    .Node(TreeNode::Branch("include/", {
        TreeNode::Branch("ftxui/", {
            TreeNode::Leaf("ui.hpp",            [&]{ on_file("include/ftxui/ui.hpp"); }),
        }),
    }))
    .Node(TreeNode::Branch("examples/", {
        TreeNode::Branch("ui/", {
            TreeNode::Leaf("form_demo.cpp",     [&]{ on_file("examples/ui/form_demo.cpp"); }),
            TreeNode::Leaf("mvu_counter.cpp",   [&]{ on_file("examples/ui/mvu_counter.cpp"); }),
            TreeNode::Leaf("tree_demo.cpp",     [&]{ on_file("examples/ui/tree_demo.cpp"); }),
        }),
    }))
    .Node(TreeNode::Leaf("CMakeLists.txt",      [&]{ on_file("CMakeLists.txt"); }))
    .Node(TreeNode::Leaf("README.md",           [&]{ on_file("README.md"); }))
    .Build();

  auto detail = Renderer([&]() -> Element {
    const Theme& t = GetTheme();
    if (selected_path.empty()) {
      return text("(select a file)") | dim | hcenter;
    }
    return hbox({
        text("  "), text(selected_path) | bold | color(t.accent),
    });
  });

  auto layout = Container::Vertical({tree});
  auto comp = Renderer(layout, [&, tree, detail]() -> Element {
    const Theme& t = GetTheme();
    return hbox({
               vbox({
                   text(" File Tree ") | bold | hcenter | color(t.primary),
                   separatorLight(),
                   tree->Render() | yframe | flex,
               }) | borderStyled(t.border_style, t.border_color) |
                   size(WIDTH, EQUAL, 35) | flex,
               vbox({
                   text(" Detail ") | bold | hcenter | color(t.secondary),
                   separatorLight(),
                   detail->Render(),
                   separatorEmpty(),
                   text(" " + status + " ") | dim | xflex,
               }) | borderStyled(t.border_style, t.border_color) | flex,
           }) | flex;
  });

  comp |= Keymap()
    .Bind("q", []{ if (App* a = App::Active()) a->Exit(); }, "Quit")
    .AsDecorator();

  App::Fullscreen().Loop(comp);
  return 0;
}
