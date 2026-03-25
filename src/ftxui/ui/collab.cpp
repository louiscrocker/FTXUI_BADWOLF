// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/collab.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace ftxui::ui {

// ── Wire protocol helpers (identical framing to network_reactive.cpp)
// ─────────────── Messages: [4-byte LE length][payload bytes]

namespace {

bool SendMessage(int fd, const std::string& payload) {
  const uint32_t len = static_cast<uint32_t>(payload.size());
  uint8_t hdr[4];
  hdr[0] = static_cast<uint8_t>(len & 0xFF);
  hdr[1] = static_cast<uint8_t>((len >> 8) & 0xFF);
  hdr[2] = static_cast<uint8_t>((len >> 16) & 0xFF);
  hdr[3] = static_cast<uint8_t>((len >> 24) & 0xFF);

  if (::send(fd, hdr, 4, MSG_NOSIGNAL) != 4) {
    return false;
  }
  if (len == 0) {
    return true;
  }
  ssize_t sent = 0;
  while (sent < static_cast<ssize_t>(len)) {
    ssize_t n =
        ::send(fd, payload.data() + sent,
               static_cast<size_t>(len) - static_cast<size_t>(sent),
               MSG_NOSIGNAL);
    if (n <= 0) {
      return false;
    }
    sent += n;
  }
  return true;
}

bool RecvMessage(int fd, std::string& out) {
  uint8_t hdr[4];
  ssize_t got = 0;
  while (got < 4) {
    ssize_t n = ::recv(fd, hdr + got, static_cast<size_t>(4 - got), 0);
    if (n <= 0) {
      return false;
    }
    got += n;
  }
  const uint32_t len = static_cast<uint32_t>(hdr[0]) |
                       (static_cast<uint32_t>(hdr[1]) << 8) |
                       (static_cast<uint32_t>(hdr[2]) << 16) |
                       (static_cast<uint32_t>(hdr[3]) << 24);
  if (len == 0) {
    out.clear();
    return true;
  }
  if (len > 64 * 1024 * 1024) {
    return false;
  }
  out.resize(len);
  ssize_t received = 0;
  while (received < static_cast<ssize_t>(len)) {
    ssize_t n =
        ::recv(fd, out.data() + received,
               static_cast<size_t>(len) - static_cast<size_t>(received), 0);
    if (n <= 0) {
      return false;
    }
    received += n;
  }
  return true;
}

// ── Simple JSON helpers ───────────────────────────────────────────────────────

int64_t NowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}

std::string JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

// Decode \\n → \n, \\\" → " etc. (minimal unescape)
std::string JsonUnescape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char next = s[i + 1];
      switch (next) {
        case '"':
          out += '"';
          ++i;
          break;
        case '\\':
          out += '\\';
          ++i;
          break;
        case 'n':
          out += '\n';
          ++i;
          break;
        case 'r':
          out += '\r';
          ++i;
          break;
        case 't':
          out += '\t';
          ++i;
          break;
        default:
          out += s[i];
      }
    } else {
      out += s[i];
    }
  }
  return out;
}

std::string GetJsonStr(const std::string& json, const std::string& key) {
  std::string search = "\"" + key + "\":\"";
  auto pos = json.find(search);
  if (pos == std::string::npos) {
    return {};
  }
  pos += search.size();
  std::string raw;
  while (pos < json.size()) {
    if (json[pos] == '\\' && pos + 1 < json.size()) {
      raw += json[pos];
      raw += json[pos + 1];
      pos += 2;
    } else if (json[pos] == '"') {
      break;
    } else {
      raw += json[pos++];
    }
  }
  return JsonUnescape(raw);
}

int64_t GetJsonInt(const std::string& json, const std::string& key) {
  std::string search = "\"" + key + "\":";
  auto pos = json.find(search);
  if (pos == std::string::npos) {
    return 0;
  }
  pos += search.size();
  // skip whitespace
  while (pos < json.size() && json[pos] == ' ') {
    ++pos;
  }
  bool neg = false;
  if (pos < json.size() && json[pos] == '-') {
    neg = true;
    ++pos;
  }
  int64_t val = 0;
  while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
    val = val * 10 + (json[pos++] - '0');
  }
  return neg ? -val : val;
}

