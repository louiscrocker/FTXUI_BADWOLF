// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file webgl_test.cpp
/// Tests for WebGLRenderer and WasmBridge — all tests exercise the native
/// stub path so they compile and run on every platform.

#include <string>
#include <utility>
#include <vector>

#include "ftxui/ui/wasm_bridge.hpp"
#include "ftxui/ui/webgl_renderer.hpp"
#include "gtest/gtest.h"

using namespace ftxui::ui;

// ── 1. IsAvailable returns false on native
// ────────────────────────────────────

TEST(WebGLRenderer, IsAvailableReturnsFalseOnNative) {
  EXPECT_FALSE(WebGLRenderer::IsAvailable());
}

// ── 2. Init returns false on native ──────────────────────────────────────────

TEST(WebGLRenderer, InitReturnsFalseOnNative) {
  EXPECT_FALSE(WebGLRenderer::Init("badwolf-canvas"));
}

// ── 3. GetStats returns zero stats on native ─────────────────────────────────

TEST(WebGLRenderer, GetStatsReturnsZeroOnNative) {
  auto stats = WebGLRenderer::GetStats();
  EXPECT_EQ(stats.quads_rendered, 0);
  EXPECT_EQ(stats.draw_calls, 0);
  EXPECT_DOUBLE_EQ(stats.frame_ms, 0.0);
}

// ── 4. Clear does not crash on native ────────────────────────────────────────

TEST(WebGLRenderer, ClearDoesNotCrash) {
  EXPECT_NO_FATAL_FAILURE(WebGLRenderer::Clear(0, 0, 0));
  EXPECT_NO_FATAL_FAILURE(WebGLRenderer::Clear(255, 128, 0));
}

// ── 5. Present does not crash on native ──────────────────────────────────────

TEST(WebGLRenderer, PresentDoesNotCrash) {
  EXPECT_NO_FATAL_FAILURE(WebGLRenderer::Present());
}

// ── 6. RenderBrailleCanvas does not crash with empty data ────────────────────

TEST(WebGLRenderer, RenderBrailleCanvasEmptyDoesNotCrash) {
  std::vector<std::vector<bool>> dots;
  EXPECT_NO_FATAL_FAILURE(WebGLRenderer::RenderBrailleCanvas(dots, 0, 0));
}

// ── 7. RenderBrailleCanvas does not crash with 10×10 data ────────────────────

TEST(WebGLRenderer, RenderBrailleCanvas10x10DoesNotCrash) {
  std::vector<std::vector<bool>> dots(10, std::vector<bool>(10, true));
  EXPECT_NO_FATAL_FAILURE(
      WebGLRenderer::RenderBrailleCanvas(dots, 10, 10, 0, 255, 0));
}

// ── 8. RenderScreen does not crash with empty lines
// ───────────────────────────

TEST(WebGLRenderer, RenderScreenEmptyDoesNotCrash) {
  std::vector<std::string> lines;
  std::vector<std::vector<uint32_t>> colors;
  EXPECT_NO_FATAL_FAILURE(WebGLRenderer::RenderScreen(0, 0, lines, colors));
}

// ── 9. WebGLScope constructs and destructs without crash ─────────────────────

TEST(WebGLRenderer, WebGLScopeLifetime) {
  EXPECT_NO_FATAL_FAILURE({
    WebGLScope scope("badwolf-canvas");
    // ok must be false on native (no WebGL context available).
    EXPECT_FALSE(scope.ok);
  });
}

// ── 10. WasmBridge::GetCanvasDimensions returns positive integers on native ──

TEST(WasmBridge, GetCanvasDimensionsPositiveOnNative) {
  auto [cols, rows] = ftxui::ui::GetCanvasDimensions();
  EXPECT_GT(cols, 0);
  EXPECT_GT(rows, 0);
}
