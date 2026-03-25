// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/inspector.hpp"

#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

using namespace ftxui;

namespace ftxui::ui {

namespace {

// Walk the component tree collecting lines of text.
void WalkTree(ComponentBase* node,
              int depth,
              std::vector<std::string>& lines,
              ComponentBase* focused) {
  if (!node) {
    return;
  }
  std::string prefix(depth * 2, ' ');
  bool is_focused = (node == focused);
  std::string marker = is_focused ? "▶ " : "  ";
  std::string type_name = "Component";
  lines.push_back(prefix + marker + type_name);
  for (size_t i = 0; i < node->ChildCount(); ++i) {
    auto child = node->ChildAt(i);
    if (child) {
      WalkTree(child.get(), depth + 1, lines, focused);
    }
  }
}

class InspectorComponent : public ComponentBase {
 public:
  explicit InspectorComponent(Component inner) : inner_(std::move(inner)) {
    Add(inner_);
  }

  Element OnRender() override {
    Element content = inner_->Render();
    if (!show_inspector_) {
      return content;
    }

    // Build tree lines.
    std::vector<std::string> lines;
    WalkTree(inner_.get(), 0, lines, inner_.get());

    Elements tree_elems;
    for (const auto& line : lines) {
      tree_elems.push_back(text(line));
    }
    if (tree_elems.empty()) {
      tree_elems.push_back(text("(no children)") | dim);
    }

    char dims_buf[64];
    std::snprintf(dims_buf, sizeof(dims_buf), "Root children: %zu",
                  inner_->ChildCount());

    Element panel =
        vbox({
            separatorLight(),
            hbox({
                text(" ╭─ Component Inspector ") | color(Color::Cyan) | bold,
                filler(),
                text("Ctrl+I to hide ") | dim,
            }),
            hbox({
                text(" "),
                vbox(std::move(tree_elems)) | frame |
                    size(HEIGHT, LESS_THAN, 8),
            }),
            hbox({
                text(" "),
                text(dims_buf) | dim,
            }),
        }) |
        border;

    return vbox({content | flex, panel});
  }

  bool OnEvent(Event event) override {
    // Ctrl+I toggles the inspector.
    if (event == Event::CtrlI) {
      show_inspector_ = !show_inspector_;
      return true;
    }

    return inner_->OnEvent(event);
  }

  Component ActiveChild() override { return inner_; }

 private:
  Component inner_;
  bool show_inspector_ = false;
};

}  // namespace

ftxui::Component WithInspector(ftxui::Component inner) {
  return std::make_shared<InspectorComponent>(std::move(inner));
}

}  // namespace ftxui::ui