std::string TypeToStr(CollabEvent::Type t) {
  switch (t) {
    case CollabEvent::Type::JOIN:
      return "JOIN";
    case CollabEvent::Type::LEAVE:
      return "LEAVE";
    case CollabEvent::Type::CURSOR_MOVE:
      return "CURSOR_MOVE";
    case CollabEvent::Type::KEY_PRESS:
      return "KEY_PRESS";
    case CollabEvent::Type::STATE_SYNC:
      return "STATE_SYNC";
  }
  return "JOIN";
}

CollabEvent::Type StrToType(const std::string& s) {
  if (s == "LEAVE") {
    return CollabEvent::Type::LEAVE;
  }
  if (s == "CURSOR_MOVE") {
    return CollabEvent::Type::CURSOR_MOVE;
  }
  if (s == "KEY_PRESS") {
    return CollabEvent::Type::KEY_PRESS;
  }
  if (s == "STATE_SYNC") {
    return CollabEvent::Type::STATE_SYNC;
  }
  return CollabEvent::Type::JOIN;
}

std::string EventToJson(const CollabEvent& ev) {
  return "{\"peer_id\":\"" + JsonEscape(ev.peer_id) +
         "\",\"peer_name\":\"" + JsonEscape(ev.peer_name) +
         "\",\"type\":\"" + TypeToStr(ev.type) +
         "\",\"x\":" + std::to_string(ev.x) +
         ",\"y\":" + std::to_string(ev.y) +
         ",\"payload\":\"" + JsonEscape(ev.payload) +
         "\",\"ts\":" + std::to_string(ev.timestamp_ms) + "}";
}

CollabEvent JsonToEvent(const std::string& json) {
  CollabEvent ev;
  ev.peer_id = GetJsonStr(json, "peer_id");
  ev.peer_name = GetJsonStr(json, "peer_name");
  ev.type = StrToType(GetJsonStr(json, "type"));
  ev.x = static_cast<int>(GetJsonInt(json, "x"));
  ev.y = static_cast<int>(GetJsonInt(json, "y"));
  ev.payload = GetJsonStr(json, "payload");
  ev.timestamp_ms = GetJsonInt(json, "ts");

  if (ev.type == CollabEvent::Type::KEY_PRESS && !ev.payload.empty()) {
    ev.ftxui_event = Event::Character(ev.payload);
  }
  return ev;
}

}  // namespace

// ── CollabServer::Impl ───────────────────────────────────────────────────────

struct CollabServer::Impl {
  int port_ = 7777;
  int server_fd_ = -1;
  std::atomic<bool> running_{false};

  std::thread accept_thread_;

  mutable std::mutex peers_mutex_;
  std::map<std::string, CollabPeer> peers_;
  std::map<std::string, int> peer_fds_;

  mutable std::mutex threads_mutex_;
  std::vector<std::thread> client_threads_;

  // Broadcast a raw JSON payload to all clients except one (pass "" to
  // broadcast to all).
  void BroadcastExcept(const std::string& payload,
                       const std::string& exclude_id) {
    std::vector<int> fds_to_send;
    std::vector<std::string> dead_ids;
    {
      std::lock_guard<std::mutex> lk(peers_mutex_);
      for (auto& [id, fd] : peer_fds_) {
        if (id != exclude_id) {
          fds_to_send.push_back(fd);
        }
      }
    }
    for (int fd : fds_to_send) {
      if (!SendMessage(fd, payload)) {
        // Find and record dead peer id
        std::lock_guard<std::mutex> lk(peers_mutex_);
        for (auto& [id, pfd] : peer_fds_) {
          if (pfd == fd) {
            dead_ids.push_back(id);
            break;
          }
        }
      }
    }
    if (!dead_ids.empty()) {
      std::lock_guard<std::mutex> lk(peers_mutex_);
      for (auto& id : dead_ids) {
        peer_fds_.erase(id);
        peers_.erase(id);
      }
    }
  }

