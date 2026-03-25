// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_NETWORK_REACTIVE_HPP
#define FTXUI_UI_NETWORK_REACTIVE_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "ftxui/ui/reactive.hpp"

namespace ftxui::ui {

/// @brief TCP server that broadcasts Reactive<std::string> state to all
/// connected clients. The server holds the authoritative copy of the state.
///
/// Uses POSIX TCP sockets (macOS + Linux). No external dependencies.
/// Protocol: 4-byte little-endian length prefix followed by UTF-8 payload.
///
/// @code
/// auto state = std::make_shared<Reactive<std::string>>("initial");
/// auto server = NetworkReactiveServer::Create(state, 8765);
/// server->Start();
/// // Any state->Set(...) or state->Update(...) now broadcasts to clients.
/// @endcode
class NetworkReactiveServer {
 public:
  /// @brief Create a server bound to the given state and port.
  static std::shared_ptr<NetworkReactiveServer> Create(
      std::shared_ptr<Reactive<std::string>> state,
      uint16_t port);

  /// @brief Start accepting connections and broadcasting state.
  void Start();

  /// @brief Stop the server and close all connections.
  void Stop();

  /// @brief Number of currently connected clients.
  int ConnectedClients() const;

  /// @brief Port the server is listening on.
  uint16_t Port() const;

  ~NetworkReactiveServer();

 private:
  NetworkReactiveServer() = default;
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

/// @brief TCP client that mirrors a remote NetworkReactiveServer's state.
///
/// Automatically reconnects every 2 seconds if disconnected. State updates
/// arrive on a background thread and are applied to the local Reactive<string>.
///
/// @code
/// auto mirror = NetworkReactiveClient::Connect("127.0.0.1", 8765);
/// mirror->State()->OnChange([](const std::string& s){
///     // react to remote state
/// });
/// @endcode
class NetworkReactiveClient {
 public:
  /// @brief Connect to a server at host:port.
  static std::shared_ptr<NetworkReactiveClient> Connect(std::string host,
                                                        uint16_t port);

  /// @brief The mirrored reactive state (read-only from client side).
  std::shared_ptr<Reactive<std::string>> State() const;

  /// @brief True if currently connected to the server.
  bool Connected() const;

  /// @brief Disconnect and stop reconnection attempts.
  void Disconnect();

  ~NetworkReactiveClient();

 private:
  NetworkReactiveClient() = default;
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_NETWORK_REACTIVE_HPP
