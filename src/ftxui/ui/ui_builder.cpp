// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/ui_builder.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ---------------------------------------------------------------------------
// Internal tree model
// ---------------------------------------------------------------------------

namespace {

struct UINode {
  std::string type;
  std::string label;
  std::vector<UINode> children;
};

// Recursive C++ code generation.
std::string GenerateCode(const UINode& node, int indent = 0) {
  std::string pad(indent * 2, ' ');
  if (node.type == "text") {
    return pad + "ftxui::text(\"" + node.label + "\")";
  }
  if (node.type == "separator") {
    return pad + "ftxui::separator()";
  }
  if (node.type == "gauge") {
    return pad + "ftxui::gauge(0.5f)";
  }
  if (node.type == "spinner") {
    return pad + "ftxui::spinner(0, 0)";
  }
  if (node.type == "button") {
    return pad + "ftxui::Button(\"" + node.label +
           "\", []{ /* action */ })->Render()";
  }

  // Container types: hbox, vbox, border
  auto gen_children = [&]() -> std::string {
    if (node.children.empty()) {
      return pad + "  ftxui::text(\"(empty)\")";
    }
    std::string result;
    for (const auto& c : node.children) {
      if (!result.empty()) {
        result += ",\n";
      }
      result += GenerateCode(c, indent + 1);
    }
    return result;
  };

  if (node.type == "hbox") {
    return pad + "ftxui::hbox({\n" + gen_children() + "\n" + pad + "})";
  }
  if (node.type == "vbox") {
    return pad + "ftxui::vbox({\n" + gen_children() + "\n" + pad + "})";
  }
  if (node.type == "border") {
    std::string inner = node.children.empty()
                            ? (pad + "  ftxui::text(\"(empty)\")")
                            : GenerateCode(node.children.front(), indent + 1);
    return pad + "ftxui::border(\n" + inner + "\n" + pad + ")";
  }

  return pad + "ftxui::text(\"" + node.type + "\")";
}

// Render a tree as ASCII-art preview elements.
Element RenderTree(const std::vector<UINode>& nodes, int depth = 0) {
  if (nodes.empty()) {
    return text("(empty tree)") | dim;
  }
  Elements rows;
  for (const auto& node : nodes) {
    std::string prefix(depth * 2, ' ');
    prefix += depth > 0 ? "└ " : "• ";
    auto row = text(prefix + node.type +
                    (node.label.empty() ? "" : " \"" + node.label + "\""));
    if (!node.children.empty()) {
      rows.push_back(row);
      rows.push_back(RenderTree(node.children, depth + 1));
    } else {
      rows.push_back(row);
    }
  }
  return vbox(std::move(rows));
}

// ---------------------------------------------------------------------------
// UIBuilderComponent
// ---------------------------------------------------------------------------

static const std::vector<std::string> kPalette = {
    "text", "hbox", "vbox", "border", "separator", "button", "gauge", "spinner",
};

class UIBuilderComponent : public ComponentBase {
 public:
  UIBuilderComponent() {
    // Create the palette menu component.
    palette_menu_ = ftxui::Menu(&kPalette, &palette_selected_);
    Add(palette_menu_);
  }