  void HandleClient(int client_fd) {
    std::string peer_id;
    // First message must be a JOIN event
    {
      std::string msg;
      if (!RecvMessage(client_fd, msg)) {
        ::shutdown(client_fd, SHUT_RDWR);
        ::close(client_fd);
        return;
      }
      CollabEvent ev = JsonToEvent(msg);
      peer_id = ev.peer_id;

      // Assign a color based on the number of existing peers
      int color_idx = 0;
      {
        std::lock_guard<std::mutex> lk(peers_mutex_);
        color_idx = static_cast<int>(peers_.size());
        CollabPeer peer;
        peer.id = ev.peer_id;
        peer.name = ev.peer_name;
        peer.cursor_color = CollabClient::AssignColor(color_idx);
        peer.is_active = true;
        peers_[peer_id] = peer;
        peer_fds_[peer_id] = client_fd;
      }

      // Broadcast JOIN to all OTHER clients
      CollabEvent join_broadcast = ev;
      join_broadcast.type = CollabEvent::Type::JOIN;
      BroadcastExcept(EventToJson(join_broadcast), peer_id);
    }

    // Event forwarding loop
    while (running_.load()) {
      // Use select with timeout so we can poll running_
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(client_fd, &fds);
      struct timeval tv{};
      tv.tv_sec = 0;
      tv.tv_usec = 100000;  // 100 ms
      int sel = ::select(client_fd + 1, &fds, nullptr, nullptr, &tv);
      if (sel < 0) {
        break;
      }
      if (sel == 0) {
        continue;
      }

      std::string msg;
      if (!RecvMessage(client_fd, msg)) {
        break;
      }
      // Forward to all other clients verbatim
      BroadcastExcept(msg, peer_id);
    }

    // Cleanup: remove peer and notify others
    CollabEvent leave_ev;
    leave_ev.type = CollabEvent::Type::LEAVE;
    leave_ev.timestamp_ms = NowMs();
    {
      std::lock_guard<std::mutex> lk(peers_mutex_);
      auto it = peers_.find(peer_id);
      if (it != peers_.end()) {
        leave_ev.peer_id = it->second.id;
        leave_ev.peer_name = it->second.name;
        peers_.erase(it);
      }
      peer_fds_.erase(peer_id);
    }
    ::shutdown(client_fd, SHUT_RDWR);
    ::close(client_fd);

    if (!leave_ev.peer_id.empty()) {
      BroadcastExcept(EventToJson(leave_ev), "");
    }
  }

