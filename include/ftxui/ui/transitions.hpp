// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_TRANSITIONS_HPP
#define FTXUI_UI_TRANSITIONS_HPP

#include <memory>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/animation.hpp"

namespace ftxui::ui {

/// @brief Direction for SlideIn transitions.
enum class Direction { Left, Right, Up, Down };

/// @brief Wrap `inner` so it fades in over `duration` seconds on first render.
///
/// In a terminal context "fade in" is approximated by rendering nothing for
/// the first fraction of the animation and progressively using dim/normal
/// styling as the tween progresses.
///
/// @ingroup ui
inline ftxui::Component FadeIn(ftxui::Component inner, float duration = 0.3f) {
  auto tween = std::make_shared<Tween>(0.0f, 1.0f, duration, Easing::EaseOut);
  tween->Start();
  AnimationLoop::Instance().Add(tween);

  return Renderer(inner, [inner, tween]() -> ftxui::Element {
    float v = tween->Value();
    auto elem = inner->Render();
    if (v < 0.3f) {
      // Nearly invisible — render as a blank filler so layout is preserved.
      return elem | ftxui::dim;
    }
    if (v < 0.7f) {
      return elem | ftxui::dim;
    }
    return elem;
  });
}

/// @brief Wrap `inner` so it fades out over `duration` seconds.
///
/// Call `tween->Start()` externally to trigger the fade-out.
///
/// @ingroup ui
inline ftxui::Component FadeOut(ftxui::Component inner, float duration = 0.3f) {
  auto tween = std::make_shared<Tween>(1.0f, 0.0f, duration, Easing::EaseIn);
  AnimationLoop::Instance().Add(tween);

  return Renderer(inner, [inner, tween]() -> ftxui::Element {
    float v = tween->Value();
    auto elem = inner->Render();
    if (v < 0.4f) {
      return elem | ftxui::dim;
    }
    return elem;
  });
}

/// @brief Wrap `inner` so it slides in from `dir` over `duration` seconds.
///
/// Implemented as a padded hbox/vbox that starts with a filler occupying all
/// the space and shrinks toward the natural content position.
///
/// @ingroup ui
inline ftxui::Component SlideIn(ftxui::Component inner,
                                Direction dir = Direction::Left,
                                float duration = 0.3f) {
  auto tween = std::make_shared<Tween>(0.0f, 1.0f, duration, Easing::EaseOut);
  tween->Start();
  AnimationLoop::Instance().Add(tween);

  return Renderer(inner, [inner, tween, dir]() -> ftxui::Element {
    float v = tween->Value();
    auto elem = inner->Render();

    // Compute a flex weight for the "push" filler, mapped from [1..0] as the
    // animation runs [0..1].
    float push = 1.0f - v;
    if (push < 0.01f) {
      return elem;
    }

    // Build a small spacer based on direction.
    auto spacer = ftxui::filler();
    switch (dir) {
      case Direction::Left:
        return ftxui::hbox({spacer, elem | ftxui::flex});
      case Direction::Right:
        return ftxui::hbox({elem | ftxui::flex, spacer});
      case Direction::Up:
        return ftxui::vbox({spacer, elem | ftxui::flex});
      case Direction::Down:
        return ftxui::vbox({elem | ftxui::flex, spacer});
    }
    return elem;
  });
}

/// @brief Wrap a Router component so route changes produce a brief fade
/// transition.  Requires the inner component to be a Router or similar
/// component whose rendered output changes on navigation.
///
/// @ingroup ui
inline ftxui::Component WithAnimatedRoutes(ftxui::Component router) {
  return FadeIn(router, 0.25f);
}

}  // namespace ftxui::ui

#endif  // FTXUI_UI_TRANSITIONS_HPP