  Element OnRender() override {
    // ── Build current generated code ──────────────────────────────────────
    std::string code = BuildCode();

    // ── Left: Palette ─────────────────────────────────────────────────────
    Element palette_panel = vbox({
                                text(" Palette ") | bold | center,
                                separator(),
                                palette_menu_->Render() | frame | flex,
                                separator(),
                                text(" Enter=add  Del=remove ") | dim | center,
                            }) |
                            border | size(WIDTH, EQUAL, 18);

    // ── Center: Preview ───────────────────────────────────────────────────
    Element preview_panel = vbox({
                                text(" Preview ") | bold | center,
                                separator(),
                                RenderTree(tree_) | flex,
                            }) |
                            border | flex;

    // ── Right: Code ───────────────────────────────────────────────────────
    // Split code into lines for display.
    Elements code_lines;
    {
      std::istringstream ss(code);
      std::string line;
      while (std::getline(ss, line)) {
        code_lines.push_back(text(line));
      }
    }
    if (code_lines.empty()) {
      code_lines.push_back(text("(no elements)") | dim);
    }
    Element code_panel = vbox({
                             text(" Generated C++ ") | bold | center,
                             separator(),
                             vbox(std::move(code_lines)) | frame | flex,
                             separator(),
                             text(" s=export  q=quit ") | dim | center,
                         }) |
                         border | size(WIDTH, EQUAL, 40);

    // ── Status bar ────────────────────────────────────────────────────────
    std::string status =
        "  Visual UI Builder  |  Tab=focus  Enter=add  "
        "Del=remove  s=export  q=quit  ";
    if (!status_msg_.empty()) {
      status = "  " + status_msg_;
    }
    Element status_bar = text(status) | inverted;

    return vbox({
        hbox({palette_panel, preview_panel, code_panel}) | flex,
        status_bar,
    });
  }

  bool OnEvent(Event event) override {
    // Forward to palette menu first.
    if (palette_menu_->OnEvent(event)) {
      return true;
    }

    // Enter: add selected palette item.
    if (event == Event::Return) {
      if (palette_selected_ >= 0 &&
          palette_selected_ < static_cast<int>(kPalette.size())) {
        UINode node;
        node.type = kPalette[palette_selected_];
        node.label = (node.type == "text" || node.type == "button")
                         ? node.type + "_" + std::to_string(tree_.size() + 1)
                         : "";
        tree_.push_back(std::move(node));
        status_msg_ = "Added: " + kPalette[palette_selected_];
      }
      return true;
    }

    // Del: remove last element.
    if (event == Event::Delete && !tree_.empty()) {
      tree_.pop_back();
      status_msg_ = "Removed last element.";
      return true;
    }

    // 's': export to file.
    if (event == Event::Character('s')) {
      ExportCode();
      return true;
    }

    // 'q' or Escape: exit.
    if (event == Event::Character('q') || event == Event::Escape) {
      auto* app = ftxui::App::Active();
      if (app) {
        app->Exit();
      }
      return true;
    }

    // Ctrl+C: dump to stdout.
    if (event == Event::Special("\x03")) {
      fprintf(stdout, "\n--- Generated C++ ---\n%s\n", BuildCode().c_str());
      fflush(stdout);
      status_msg_ = "Code written to stdout.";
      return true;
    }

    return false;
  }

 private:
  std::vector<UINode> tree_;
  int palette_selected_ = 0;
  ftxui::Component palette_menu_;
  std::string status_msg_;

  std::string BuildCode() const {
    if (tree_.empty()) {
      return "// (no elements added yet)";
    }
    std::ostringstream oss;
    oss << "#include \"ftxui/dom/elements.hpp\"\n";
    oss << "using namespace ftxui;\n\n";
    oss << "Element Build() {\n";
    if (tree_.size() == 1) {
      oss << "  return " << GenerateCode(tree_.front(), 1).substr(2) << ";\n";
    } else {
      oss << "  return vbox({\n";
      for (size_t i = 0; i < tree_.size(); ++i) {
        oss << GenerateCode(tree_[i], 2);
        if (i + 1 < tree_.size()) {
          oss << ",";
        }
        oss << "\n";
      }
      oss << "  });\n";
    }
    oss << "}\n";
    return oss.str();
  }

  void ExportCode() {
    std::ofstream out("builder_output.cpp");
    if (!out.is_open()) {
      status_msg_ = "ERROR: could not write builder_output.cpp";
      return;
    }
    out << BuildCode();
    status_msg_ = "Exported to builder_output.cpp";
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// Public factory
// ---------------------------------------------------------------------------

ftxui::Component UIBuilder() {
  return std::make_shared<UIBuilderComponent>();
}

}  // namespace ftxui::ui