  void AcceptLoop() {
    while (running_.load()) {
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(server_fd_, &fds);
      struct timeval tv{};
      tv.tv_sec = 0;
      tv.tv_usec = 100000;

      int sel = ::select(server_fd_ + 1, &fds, nullptr, nullptr, &tv);
      if (sel <= 0) {
        continue;
      }

      sockaddr_in client_addr{};
      socklen_t client_len = sizeof(client_addr);
      int client_fd = ::accept(
          server_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
      if (client_fd < 0) {
        if (!running_.load()) {
          break;
        }
        continue;
      }

      if (!running_.load()) {
        ::close(client_fd);
        break;
      }

      // Spawn per-client handler thread
      std::lock_guard<std::mutex> lk(threads_mutex_);
      client_threads_.emplace_back([this, client_fd]() {
        HandleClient(client_fd);
      });
    }
  }

  void DoStop() {
    if (!running_.exchange(false)) {
      return;
    }

    // Close server socket to unblock accept()
    if (server_fd_ >= 0) {
      ::shutdown(server_fd_, SHUT_RDWR);
      ::close(server_fd_);
      server_fd_ = -1;
    }

    // Wait for accept thread
    if (accept_thread_.joinable()) {
      accept_thread_.join();
    }

    // Client threads will exit within 100 ms (select timeout) once
    // running_ == false.  We just need to join them.
    std::vector<std::thread> to_join;
    {
      std::lock_guard<std::mutex> lk(threads_mutex_);
      to_join = std::move(client_threads_);
    }
    for (auto& t : to_join) {
      if (t.joinable()) {
        t.join();
      }
    }

    // Close any remaining fds
    std::lock_guard<std::mutex> lk(peers_mutex_);
    for (auto& [id, fd] : peer_fds_) {
      ::shutdown(fd, SHUT_RDWR);
      ::close(fd);
    }
    peer_fds_.clear();
    peers_.clear();
  }
};

// ── CollabServer public API ───────────────────────────────────────────────────

CollabServer::CollabServer(int port) : impl_(std::make_shared<Impl>()) {
  impl_->port_ = port;
}

CollabServer::~CollabServer() {
  impl_->DoStop();
}

void CollabServer::Start() {
  if (impl_->running_.load()) {
    return;
  }

  impl_->server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (impl_->server_fd_ < 0) {
    return;
  }

  int opt = 1;
  ::setsockopt(impl_->server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(impl_->port_));

  if (::bind(impl_->server_fd_, reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) < 0) {
    ::close(impl_->server_fd_);
    impl_->server_fd_ = -1;
    return;
  }

  if (::listen(impl_->server_fd_, 16) < 0) {
    ::close(impl_->server_fd_);
    impl_->server_fd_ = -1;
    return;
  }

  impl_->running_.store(true);
  impl_->accept_thread_ =
      std::thread([impl = impl_]() { impl->AcceptLoop(); });
}

void CollabServer::Stop() {
  impl_->DoStop();
}

bool CollabServer::IsRunning() const {
  return impl_->running_.load();
}

std::vector<CollabPeer> CollabServer::GetPeers() const {
  std::lock_guard<std::mutex> lk(impl_->peers_mutex_);
  std::vector<CollabPeer> out;
  out.reserve(impl_->peers_.size());
  for (auto& [id, p] : impl_->peers_) {
    out.push_back(p);
  }
  return out;
}

int CollabServer::PeerCount() const {
  std::lock_guard<std::mutex> lk(impl_->peers_mutex_);
  return static_cast<int>(impl_->peers_.size());
}

void CollabServer::Broadcast(const CollabEvent& event) {
  impl_->BroadcastExcept(EventToJson(event), "");
}

Element CollabServer::StatusElement() const {
  auto peers = GetPeers();
  Elements items;
  items.push_back(
      hbox({text(" [SERVER] port "),
            text(std::to_string(impl_->port_)) | bold | color(Color::Yellow),
            text("  peers: "),
            text(std::to_string(static_cast<int>(peers.size()))) | bold |
                color(Color::GreenLight)}) |
      color(Color::CyanLight));
  for (auto& p : peers) {
    items.push_back(hbox({text("  ● ") | color(p.cursor_color),
                          text(p.name) | color(Color::White)}));
  }
  return vbox(items);
}

// ── CollabClient::Impl ───────────────────────────────────────────────────────

struct CollabClient::Impl {
  std::string host_;
  int port_ = 7777;

  CollabPeer local_peer_;
  int sock_fd_ = -1;
  std::atomic<bool> connected_{false};
  std::thread recv_thread_;

  mutable std::mutex mutex_;
  int next_id_ = 0;
  std::map<int, std::function<void(CollabEvent)>> callbacks_;
  std::map<std::string, CollabPeer> remote_peers_;

  void FireCallbacks(const CollabEvent& ev) {
    std::map<int, std::function<void(CollabEvent)>> cbs;
    {
      std::lock_guard<std::mutex> lk(mutex_);
      cbs = callbacks_;
    }
    for (auto& [id, cb] : cbs) {
      cb(ev);
    }
  }

  void RecvLoop() {
    while (connected_.load()) {
      // select() with timeout so we can check connected_
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(sock_fd_, &fds);
      struct timeval tv{};
      tv.tv_sec = 0;
      tv.tv_usec = 100000;
      int sel = ::select(sock_fd_ + 1, &fds, nullptr, nullptr, &tv);
      if (sel < 0) {
        break;
      }
      if (sel == 0) {
        continue;
      }

      std::string msg;
      if (!RecvMessage(sock_fd_, msg)) {
        break;
      }
      CollabEvent ev = JsonToEvent(msg);

      // Update remote_peers_ based on event type
      {
        std::lock_guard<std::mutex> lk(mutex_);
        if (ev.type == CollabEvent::Type::JOIN ||
            ev.type == CollabEvent::Type::CURSOR_MOVE ||
            ev.type == CollabEvent::Type::KEY_PRESS) {
          CollabPeer& peer = remote_peers_[ev.peer_id];
          peer.id = ev.peer_id;
          peer.name = ev.peer_name;
          if (ev.type == CollabEvent::Type::JOIN) {
            // Assign color based on peer count
            int idx = static_cast<int>(remote_peers_.size()) - 1;
            peer.cursor_color = CollabClient::AssignColor(idx);
            peer.is_active = true;
          }
          if (ev.type == CollabEvent::Type::CURSOR_MOVE) {
            peer.cursor_x = ev.x;
            peer.cursor_y = ev.y;
          }
        } else if (ev.type == CollabEvent::Type::LEAVE) {
          remote_peers_.erase(ev.peer_id);
        }
      }

      FireCallbacks(ev);
    }
    connected_.store(false);
  }

