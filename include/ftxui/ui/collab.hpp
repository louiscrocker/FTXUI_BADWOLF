// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_COLLAB_HPP
#define FTXUI_UI_COLLAB_HPP

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

/// @brief Represents a connected collaborator in a shared TUI session.
struct CollabPeer {
  std::string id;         ///< Unique peer ID (8 hex chars from /dev/urandom)
  std::string name;       ///< Display name
  Color cursor_color;     ///< Assigned color for cursor overlay
  int cursor_x = 0;       ///< Last known cursor column
  int cursor_y = 0;       ///< Last known cursor row
  bool is_active = true;  ///< Whether this peer is currently connected
};

/// @brief An event exchanged between collaborating peers.
struct CollabEvent {
  std::string peer_id;
  std::string peer_name;
  enum class Type {
    JOIN,         ///< Peer connected
    LEAVE,        ///< Peer disconnected
    CURSOR_MOVE,  ///< Cursor position changed
    KEY_PRESS,    ///< A key was pressed
    STATE_SYNC,   ///< Full shared state payload
  } type = Type::JOIN;

  int x = 0;
  int y = 0;
  Event ftxui_event = Event::Custom;  ///< The underlying FTXUI event
  std::string payload;                ///< Serialised state for STATE_SYNC
  int64_t timestamp_ms = 0;
};

// ── CollabServer ─────────────────────────────────────────────────────────────

/// @brief TCP server that accepts peer connections and broadcasts CollabEvents.
///
/// Each connecting client sends a JOIN event with its peer_id/name.  The
/// server forwards every subsequent event from any client to all other
/// connected clients.
///
/// @code
/// auto server = std::make_shared<CollabServer>(7777);
/// server->Start();
/// // …
/// server->Stop();
/// @endcode
class CollabServer {
 public:
  explicit CollabServer(int port = 7777);
  ~CollabServer();

  /// @brief Start accepting peer connections (non-blocking after call).
  void Start();

  /// @brief Stop the server and close all connections.
  void Stop();

  /// @brief True if the server is currently listening.
  bool IsRunning() const;

  /// @brief Snapshot of all currently-connected peers.
  std::vector<CollabPeer> GetPeers() const;

  /// @brief Number of connected peers.
  int PeerCount() const;

  /// @brief Broadcast an event to every connected peer.
  void Broadcast(const CollabEvent& event);

  /// @brief Returns a status Element suitable for embedding in a UI.
  Element StatusElement() const;

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

// ── CollabClient ─────────────────────────────────────────────────────────────

/// @brief TCP client that connects to a CollabServer for shared TUI sessions.
///
/// @code
/// auto client = std::make_shared<CollabClient>("127.0.0.1", 7777, "Alice");
/// if (client->Connect()) {
///     client->OnRemoteEvent([](CollabEvent ev){ /* handle */ });
/// }
/// @endcode
class CollabClient {
 public:
  CollabClient(std::string host, int port, std::string peer_name);
  ~CollabClient();

  /// @brief Attempt a synchronous TCP connection.  Returns false on failure.
  bool Connect();

  /// @brief Disconnect from the server.
  void Disconnect();

  /// @brief True if currently connected.
  bool IsConnected() const;

  /// @brief The local peer's info (id, name, color).
  const CollabPeer& LocalPeer() const;

  /// @brief Snapshot of all remote peers known to this client.
  std::vector<CollabPeer> GetRemotePeers() const;

  /// @brief Send a local event to the server for broadcasting.
  void SendEvent(const CollabEvent& event);

  /// @brief Register a callback invoked for every remote event received.
  /// @return An opaque subscription id for use with RemoveOnRemoteEvent().
  int OnRemoteEvent(std::function<void(CollabEvent)> cb);

  /// @brief Unregister a previously registered remote-event callback.
  void RemoveOnRemoteEvent(int id);

  /// @brief Generate an 8-hex-character peer ID from /dev/urandom.
  static std::string GeneratePeerId();

  /// @brief Map an integer index to one of six distinct peer colors.
  static Color AssignColor(int index);

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

// ── Higher-level components
// ───────────────────────────────────────────────────

/// @brief Wraps any Component to broadcast key events and render peer cursors.
///
/// Every key-press is forwarded to the CollabServer.  Peer cursor positions
/// are overlaid as colored "▌" glyphs on the rendered output.
Component WithCollabSession(Component inner,
                            std::shared_ptr<CollabClient> client);

/// @brief Peer-list sidebar showing all connected peers with colored dots.
Component CollabPeerList(std::shared_ptr<CollabClient> client);
/// @overload Shows peers from the server's perspective.
Component CollabPeerList(std::shared_ptr<CollabServer> server);

/// @brief A shared text editor.  All peers can type and see changes live.
///
/// Uses STATE_SYNC events to propagate the full text on each key-press.
Component CollabNotepad(std::shared_ptr<CollabClient> client);

/// @brief Status bar Element: "● Peer1  ● Peer2  [COLLAB:port]"
Element CollabStatusBar(std::shared_ptr<CollabClient> client);
/// @overload Server-side status bar showing peer count.
Element CollabStatusBar(std::shared_ptr<CollabServer> server);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_COLLAB_HPP
