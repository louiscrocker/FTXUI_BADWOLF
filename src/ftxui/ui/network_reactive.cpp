// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/network_reactive.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/ui/reactive.hpp"

namespace ftxui::ui {

// ── Wire protocol helpers ─────────────────────────────────────────────────────
// Messages are framed as: [4-byte LE length][payload bytes]

namespace {

/// Send a framed message on a socket. Returns false on error.
bool SendMessage(int fd, const std::string& payload) {
  const uint32_t len = static_cast<uint32_t>(payload.size());
  // Write length (little-endian)
  uint8_t hdr[4];
  hdr[0] = static_cast<uint8_t>(len & 0xFF);
  hdr[1] = static_cast<uint8_t>((len >> 8) & 0xFF);
  hdr[2] = static_cast<uint8_t>((len >> 16) & 0xFF);
  hdr[3] = static_cast<uint8_t>((len >> 24) & 0xFF);

  if (::send(fd, hdr, 4, MSG_NOSIGNAL) != 4) return false;
  if (len == 0) return true;
  ssize_t sent = 0;
  while (sent < static_cast<ssize_t>(len)) {
    ssize_t n = ::send(fd, payload.data() + sent,
                       static_cast<size_t>(len) - static_cast<size_t>(sent),
                       MSG_NOSIGNAL);
    if (n <= 0) return false;
    sent += n;
  }
  return true;
}

/// Receive a framed message from a socket. Returns false on error/disconnect.
bool RecvMessage(int fd, std::string& out) {
  uint8_t hdr[4];
  ssize_t got = 0;
  while (got < 4) {
    ssize_t n = ::recv(fd, hdr + got, static_cast<size_t>(4 - got), 0);
    if (n <= 0) return false;
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
  if (len > 64 * 1024 * 1024) return false;  // sanity limit: 64 MB

  out.resize(len);
  ssize_t received = 0;
  while (received < static_cast<ssize_t>(len)) {
    ssize_t n = ::recv(fd, out.data() + received,
                       static_cast<size_t>(len) - static_cast<size_t>(received),
                       0);
    if (n <= 0) return false;
    received += n;
  }
  return true;
}

}  // namespace

// ── NetworkReactiveServer::Impl ───────────────────────────────────────────────

struct NetworkReactiveServer::Impl {
  std::shared_ptr<Reactive<std::string>> state_;
  uint16_t port_ = 0;

  int server_fd_ = -1;
  std::atomic<bool> running_{false};
  std::thread accept_thread_;

  mutable std::mutex clients_mutex_;
  std::vector<int> client_fds_;

  int state_listener_id_ = -1;

  ~Impl() { DoStop(); }

  void DoStop() {
    if (!running_.exchange(false)) return;

    // Remove the state listener
    if (state_ && state_listener_id_ >= 0) {
      state_->RemoveListener(state_listener_id_);
      state_listener_id_ = -1;
    }

    // Close server socket to unblock accept()
    if (server_fd_ >= 0) {
      ::shutdown(server_fd_, SHUT_RDWR);
      ::close(server_fd_);
      server_fd_ = -1;
    }

    if (accept_thread_.joinable()) accept_thread_.join();

    // Close all client sockets
    std::lock_guard<std::mutex> lk(clients_mutex_);
    for (int fd : client_fds_) {
      ::shutdown(fd, SHUT_RDWR);
      ::close(fd);
    }
    client_fds_.clear();
  }

  void Broadcast(const std::string& payload) {
    std::lock_guard<std::mutex> lk(clients_mutex_);
    std::vector<int> dead;
    for (int fd : client_fds_) {
      if (!SendMessage(fd, payload)) {
        dead.push_back(fd);
      }
    }
    for (int fd : dead) {
      ::close(fd);
      client_fds_.erase(
          std::remove(client_fds_.begin(), client_fds_.end(), fd),
          client_fds_.end());
    }
  }

