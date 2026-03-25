// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_WEBGL_RENDERER_HPP
#define FTXUI_UI_WEBGL_RENDERER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace ftxui::ui {

/// @brief WebGL renderer — only active when compiled with Emscripten.
///
/// On native builds every method is a no-op stub so code compiles everywhere
/// without any WebGL or Emscripten headers being pulled in.
///
/// @code
/// if (WebGLRenderer::Init("badwolf-canvas")) {
///   WebGLRenderer::Clear();
///   WebGLRenderer::RenderBrailleCanvas(dots, w, h);
///   WebGLRenderer::Present();
/// }
/// @endcode
class WebGLRenderer {
 public:
  /// @brief Initialize a WebGL context from an HTML canvas element id.
  /// @return true when a context was successfully created (WASM only).
  static bool Init(const std::string& canvas_id = "badwolf-canvas");

  /// @brief Returns true when WebGL is active (Emscripten + WebGL available).
  static bool IsAvailable();

  /// @brief Render a braille dot grid to WebGL.
  ///
  /// Each @p dots[row][col] == true entry is drawn as a tiny colored quad.
  /// Falls back to no-op when not available.
  static void RenderBrailleCanvas(const std::vector<std::vector<bool>>& dots,
                                  int width, int height,
                                  uint8_t r = 0, uint8_t g = 255,
                                  uint8_t b = 0);

  /// @brief Render a full text buffer as monospace glyph quads.
  ///
  /// @p lines[row] contains the UTF-8 text for that row.
  /// @p fg_colors[row][col] contains the packed RGBA foreground colour.
  static void RenderScreen(int cols, int rows,
                            const std::vector<std::string>& lines,
                            const std::vector<std::vector<uint32_t>>& fg_colors);

  /// @brief Clear the WebGL canvas with a solid colour.
  static void Clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);

  /// @brief Commit the current frame (swap buffers / commit).
  static void Present();

  /// @brief Release the WebGL context and all GPU resources.
  static void Shutdown();

  /// @brief Per-frame rendering statistics for debug overlays.
  struct Stats {
    int quads_rendered = 0;
    int draw_calls = 0;
    double frame_ms = 0.0;
  };

  /// @brief Return render statistics from the most recent frame.
  static Stats GetStats();
};

// ---------------------------------------------------------------------------
// WebGLScope
// ---------------------------------------------------------------------------

/// @brief RAII guard that initialises WebGL on construction and shuts it down
///        on destruction.
///
/// @code
/// {
///   WebGLScope gl("my-canvas");
///   if (gl.ok) { /* render … */ }
/// }  // Shutdown() called here
/// @endcode
struct WebGLScope {
  explicit WebGLScope(const std::string& canvas_id = "badwolf-canvas");
  ~WebGLScope();

  /// true when the WebGL context was successfully created.
  bool ok;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_WEBGL_RENDERER_HPP
