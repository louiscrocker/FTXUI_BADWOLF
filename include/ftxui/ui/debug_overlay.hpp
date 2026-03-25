// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_DEBUG_OVERLAY_HPP
#define FTXUI_UI_DEBUG_OVERLAY_HPP

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Wraps any component with a toggleable debug overlay (Ctrl+D).
///
/// When active the overlay shows a panel at the bottom of the screen with:
///  - Render time in milliseconds
///  - Event rate (events per second)
///  - Last received event description
///  - Last known mouse position
///  - Terminal dimensions
///
/// The overlay is **opt-in** — users must call `WithDebugOverlay()`.
/// It is never auto-enabled.
///
/// @code
/// auto app = WithDebugOverlay(my_component);
/// @endcode
///
/// @param inner  The component to wrap.
/// @return A new component that forwards all events/renders to @p inner and
///         overlays the debug panel when Ctrl+D has been pressed.
/// @ingroup ui
ftxui::Component WithDebugOverlay(ftxui::Component inner);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_DEBUG_OVERLAY_HPP
