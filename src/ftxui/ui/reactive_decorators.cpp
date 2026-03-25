// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "ftxui/ui/reactive_decorators.hpp"

#include <functional>
#include <memory>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/reactive.hpp"

namespace ftxui::ui {

ftxui::Element ShowIf(ftxui::Element e,
                      std::shared_ptr<Reactive<bool>> condition) {
  if (condition && condition->Get()) {
    return e;
  }
  return ftxui::emptyElement();
}

ftxui::Element HideIf(ftxui::Element e,
                      std::shared_ptr<Reactive<bool>> condition) {
  if (condition && condition->Get()) {
    return ftxui::emptyElement();
  }
  return e;
}

ftxui::Element BindText(std::shared_ptr<Reactive<std::string>> text_reactive) {
  if (!text_reactive) {
    return ftxui::text("");
  }
  return ftxui::text(text_reactive->Get());
}

ftxui::Element BindText(std::shared_ptr<Reactive<std::string>> text_reactive,
                        bool is_bold) {
  auto e = BindText(std::move(text_reactive));
  if (is_bold) {
    return e | ftxui::bold;
  }
  return e;
}

ftxui::Element BindColor(
    ftxui::Element e,
    std::shared_ptr<Reactive<ftxui::Color>> color_reactive) {
  if (!color_reactive) {
    return e;
  }
  return e | ftxui::color(color_reactive->Get());
}

ftxui::Component ReactiveBox(std::function<ftxui::Element()> render_fn,
                             std::vector<std::shared_ptr<void>> /*deps*/) {
  return ftxui::Renderer(
      [render_fn = std::move(render_fn)]() -> ftxui::Element {
        return render_fn();
      });
}

}  // namespace ftxui::ui