  void DoDisconnect() {
    connected_.store(false);
    if (sock_fd_ >= 0) {
      ::shutdown(sock_fd_, SHUT_RDWR);
      ::close(sock_fd_);
      sock_fd_ = -1;
    }
    if (recv_thread_.joinable()) {
      recv_thread_.join();
    }
  }
};

// ── CollabClient public API ───────────────────────────────────────────────────

CollabClient::CollabClient(std::string host, int port, std::string peer_name)
    : impl_(std::make_shared<Impl>()) {
  impl_->host_ = std::move(host);
  impl_->port_ = port;
  impl_->local_peer_.id = GeneratePeerId();
  impl_->local_peer_.name = std::move(peer_name);
  impl_->local_peer_.cursor_color = Color::White;
  impl_->local_peer_.is_active = true;
}

CollabClient::~CollabClient() {
  impl_->DoDisconnect();
}

bool CollabClient::Connect() {
  if (impl_->connected_.load()) {
    return true;
  }

  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(impl_->port_));

  // Resolve host
  struct addrinfo hints{};
  struct addrinfo* res = nullptr;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  bool resolved = false;
  if (::getaddrinfo(impl_->host_.c_str(), nullptr, &hints, &res) == 0 && res) {
    const auto* sin = reinterpret_cast<const sockaddr_in*>(res->ai_addr);
    addr.sin_addr = sin->sin_addr;
    resolved = true;
    ::freeaddrinfo(res);
  }

  if (!resolved ||
      ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return false;
  }

  impl_->sock_fd_ = fd;
  impl_->connected_.store(true);

  // Send JOIN event
  CollabEvent join_ev;
  join_ev.peer_id = impl_->local_peer_.id;
  join_ev.peer_name = impl_->local_peer_.name;
  join_ev.type = CollabEvent::Type::JOIN;
  join_ev.timestamp_ms = NowMs();

  if (!SendMessage(fd, EventToJson(join_ev))) {
    impl_->connected_.store(false);
    ::close(fd);
    impl_->sock_fd_ = -1;
    return false;
  }

  // Start receive thread
  impl_->recv_thread_ =
      std::thread([impl = impl_]() { impl->RecvLoop(); });

  return true;
}

void CollabClient::Disconnect() {
  impl_->DoDisconnect();
}

bool CollabClient::IsConnected() const {
  return impl_->connected_.load();
}

const CollabPeer& CollabClient::LocalPeer() const {
  return impl_->local_peer_;
}

std::vector<CollabPeer> CollabClient::GetRemotePeers() const {
  std::lock_guard<std::mutex> lk(impl_->mutex_);
  std::vector<CollabPeer> out;
  out.reserve(impl_->remote_peers_.size());
  for (auto& [id, p] : impl_->remote_peers_) {
    out.push_back(p);
  }
  return out;
}

void CollabClient::SendEvent(const CollabEvent& event) {
  if (!impl_->connected_.load() || impl_->sock_fd_ < 0) {
    return;
  }
  SendMessage(impl_->sock_fd_, EventToJson(event));
}

int CollabClient::OnRemoteEvent(std::function<void(CollabEvent)> cb) {
  std::lock_guard<std::mutex> lk(impl_->mutex_);
  int id = impl_->next_id_++;
  impl_->callbacks_[id] = std::move(cb);
  return id;
}

void CollabClient::RemoveOnRemoteEvent(int id) {
  std::lock_guard<std::mutex> lk(impl_->mutex_);
  impl_->callbacks_.erase(id);
}

// static
std::string CollabClient::GeneratePeerId() {
  static const char hex[] = "0123456789abcdef";
  std::string id(8, '0');
  int fd = ::open("/dev/urandom", O_RDONLY);
  if (fd >= 0) {
    uint8_t bytes[4];
    if (::read(fd, bytes, 4) == 4) {
      for (int i = 0; i < 4; ++i) {
        id[i * 2] = hex[(bytes[i] >> 4) & 0xF];
        id[i * 2 + 1] = hex[bytes[i] & 0xF];
      }
    }
    ::close(fd);
  }
  return id;
}

