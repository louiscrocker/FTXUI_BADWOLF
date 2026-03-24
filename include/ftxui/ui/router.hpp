// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_ROUTER_HPP
#define FTXUI_UI_ROUTER_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief History-based client-side router for multi-screen terminal apps.
///
/// Register named routes (either a pre-built Component or a lazy factory),
/// set a default, then call Build() to get a single Component that always
/// renders the current route.  Navigation is performed via Push/Pop/Replace
/// from within event handlers or button callbacks.
///
/// @code
/// ftxui::ui::Router router;
/// router.Route("home",    home_screen)
///       .Route("settings", []{ return MakeSettings(); })
///       .Default("home");
///
/// auto root = router.Build();
/// // Inside a button: router.Push("settings");
/// @endcode
///
/// @ingroup ui
class Router {
 public:
  Router();
  ~Router();

  /// @brief Register a pre-built component for @p name.
  Router& Route(std::string_view name, ftxui::Component component);

  /// @brief Register a lazy factory for @p name (created on first navigation).
  Router& Route(std::string_view name,
                std::function<ftxui::Component()> factory);

  /// @brief Set the initial/default route (pushed on first Build()).
  Router& Default(std::string_view name);

  /// @brief Callback invoked whenever the active route changes.
  Router& OnNavigate(
      std::function<void(std::string_view from, std::string_view to)> cb);

  // ── Navigation ─────────────────────────────────────────────────────────────

  /// @brief Push @p name onto the history stack and navigate to it.
  void Push(std::string_view name);

  /// @brief Pop the current route; navigates back (no-op if history size ≤ 1).
  void Pop();

  /// @brief Replace the current route with @p name (no extra history entry).
  void Replace(std::string_view name);

  /// @brief Return the name of the currently active route.
  std::string_view Current() const;

  /// @brief Return true if there is a previous route to go back to.
  bool CanGoBack() const;

  // ── Build ──────────────────────────────────────────────────────────────────

  /// @brief Build the root component.  Can be called multiple times; all
  ///        returned components share the same navigation state.
  ftxui::Component Build();

  struct Impl;

 private:
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_ROUTER_HPP
