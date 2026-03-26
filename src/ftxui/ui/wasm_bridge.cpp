// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file wasm_bridge.cpp
///
/// Two compile-time paths:
///   - __EMSCRIPTEN__  : browser-native integration via Emscripten APIs.
///   - otherwise       : native terminal implementation.

#include "ftxui/ui/wasm_bridge.hpp"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "ftxui/component/component.hpp"
#include "ftxui/ui/app.hpp"

namespace {

struct LoopState {
  std::function<ftxui::Component()> factory;
  ftxui::Component root;
};

static LoopState* g_loop = nullptr;

static void MainLoopStep(void* arg) {
  auto* state = static_cast<LoopState*>(arg);
  (void)state;
  // The FTXUI event loop ticks are driven by the browser's rAF; rendering is
  // handled by the FTXUI App internally when it was started with
  // emscripten_set_main_loop.  Nothing extra to do here.
}

}  // namespace

namespace ftxui::ui {

void RunApp(std::function<ftxui::Component()> app_factory) {
  g_loop = new LoopState{std::move(app_factory), nullptr};
  g_loop->root = g_loop->factory();
  // Delegate to the standard RunFullscreen which internally uses
  // emscripten_set_main_loop when built with Emscripten.
  RunFullscreen(g_loop->root);
}

void RequestRedraw() {
  // In the WASM path the browser rAF loop handles scheduling; a no-op here is
  // sufficient because the next frame will automatically re-render.
}

std::pair<int, int> GetCanvasDimensions() {
  int w = 80, h = 24;
  // Query the actual canvas pixel size and convert to character cells
  // (assume 8×16 px per cell as a reasonable default).
  EMSCRIPTEN_RESULT res =
      emscripten_get_canvas_element_size("#badwolf-canvas", &w, &h);
  if (res == EMSCRIPTEN_RESULT_SUCCESS && w > 0 && h > 0) {
    return {w / 8, h / 16};
  }
  return {80, 24};
}

void SaveState(const std::string& key, const std::string& value) {
  EM_ASM(
      {
        try {
          localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
        } catch (e) {
        }
      },
      key.c_str(), value.c_str());
}

std::string LoadState(const std::string& key,
                      const std::string& default_value) {
  char* raw = reinterpret_cast<char*>(EM_ASM_INT(
      {
        var key = UTF8ToString($0);
        var val = localStorage.getItem(key);
        if (val == = null) {
          return 0;
        }
        var len = lengthBytesUTF8(val) + 1;
        var buf = _malloc(len);
        stringToUTF8(val, buf, len);
        return buf;
      },
      key.c_str()));
  if (!raw) {
    return default_value;
  }
  std::string result(raw);
  free(raw);
  return result;
}

}  // namespace ftxui::ui

#else  // !__EMSCRIPTEN__ — native path

#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "ftxui/component/component.hpp"
#include "ftxui/ui/app.hpp"

namespace ftxui::ui {

void RunApp(std::function<ftxui::Component()> app_factory) {
  RunFullscreen(app_factory());
}

void RequestRedraw() {
  // On native builds the event loop is spinning; posting a custom event would
  // wake it up, but for simplicity we rely on the existing animation ticks.
}

std::pair<int, int> GetCanvasDimensions() {
#if defined(TIOCGWINSZ)
  struct winsize ws{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 &&
      ws.ws_row > 0) {
    return {static_cast<int>(ws.ws_col), static_cast<int>(ws.ws_row)};
  }
#endif
  return {80, 24};
}

namespace {
std::string StateFilePath(const std::string& key) {
  const char* home = std::getenv("HOME");
  if (!home) {
    home = ".";
  }
  return std::string(home) + "/.config/badwolf/" + key + ".txt";
}
}  // namespace

void SaveState(const std::string& key, const std::string& value) {
  const std::string path = StateFilePath(key);
  // Best-effort: create parent directories if they don't exist.
  std::system(("mkdir -p \"$(dirname '" + path + "')\" 2>/dev/null").c_str());
  std::ofstream f(path);
  if (f) {
    f << value;
  }
}

std::string LoadState(const std::string& key,
                      const std::string& default_value) {
  std::ifstream f(StateFilePath(key));
  if (!f) {
    return default_value;
  }
  std::string content((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
  return content.empty() ? default_value : content;
}

}  // namespace ftxui::ui

#endif  // __EMSCRIPTEN__
