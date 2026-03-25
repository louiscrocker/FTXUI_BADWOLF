// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_WASM_BRIDGE_HPP
#define FTXUI_UI_WASM_BRIDGE_HPP

#include <functional>
#include <string>
#include <utility>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Run a BadWolf app in a portable way.
///
/// - On **native** builds delegates to `RunFullscreen()`.
/// - On **WASM** builds installs the app via `emscripten_set_main_loop` so the
///   browser's event loop drives rendering.
///
/// @param app_factory Called once to create the root component.
void RunApp(std::function<ftxui::Component()> app_factory);

/// @brief Post a redraw request to the running application.
///
/// - Native: calls the active App event queue.
/// - WASM: schedules the next `requestAnimationFrame` iteration.
void RequestRedraw();

/// @brief Return the available canvas dimensions in character columns/rows.
///
/// - WASM: queries the HTML canvas element size divided by the glyph size.
/// - Native: queries the terminal via `TIOCGWINSZ` (fallback: 80×24).
///
/// @return {columns, rows}
std::pair<int, int> GetCanvasDimensions();

/// @brief Persist a string value associated with @p key.
///
/// - WASM: uses `localStorage`.
/// - Native: writes to `~/.config/badwolf/<key>.txt`.
void SaveState(const std::string& key, const std::string& value);

/// @brief Load a previously saved value, returning @p default_value if absent.
///
/// - WASM: reads from `localStorage`.
/// - Native: reads from `~/.config/badwolf/<key>.txt`.
std::string LoadState(const std::string& key,
                      const std::string& default_value = "");

}  // namespace ftxui::ui

#endif  // FTXUI_UI_WASM_BRIDGE_HPP
