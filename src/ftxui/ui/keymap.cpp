// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/keymap.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

namespace {

// Parse a key string such as "q", "Ctrl+s", "F1", "Enter" into an Event.
// Returns Event::Custom as a sentinel if the key string is not recognised.
Event ParseKey(std::string_view key) {
  // Named keys.
  if (key == "Enter")     return Event::Return;
  if (key == "Return")    return Event::Return;
  if (key == "Escape")    return Event::Escape;
  if (key == "Esc")       return Event::Escape;
  if (key == "Tab")       return Event::Tab;
  if (key == "Backspace") return Event::Backspace;
  if (key == "Delete")    return Event::Delete;
  if (key == "Up")        return Event::ArrowUp;
  if (key == "Down")      return Event::ArrowDown;
  if (key == "Left")      return Event::ArrowLeft;
  if (key == "Right")     return Event::ArrowRight;
  if (key == "Home")      return Event::Home;
  if (key == "End")       return Event::End;
  if (key == "PageUp")    return Event::PageUp;
  if (key == "PageDown")  return Event::PageDown;

  // F keys: "F1" … "F12".
  if (key.size() >= 2 && (key[0] == 'F' || key[0] == 'f') &&
      std::isdigit(static_cast<unsigned char>(key[1]))) {
    int n = std::stoi(std::string(key.substr(1)));
    switch (n) {
      case 1:  return Event::F1;
      case 2:  return Event::F2;
      case 3:  return Event::F3;
      case 4:  return Event::F4;
      case 5:  return Event::F5;
      case 6:  return Event::F6;
      case 7:  return Event::F7;
      case 8:  return Event::F8;
      case 9:  return Event::F9;
      case 10: return Event::F10;
      case 11: return Event::F11;
      case 12: return Event::F12;
      default: break;
    }
  }

  // "Ctrl+X" or "ctrl+x".
  {
    std::string lower(key);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower.size() == 6 && lower.substr(0, 5) == "ctrl+") {
      char ch = lower[5];
      switch (ch) {
        case 'a': return Event::CtrlA;
        case 'b': return Event::CtrlB;
        case 'c': return Event::CtrlC;
        case 'd': return Event::CtrlD;
        case 'e': return Event::CtrlE;
        case 'f': return Event::CtrlF;
        case 'g': return Event::CtrlG;
        case 'h': return Event::CtrlH;
        case 'i': return Event::CtrlI;
        case 'j': return Event::CtrlJ;
        case 'k': return Event::CtrlK;
        case 'l': return Event::CtrlL;
        case 'm': return Event::CtrlM;
        case 'n': return Event::CtrlN;
        case 'o': return Event::CtrlO;
        case 'p': return Event::CtrlP;
        case 'q': return Event::CtrlQ;
        case 'r': return Event::CtrlR;
        case 's': return Event::CtrlS;
        case 't': return Event::CtrlT;
        case 'u': return Event::CtrlU;
        case 'v': return Event::CtrlV;
        case 'w': return Event::CtrlW;
        case 'x': return Event::CtrlX;
        case 'y': return Event::CtrlY;
        case 'z': return Event::CtrlZ;
        default:  break;
      }
    }
  }

  // Single printable character: "q", "?", "/"
  if (key.size() == 1) {
    return Event::Character(key[0]);
  }

  // Unrecognised – return a sentinel that will never match a real event.
  return Event::Custom;
}

}  // namespace

// ── Keymap ────────────────────────────────────────────────────────────────────

Keymap& Keymap::Bind(std::string_view key,
                     std::function<void()> action,
                     std::string_view description) {
  bindings_.push_back(
      {std::string(key), std::move(action), std::string(description)});
  return *this;
}

Component Keymap::Wrap(Component component) const {
  // Build a copy of bindings as {parsed_event, action} pairs.
  struct ParsedBinding {
    Event event;
    std::function<void()> action;
  };
  auto parsed = std::make_shared<std::vector<ParsedBinding>>();
  for (const auto& b : bindings_) {
    parsed->push_back({ParseKey(b.key), b.action});
  }

  return CatchEvent(std::move(component), [parsed](Event event) -> bool {
    for (const auto& pb : *parsed) {
      if (event == pb.event) {
        pb.action();
        return true;
      }
    }
    return false;
  });
}

ComponentDecorator Keymap::AsDecorator() const {
  // Capture a copy of this Keymap's bindings.
  auto copy = *this;
  return [copy = std::move(copy)](Component comp) -> Component {
    return copy.Wrap(std::move(comp));
  };
}

Element Keymap::HelpElement() const {
  if (bindings_.empty()) {
    return text("(no keybindings registered)") | dim;
  }

  const Theme& t = GetTheme();
  Elements rows;
  rows.push_back(hbox({
      text("Key") | bold | color(t.primary) | size(WIDTH, EQUAL, 12),
      text("Action") | bold | color(t.primary),
  }));
  rows.push_back(separator());

  for (const auto& b : bindings_) {
    rows.push_back(hbox({
        text(b.key) | color(t.primary) | size(WIDTH, EQUAL, 12),
        text(b.description.empty() ? b.key : b.description),
    }));
  }

  return vbox(std::move(rows));
}

}  // namespace ftxui::ui
