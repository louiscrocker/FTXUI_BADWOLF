// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_INSPECTOR_HPP
#define FTXUI_UI_INSPECTOR_HPP

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Wrap a component with a toggleable component-tree inspector.
///
/// Press Ctrl+I to toggle the overlay.  The overlay shows:
///  - "Component Tree Inspector" title
///  - Each child component's type info (via ChildCount traversal)
///  - The currently focused component (marked with ▶)
///  - Render-box dimensions of the root component
///
/// @param inner  The component to wrap.
/// @return A new component that forwards all events/renders to @p inner.
/// @ingroup ui
ftxui::Component WithInspector(ftxui::Component inner);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_INSPECTOR_HPP