  void AcceptLoop() {
    while (running_.load()) {
      sockaddr_in client_addr{};
      socklen_t client_len = sizeof(client_addr);
      int client_fd =
          ::accept(server_fd_, reinterpret_cast<sockaddr*>(&client_addr),
                   &client_len);
      if (client_fd < 0) {
        if (!running_.load()) break;
        continue;
      }

      // Send current state immediately
      std::string current;
      if (state_) current = state_->Get();
      SendMessage(client_fd, current);

      {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        client_fds_.push_back(client_fd);
      }
    }
  }
};

// ── NetworkReactiveServer public API ─────────────────────────────────────────

std::shared_ptr<NetworkReactiveServer> NetworkReactiveServer::Create(
    std::shared_ptr<Reactive<std::string>> state, uint16_t port) {
  auto srv = std::shared_ptr<NetworkReactiveServer>(new NetworkReactiveServer());
  srv->impl_ = std::make_shared<Impl>();
  srv->impl_->state_ = std::move(state);
  srv->impl_->port_ = port;
  return srv;
}

void NetworkReactiveServer::Start() {
  auto& impl = *impl_;
  if (impl.running_.load()) return;

  impl.server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (impl.server_fd_ < 0) return;

  int opt = 1;
  ::setsockopt(impl.server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(impl.port_);

  if (::bind(impl.server_fd_, reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) < 0) {
    ::close(impl.server_fd_);
    impl.server_fd_ = -1;
    return;
  }

  if (::listen(impl.server_fd_, 16) < 0) {
    ::close(impl.server_fd_);
    impl.server_fd_ = -1;
    return;
  }

  impl.running_.store(true);

  // Subscribe to state changes: broadcast to all clients
  auto weak_impl = std::weak_ptr<Impl>(impl_);
  impl.state_listener_id_ =
      impl.state_->OnChange([weak_impl](const std::string& val) {
        if (auto p = weak_impl.lock()) {
          if (p->running_.load()) {
            p->Broadcast(val);
          }
        }
      });

  impl.accept_thread_ = std::thread([&impl]() { impl.AcceptLoop(); });
}

void NetworkReactiveServer::Stop() {
  impl_->DoStop();
}

int NetworkReactiveServer::ConnectedClients() const {
  std::lock_guard<std::mutex> lk(impl_->clients_mutex_);
  return static_cast<int>(impl_->client_fds_.size());
}

uint16_t NetworkReactiveServer::Port() const {
  return impl_->port_;
}

NetworkReactiveServer::~NetworkReactiveServer() = default;

// ── NetworkReactiveClient::Impl ───────────────────────────────────────────────

struct NetworkReactiveClient::Impl {
  std::string host_;
  uint16_t port_ = 0;

  std::shared_ptr<Reactive<std::string>> state_ =
      std::make_shared<Reactive<std::string>>();

  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  std::thread recv_thread_;

  ~Impl() { DoDisconnect(); }

  void DoDisconnect() {
    running_.store(false);
    if (recv_thread_.joinable()) recv_thread_.join();
  }

  void RecvLoop() {
    while (running_.load()) {
      // Attempt to connect
      int fd = ::socket(AF_INET, SOCK_STREAM, 0);
      if (fd < 0) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue;
      }

      // Resolve host
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port_);

      struct addrinfo hints{};
      struct addrinfo* res = nullptr;
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;
      bool resolved = false;
      if (::getaddrinfo(host_.c_str(), nullptr, &hints, &res) == 0 && res) {
        const auto* sin =
            reinterpret_cast<const sockaddr_in*>(res->ai_addr);
        addr.sin_addr = sin->sin_addr;
        resolved = true;
        ::freeaddrinfo(res);
      }

      if (!resolved || ::connect(fd, reinterpret_cast<sockaddr*>(&addr),
                                  sizeof(addr)) < 0) {
        ::close(fd);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue;
      }

      connected_.store(true);

      // Receive messages until disconnect
      while (running_.load()) {
        std::string msg;
        if (!RecvMessage(fd, msg)) break;
        state_->Set(msg);
      }

      connected_.store(false);
      ::close(fd);

      if (running_.load()) {
        // Retry after 2 seconds
        std::this_thread::sleep_for(std::chrono::seconds(2));
      }
    }
  }
};

// ── NetworkReactiveClient public API ─────────────────────────────────────────

std::shared_ptr<NetworkReactiveClient> NetworkReactiveClient::Connect(
    std::string host, uint16_t port) {
  auto client =
      std::shared_ptr<NetworkReactiveClient>(new NetworkReactiveClient());
  client->impl_ = std::make_shared<Impl>();
  client->impl_->host_ = std::move(host);
  client->impl_->port_ = port;
  client->impl_->running_.store(true);
  client->impl_->recv_thread_ =
      std::thread([impl = client->impl_]() { impl->RecvLoop(); });
  return client;
}

std::shared_ptr<Reactive<std::string>> NetworkReactiveClient::State() const {
  return impl_->state_;
}

bool NetworkReactiveClient::Connected() const {
  return impl_->connected_.load();
}

void NetworkReactiveClient::Disconnect() {
  impl_->DoDisconnect();
}

NetworkReactiveClient::~NetworkReactiveClient() = default;

}  // namespace ftxui::ui
