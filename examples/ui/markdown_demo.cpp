// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file markdown_demo.cpp
/// Live Markdown editor + preview using ftxui::ui::Markdown.
/// Left panel: editable raw Markdown input.
/// Right panel: live-rendered preview via MarkdownComponent.
/// Keys: [T] cycle theme, [Q] quit.

#include <array>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/markdown.hpp"

using namespace ftxui;
using namespace ftxui::ui;

static const std::string kSampleMarkdown = R"(# FTXUI Markdown Renderer

Welcome to the **live Markdown preview**!
Edit the left panel to see *instant* updates.

---

## Features Showcase

### Text Formatting

This paragraph contains **bold text**, *italic text*,
and `inline code` all in one line.

### Links

Visit [FTXUI on GitHub](https://github.com/ArthurSonzogni/FTXUI) for docs.

---

## Lists

### Unordered List

- First item with **bold**
- Second item with *italic*
- Third item with `code`
- Fourth item: [a link](https://example.com)

### Ordered List

1. Step one: install FTXUI
2. Step two: include headers
3. Step three: build your UI
4. Step four: profit!

---

## Blockquote

> The best user interface is the one the user
> never has to think about.

---

## Code Block

```cpp
#include "ftxui/ui.hpp"
using namespace ftxui;
using namespace ftxui::ui;

int main() {
  auto el = Markdown("# Hello!");
  ui::Run(Renderer([&]{ return el; }));
}
```

---

### H3 Header Example

___

> Nested **bold** inside a blockquote!

- List item after quote
- Another *italic* item

1. Ordered after unordered
2. Works great

---

*Thank you for using FTXUI!*
)";

int main() {
  // Cycle through available themes
  static const std::array<Theme, 6> kThemes = {
      Theme::Default(), Theme::Dark(),    Theme::Light(),
      Theme::Nord(),    Theme::Dracula(), Theme::Monokai(),
  };
  static const std::array<std::string, 6> kThemeNames = {
      "Default", "Dark", "Light", "Nord", "Dracula", "Monokai",
  };
  int theme_index = 3;  // start with Nord
  SetTheme(kThemes[static_cast<size_t>(theme_index)]);

  // Editable markdown source
  std::string md_source = kSampleMarkdown;

  // Left panel: multi-line input editor
  auto input_option = InputOption::Default();
  input_option.multiline = true;
  auto editor = Input(&md_source, "", input_option);

  // Right panel: live preview — rebuilt each frame from md_source
  auto preview = Renderer([&]() -> Element {
    return Markdown(md_source) | yframe | vscroll_indicator | flex;
  });

  // Layout: left editor | right preview, split 50/50
  int split_size = 50;
  auto layout = ResizableSplitLeft(
      editor | flex | border,
      preview | flex | border,
      &split_size
  );

  // Status bar
  auto root = Renderer(layout, [&]() -> Element {
    const Theme& t = GetTheme();
    std::string theme_label =
        "Theme: " + kThemeNames[static_cast<size_t>(theme_index)];
    return vbox({
        hbox({
            text(" Markdown Demo ") | bold | color(t.primary),
            filler(),
            text(" [T] cycle theme  [Q] quit ") | dim,
        }) | color(t.border_color),
        layout->Render() | flex,
        hbox({
            text(" " + theme_label + " ") | color(t.accent),
            filler(),
            text(" ↑↓ scroll preview ") | dim,
        }),
    });
  });

  root |= CatchEvent([&](Event event) -> bool {
    if (event.is_character()) {
      const std::string& ch = event.character();
      if (ch == "q" || ch == "Q") {
        if (App* a = App::Active()) a->Exit();
        return true;
      }
      if (ch == "t" || ch == "T") {
        theme_index = (theme_index + 1) % static_cast<int>(kThemes.size());
        SetTheme(kThemes[static_cast<size_t>(theme_index)]);
        return true;
      }
    }
    return false;
  });

  RunFullscreen(root);
  return 0;
}