// static
Color CollabClient::AssignColor(int index) {
  static const Color kColors[6] = {Color::Red,     Color::Green,
                                   Color::Yellow,   Color::Blue,
                                   Color::Magenta,  Color::Cyan};
  return kColors[((index % 6) + 6) % 6];
}

// ── Higher-level components ───────────────────────────────────────────────────

// Helper: draw a peer cursor marker at (x, y) in a dbox overlay.
namespace {
Element PeerCursorOverlay(int x, int y, Color c, const std::string& label) {
  // Build y-row spacer followed by x-column spacer + glyph.
  Elements rows;
  for (int i = 0; i < y; ++i) {
    rows.push_back(text(""));
  }
  rows.push_back(
      hbox({text(std::string(static_cast<size_t>(x > 0 ? x : 0), ' ')),
            text("▌") | color(c),
            text(label) | color(c)}));
  return vbox(rows);
}
}  // namespace

Component WithCollabSession(Component inner,
                             std::shared_ptr<CollabClient> client) {
  struct State {
    std::map<std::string, CollabPeer> cursors;
    std::mutex mutex;
    int cb_id = -1;
  };
  auto state = std::make_shared<State>();

  state->cb_id = client->OnRemoteEvent([state](CollabEvent ev) {
    {
      std::lock_guard<std::mutex> lk(state->mutex);
      if (ev.type == CollabEvent::Type::JOIN ||
          ev.type == CollabEvent::Type::KEY_PRESS ||
          ev.type == CollabEvent::Type::CURSOR_MOVE) {
        CollabPeer& p = state->cursors[ev.peer_id];
        p.id = ev.peer_id;
        p.name = ev.peer_name;
        if (ev.type == CollabEvent::Type::CURSOR_MOVE) {
          p.cursor_x = ev.x;
          p.cursor_y = ev.y;
        }
      } else if (ev.type == CollabEvent::Type::LEAVE) {
        state->cursors.erase(ev.peer_id);
      }
    }
    if (App* a = App::Active()) {
      a->Post([] {});
    }
  });

  // Wrap inner: custom renderer that overlays peer cursors.
  auto decorated = Renderer(inner, [inner, client, state]() -> Element {
    Elements layers;
    layers.push_back(inner->Render());
    std::lock_guard<std::mutex> lk(state->mutex);
    for (auto& [id, peer] : state->cursors) {
      layers.push_back(PeerCursorOverlay(peer.cursor_x, peer.cursor_y,
                                          peer.cursor_color, peer.name));
    }
    if (layers.size() == 1) {
      return layers[0];
    }
    return dbox(layers);
  });

  // Intercept events: broadcast to server then pass through.
  auto with_events = CatchEvent(decorated, [client](Event e) -> bool {
    if (!client->IsConnected()) {
      return false;
    }
    CollabEvent ev;
    ev.peer_id = client->LocalPeer().id;
    ev.peer_name = client->LocalPeer().name;
    ev.timestamp_ms = NowMs();
    if (e.is_character()) {
      ev.type = CollabEvent::Type::KEY_PRESS;
      ev.payload = e.character();
      ev.ftxui_event = e;
      client->SendEvent(ev);
    }
    return false;  // never consume — inner component handles normally
  });

  return with_events;
}

Component CollabPeerList(std::shared_ptr<CollabClient> client) {
  return Renderer([client]() -> Element {
    auto peers = client->GetRemotePeers();
    const auto& local = client->LocalPeer();

    Elements items;
    items.push_back(text(" Peers") | bold | color(Color::CyanLight));
    items.push_back(separator());
    items.push_back(
        hbox({text("● ") | color(Color::White),
              text(local.name + " (you)") | color(Color::GrayLight)}));

    for (auto& p : peers) {
      items.push_back(
          hbox({text("● ") | color(p.cursor_color),
                text(p.name) | color(Color::White)}));
    }
    if (peers.empty()) {
      items.push_back(
          text(" (no peers yet)") | color(Color::GrayDark) | italic);
    }
    items.push_back(separator());
    std::string status = client->IsConnected() ? "CONNECTED" : "DISCONNECTED";
    Color sc = client->IsConnected() ? Color::GreenLight : Color::RedLight;
    items.push_back(hbox({text(" "), text(status) | bold | color(sc)}));
    return vbox(items) | border;
  });
}

