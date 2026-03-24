// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/dialog.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Confirm dialog ────────────────────────────────────────────────────────────

Component WithConfirm(Component parent,
                      std::string_view message,
                      std::function<void()> on_yes,
                      std::function<void()> on_no,
                      const bool* show) {
  std::string msg{message};
  auto btn_style = ButtonOption::Animated();

  auto btn_yes = ftxui::Button("  Yes  ", std::move(on_yes), btn_style);
  auto btn_no = ftxui::Button("  No   ", std::move(on_no), btn_style);
  auto buttons = Container::Horizontal({btn_yes, btn_no});

  auto modal_comp = Renderer(buttons, [msg, buttons]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               text(msg) | bold | hcenter,
               separatorEmpty(),
               buttons->Render() | hcenter,
           }) |
           borderStyled(t.border_style, t.error_color) |
           size(WIDTH, GREATER_THAN, 30) | center;
  });

  parent |= Modal(modal_comp, show);
  return parent;
}

ComponentDecorator WithConfirm(std::string_view message,
                                std::function<void()> on_yes,
                                std::function<void()> on_no,
                                const bool* show) {
  return [message = std::string(message), on_yes = std::move(on_yes),
          on_no = std::move(on_no), show](Component comp) -> Component {
    return WithConfirm(std::move(comp), message, on_yes, on_no, show);
  };
}

// ── Alert dialog ──────────────────────────────────────────────────────────────

Component WithAlert(Component parent,
                    std::string_view title,
                    std::string_view message,
                    const bool* show,
                    std::function<void()> on_close) {
  std::string ttl{title};
  std::string msg{message};

  auto close_fn = on_close ? std::move(on_close) : [] {};
  auto btn_ok = ftxui::Button("  OK  ", close_fn, ButtonOption::Animated());
  auto modal_comp = Renderer(btn_ok, [ttl, msg, btn_ok]() -> Element {
    const Theme& t = GetTheme();
    return vbox({
               text(" " + ttl + " ") | bold | hcenter,
               separatorLight(),
               paragraph(msg),
               separatorEmpty(),
               btn_ok->Render() | hcenter,
           }) |
           borderStyled(t.border_style, t.primary) |
           size(WIDTH, GREATER_THAN, 30) | center;
  });

  parent |= Modal(modal_comp, show);
  return parent;
}

ComponentDecorator WithAlert(std::string_view title,
                              std::string_view message,
                              const bool* show,
                              std::function<void()> on_close) {
  return [title = std::string(title), message = std::string(message), show,
          on_close = std::move(on_close)](Component comp) -> Component {
    return WithAlert(std::move(comp), title, message, show, on_close);
  };
}

// ── Help dialog ───────────────────────────────────────────────────────────────

Component WithHelp(
    Component parent,
    std::string_view title,
    const std::vector<std::pair<std::string, std::string>>& bindings,
    const bool* show,
    std::function<void()> on_close) {
  std::string ttl{title};
  auto bindings_copy =
      std::make_shared<std::vector<std::pair<std::string, std::string>>>(
          bindings);

  auto close_fn = on_close ? std::move(on_close) : [] {};
  auto btn_close =
      ftxui::Button("  Close  ", close_fn, ButtonOption::Animated());

  auto modal_comp = Renderer(btn_close, [ttl, bindings_copy, btn_close]() -> Element {
    const Theme& t = GetTheme();
    Elements rows;
    rows.push_back(text(" " + ttl + " ") | bold | hcenter);
    rows.push_back(separatorLight());

    // Two-column layout: key | description
    for (const auto& [key, desc] : *bindings_copy) {
      rows.push_back(hbox({
          text(key) | bold | color(t.primary) | size(WIDTH, EQUAL, 12),
          text(desc),
      }));
    }
    rows.push_back(separatorEmpty());
    rows.push_back(btn_close->Render() | hcenter);

    return vbox(std::move(rows)) |
           borderStyled(t.border_style, t.secondary) |
           size(WIDTH, GREATER_THAN, 36) | center;
  });

  parent |= Modal(modal_comp, show);
  return parent;
}

ComponentDecorator WithHelp(
    std::string_view title,
    const std::vector<std::pair<std::string, std::string>>& bindings,
    const bool* show,
    std::function<void()> on_close) {
  return [title = std::string(title),
          bindings =
              std::vector<std::pair<std::string, std::string>>(bindings),
          show, on_close = std::move(on_close)](Component comp) -> Component {
    return WithHelp(std::move(comp), title, bindings, show, on_close);
  };
}

}  // namespace ftxui::ui
