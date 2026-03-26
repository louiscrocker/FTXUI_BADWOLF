// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_WEB_BRIDGE_HPP
#define FTXUI_UI_WEB_BRIDGE_HPP

#include <functional>
#include <memory>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"

namespace ftxui::ui {

/// @brief Configuration for the WebBridge server.
struct WebBridgeConfig {
  int port = 7681;  ///< HTTP + WS listening port
  std::string host = "0.0.0.0";
  int cols = 220;                 ///< Terminal width served to browser
  int rows = 50;                  ///< Terminal height served to browser
  int fps = 30;                   ///< Maximum render rate (frames per second)
  bool open_browser = false;      ///< Auto-open browser on start
  std::string title = "BadWolf";  ///< HTML page title
};

/// @brief Serve any BadWolf TUI component in a browser over HTTP + WebSocket.
///
/// The server renders the FTXUI component to ANSI escape sequences and streams
/// updates to a browser terminal emulator (xterm.js). Browser keyboard events
/// are converted to ftxui::Event and injected into the component.
///
/// **Usage:**
/// ```cpp
/// ftxui::ui::WebBridgeConfig cfg;
/// cfg.port = 7681;
/// ftxui::ui::WebBridge bridge(cfg);
/// bridge.Run(MyComponent());
/// ```
class WebBridge {
 public:
  explicit WebBridge(WebBridgeConfig cfg = {});
  ~WebBridge();

  /// @brief Run component in web bridge mode. Blocks until stopped.
  void Run(ftxui::Component root);

  /// @brief Start the server without blocking. Returns immediately.
  void Start(ftxui::Component root);

  /// @brief Stop the server and all connections.
  void Stop();

  /// @brief Return true if the server is currently running.
  bool IsRunning() const;

  /// @brief Return the actual listening port (may differ from cfg.port if
  ///        there was a conflict and an alternate port was chosen).
  int port() const;

  /// @brief Return the full URL, e.g. "http://localhost:7681".
  std::string url() const;

  /// @brief Inject an ftxui::Event directly (useful for testing).
  void InjectEvent(ftxui::Event e);

  // Non-copyable, non-movable.
  WebBridge(const WebBridge&) = delete;
  WebBridge& operator=(const WebBridge&) = delete;
  WebBridge(WebBridge&&) = delete;
  WebBridge& operator=(WebBridge&&) = delete;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// @brief Run a component in web mode if --web flag is present, otherwise run
///        it in the normal terminal fullscreen mode.
///
/// **Usage:**
/// ```cpp
/// int main(int argc, char** argv) {
///   ftxui::ui::RunWebOrTerminal(argc, argv, [] { return MyComponent(); });
/// }
/// ```
/// Launch with `--web` to serve in browser, or without for terminal mode.
void RunWebOrTerminal(int argc,
                      char** argv,
                      std::function<ftxui::Component()> factory,
                      WebBridgeConfig cfg = {});

}  // namespace ftxui::ui

#endif  // FTXUI_UI_WEB_BRIDGE_HPP