Component CollabPeerList(std::shared_ptr<CollabServer> server) {
  return Renderer([server]() -> Element {
    auto peers = server->GetPeers();
    Elements items;
    items.push_back(text(" Server Peers") | bold | color(Color::CyanLight));
    items.push_back(separator());
    for (auto& p : peers) {
      items.push_back(
          hbox({text("● ") | color(p.cursor_color),
                text(p.name) | color(Color::White)}));
    }
    if (peers.empty()) {
      items.push_back(
          text(" (no peers connected)") | color(Color::GrayDark) | italic);
    }
    items.push_back(separator());
    items.push_back(
        hbox({text(" Listening on port "),
              text(std::to_string(server->IsRunning() ? 1 : 0)) |
                  color(Color::Yellow)}));
    return vbox(items) | border;
  });
}

Component CollabNotepad(std::shared_ptr<CollabClient> client) {
  auto content = std::make_shared<std::string>();
  auto mutex = std::make_shared<std::mutex>();

  // Listen for remote STATE_SYNC to update local text
  client->OnRemoteEvent([content, mutex](CollabEvent ev) {
    if (ev.type == CollabEvent::Type::STATE_SYNC) {
      {
        std::lock_guard<std::mutex> lk(*mutex);
        *content = ev.payload;
      }
      if (App* a = App::Active()) {
        a->Post([] {});
      }
    }
  });

  InputOption opt;
  opt.multiline = true;
  opt.on_change = [content, client, mutex]() {
    if (!client->IsConnected()) {
      return;
    }
    std::string current;
    {
      std::lock_guard<std::mutex> lk(*mutex);
      current = *content;
    }
    CollabEvent ev;
    ev.peer_id = client->LocalPeer().id;
    ev.peer_name = client->LocalPeer().name;
    ev.type = CollabEvent::Type::STATE_SYNC;
    ev.payload = current;
    ev.timestamp_ms = NowMs();
    client->SendEvent(ev);
  };

  auto input = Input(content.get(), opt);
  return Renderer(input, [input, content, client]() -> Element {
    return vbox({
               hbox({
                   text(" ✏  Shared Notepad") | bold | color(Color::CyanLight),
                   filler(),
                   text(client->IsConnected() ? " ● LIVE" : " ○ OFFLINE") |
                       color(client->IsConnected() ? Color::GreenLight
                                                   : Color::GrayDark),
                   text(" "),
               }),
               separator(),
               input->Render() | flex,
               separator(),
               text(" Ctrl+C to quit") | color(Color::GrayDark),
           }) |
           border | flex;
  });
}

Element CollabStatusBar(std::shared_ptr<CollabClient> client) {
  Elements items;
  const auto& local = client->LocalPeer();
  items.push_back(
      text(std::string(" [COLLAB:") + std::to_string(client->IsConnected()
                                                          ? 1
                                                          : 0) +
           "] ") |
      color(client->IsConnected() ? Color::GreenLight : Color::GrayDark));
  items.push_back(hbox({text("● ") | color(Color::White),
                        text(local.name + " (you)") | color(Color::GrayLight),
                        text("  ")}));
  for (auto& p : client->GetRemotePeers()) {
    items.push_back(
        hbox({text("● ") | color(p.cursor_color),
              text(p.name) | color(Color::White),
              text("  ")}));
  }
  return hbox(items);
}

Element CollabStatusBar(std::shared_ptr<CollabServer> server) {
  Elements items;
  items.push_back(text(" [SERVER] ") | bold |
                  color(server->IsRunning() ? Color::GreenLight
                                            : Color::GrayDark));
  for (auto& p : server->GetPeers()) {
    items.push_back(
        hbox({text("● ") | color(p.cursor_color),
              text(p.name) | color(Color::White),
              text("  ")}));
  }
  if (server->GetPeers().empty()) {
    items.push_back(text("(no peers)") | color(Color::GrayDark));
  }
  return hbox(items);
}

}  // namespace ftxui::ui
