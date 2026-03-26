// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file web_bridge.cpp
///
/// Terminal-to-Web bridge: render any BadWolf TUI component over HTTP +
/// WebSocket so it can be viewed and interacted with from a browser using
/// xterm.js. Uses POSIX sockets only — no external dependencies.

#include "ftxui/ui/web_bridge.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/ui/app.hpp"

namespace ftxui::ui {

// ── SHA1 (RFC 3174) ──────────────────────────────────────────────────────────
// Minimal self-contained implementation used only for WebSocket handshake.

namespace {

inline uint32_t RotL32(uint32_t x, uint32_t n) {
  return (x << n) | (x >> (32u - n));
}

std::array<uint8_t, 20> SHA1Digest(const std::string& input) {
  uint32_t H[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u,
                   0xC3D2E1F0u};

  // Build padded message
  std::vector<uint8_t> msg(input.begin(), input.end());
  const uint64_t bit_len = static_cast<uint64_t>(input.size()) * 8u;
  msg.push_back(0x80u);
  while ((msg.size() % 64) != 56) {
    msg.push_back(0x00u);
  }
  for (int i = 7; i >= 0; --i) {
    msg.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFFu));
  }

  // Process 512-bit blocks
  for (size_t blk = 0; blk < msg.size(); blk += 64) {
    uint32_t W[80];
    for (int j = 0; j < 16; ++j) {
      W[j] = (static_cast<uint32_t>(msg[blk + j * 4]) << 24u) |
             (static_cast<uint32_t>(msg[blk + j * 4 + 1]) << 16u) |
             (static_cast<uint32_t>(msg[blk + j * 4 + 2]) << 8u) |
             static_cast<uint32_t>(msg[blk + j * 4 + 3]);
    }
    for (int j = 16; j < 80; ++j) {
      W[j] = RotL32(W[j - 3] ^ W[j - 8] ^ W[j - 14] ^ W[j - 16], 1);
    }

    uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4];
    for (int j = 0; j < 80; ++j) {
      uint32_t f = 0, k = 0;
      if (j < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999u;
      } else if (j < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1u;
      } else if (j < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCu;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6u;
      }
      const uint32_t tmp = RotL32(a, 5) + f + e + k + W[j];
      e = d;
      d = c;
      c = RotL32(b, 30);
      b = a;
      a = tmp;
    }
    H[0] += a;
    H[1] += b;
    H[2] += c;
    H[3] += d;
    H[4] += e;
  }

  std::array<uint8_t, 20> result{};
  for (int i = 0; i < 5; ++i) {
    result[i * 4 + 0] = static_cast<uint8_t>((H[i] >> 24u) & 0xFFu);
    result[i * 4 + 1] = static_cast<uint8_t>((H[i] >> 16u) & 0xFFu);
    result[i * 4 + 2] = static_cast<uint8_t>((H[i] >> 8u) & 0xFFu);
    result[i * 4 + 3] = static_cast<uint8_t>(H[i] & 0xFFu);
  }
  return result;
}

// ── Base64 encoding
// ───────────────────────────────────────────────────────────

static const char kBase64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const uint8_t* data, size_t len) {
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  for (size_t i = 0; i < len; i += 3) {
    const uint32_t v =
        (static_cast<uint32_t>(data[i]) << 16u) |
        (i + 1 < len ? static_cast<uint32_t>(data[i + 1]) << 8u : 0u) |
        (i + 2 < len ? static_cast<uint32_t>(data[i + 2]) : 0u);
    out += kBase64Table[(v >> 18u) & 63u];
    out += kBase64Table[(v >> 12u) & 63u];
    out += (i + 1 < len) ? kBase64Table[(v >> 6u) & 63u] : '=';
    out += (i + 2 < len) ? kBase64Table[v & 63u] : '=';
  }
  return out;
}

// ── WebSocket handshake
// ───────────────────────────────────────────────────────

std::string WebSocketAcceptKey(const std::string& client_key) {
  static const std::string kGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  const auto digest = SHA1Digest(client_key + kGUID);
  return Base64Encode(digest.data(), 20);
}

// ── WebSocket framing
// ─────────────────────────────────────────────────────────

// Encode a server→browser text frame (unmasked, FIN=1, opcode=0x1).
std::string WSEncodeTextFrame(const std::string& payload) {
  std::string frame;
  frame += '\x81';  // FIN=1, opcode=1 (text)
  const size_t len = payload.size();
  if (len < 126) {
    frame += static_cast<char>(len);
  } else if (len < 65536) {
    frame += '\x7E';
    frame += static_cast<char>((len >> 8u) & 0xFFu);
    frame += static_cast<char>(len & 0xFFu);
  } else {
    frame += '\x7F';
    for (int i = 7; i >= 0; --i) {
      frame += static_cast<char>((len >> (i * 8u)) & 0xFFu);
    }
  }
  frame += payload;
  return frame;
}

// Encode a close frame.
std::string WSCloseFrame() {
  std::string f;
  f += '\x88';  // FIN=1, opcode=8 (close)
  f += '\x00';  // payload length=0
  return f;
}

struct WSFrame {
  bool fin = false;
  int opcode = 0;
  std::string payload;
  bool valid = false;
};

// Decode one WebSocket frame from raw bytes.
// Returns the number of bytes consumed, or 0 if the buffer is incomplete.
size_t WSDecodeFrame(const uint8_t* data, size_t len, WSFrame& out) {
  if (len < 2) {
    return 0;
  }
  out.fin = (data[0] & 0x80u) != 0;
  out.opcode = static_cast<int>(data[0] & 0x0Fu);
  const bool masked = (data[1] & 0x80u) != 0;
  uint64_t payload_len = data[1] & 0x7Fu;
  size_t offset = 2;

  if (payload_len == 126) {
    if (len < offset + 2) {
      return 0;
    }
    payload_len =
        (static_cast<uint64_t>(data[offset]) << 8u) | data[offset + 1];
    offset += 2;
  } else if (payload_len == 127) {
    if (len < offset + 8) {
      return 0;
    }
    payload_len = 0;
    for (int i = 0; i < 8; ++i) {
      payload_len = (payload_len << 8u) | data[offset + i];
    }
    offset += 8;
  }

  uint8_t mask[4] = {0, 0, 0, 0};
  if (masked) {
    if (len < offset + 4) {
      return 0;
    }
    std::memcpy(mask, data + offset, 4);
    offset += 4;
  }

  if (len < offset + static_cast<size_t>(payload_len)) {
    return 0;
  }

  out.payload.resize(static_cast<size_t>(payload_len));
  for (size_t i = 0; i < static_cast<size_t>(payload_len); ++i) {
    out.payload[i] =
        static_cast<char>(data[offset + i] ^ (masked ? mask[i % 4] : 0u));
  }
  out.valid = true;
  return offset + static_cast<size_t>(payload_len);
}

// ── Embedded HTML page
// ────────────────────────────────────────────────────────

std::string BuildHTMLPage(const std::string& title) {
  return std::string(
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "  <title>") +
         title +
         std::string(
             "</title>\n"
             "  <link rel=\"stylesheet\" "
             "href=\"https://cdn.jsdelivr.net/npm/xterm@5/css/xterm.css\"/>\n"
             "  <style>\n"
             "    body { background:#000; margin:0; display:flex; "
             "justify-content:center; align-items:center; height:100vh; }\n"
             "    #terminal { width:100%; height:100%; }\n"
             "    .badge { position:fixed; top:8px; right:8px; "
             "background:rgba(139,92,246,0.8); color:#fff; padding:4px 10px; "
             "border-radius:6px; font:bold 12px monospace; }\n"
             "  </style>\n"
             "</head>\n"
             "<body>\n"
             "  <div id=\"terminal\"></div>\n"
             "  <div class=\"badge\">\xf0\x9f\x90\xba BadWolf Web</div>\n"
             "  <script "
             "src=\"https://cdn.jsdelivr.net/npm/xterm@5/lib/xterm.js\">"
             "</script>\n"
             "  <script "
             "src=\"https://cdn.jsdelivr.net/npm/xterm-addon-fit@0.8/lib/"
             "xterm-addon-fit.js\"></script>\n"
             "  <script>\n"
             "    const term = new Terminal({ cursorBlink:true, fontSize:14, "
             "fontFamily:'monospace', "
             "theme:{background:'#0d0d1a',foreground:'#e2e8f0'} });\n"
             "    const fitAddon = new FitAddon.FitAddon();\n"
             "    term.loadAddon(fitAddon);\n"
             "    term.open(document.getElementById('terminal'));\n"
             "    fitAddon.fit();\n"
             "    window.addEventListener('resize', () => fitAddon.fit());\n"
             "\n"
             "    const ws = new WebSocket(`ws://${location.host}/ws`);\n"
             "    ws.onmessage = e => term.write(e.data);\n"
             "    ws.onclose = () => term.write('\\r\\n\\x1b[31m[disconnected]"
             "\\x1b[0m\\r\\n');\n"
             "    term.onKey(e => ws.readyState === 1 && ws.send(e.key));\n"
             "    ws.onopen = () => term.write('\\x1b[2J\\x1b[H');\n"
             "  </script>\n"
             "</body>\n"
             "</html>\n");
}

// ── Minimal HTTP request parser
// ───────────────────────────────────────────────

struct HttpRequest {
  std::string method;
  std::string path;
  // Only headers we care about
  std::string ws_key;  // Sec-WebSocket-Key
  bool is_ws_upgrade = false;
  bool complete = false;
};

HttpRequest ParseHTTPRequest(const std::string& raw) {
  HttpRequest req;
  const size_t header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return req;
  }

  std::istringstream ss(raw.substr(0, header_end));
  std::string line;

  if (!std::getline(ss, line)) {
    return req;
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  {
    std::istringstream ls(line);
    ls >> req.method >> req.path;
  }

  while (std::getline(ss, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    const auto colon = line.find(": ");
    if (colon == std::string::npos) {
      continue;
    }
    std::string key = line.substr(0, colon);
    const std::string val = line.substr(colon + 2);
    for (auto& ch : key) {
      ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    }
    if (key == "sec-websocket-key") {
      req.ws_key = val;
    } else if (key == "upgrade") {
      std::string v = val;
      for (auto& ch : v) {
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
      }
      req.is_ws_upgrade = (v.find("websocket") != std::string::npos);
    }
  }
  req.complete = true;
  return req;
}

// ── Keystroke → ftxui::Event conversion ──────────────────────────────────────

ftxui::Event KeystrokeToEvent(const std::string& key) {
  if (key.empty()) {
    return ftxui::Event::Custom;
  }

  // CSI sequences from xterm.js
  if (key.size() >= 3 && key[0] == '\x1b' && key[1] == '[') {
    if (key == "\x1b[A") {
      return ftxui::Event::ArrowUp;
    }
    if (key == "\x1b[B") {
      return ftxui::Event::ArrowDown;
    }
    if (key == "\x1b[C") {
      return ftxui::Event::ArrowRight;
    }
    if (key == "\x1b[D") {
      return ftxui::Event::ArrowLeft;
    }
    if (key == "\x1b[H") {
      return ftxui::Event::Home;
    }
    if (key == "\x1b[F") {
      return ftxui::Event::End;
    }
    if (key == "\x1b[2~") {
      return ftxui::Event::Insert;
    }
    if (key == "\x1b[3~") {
      return ftxui::Event::Delete;
    }
    if (key == "\x1b[5~") {
      return ftxui::Event::PageUp;
    }
    if (key == "\x1b[6~") {
      return ftxui::Event::PageDown;
    }
    return ftxui::Event::Special(key);
  }

  if (key == "\x1b") {
    return ftxui::Event::Escape;
  }
  if (key == "\r" || key == "\n") {
    return ftxui::Event::Return;
  }
  if (key == "\x7f" || key == "\x08") {
    return ftxui::Event::Backspace;
  }
  if (key == "\t") {
    return ftxui::Event::Tab;
  }
  if (key == "\x1b[Z") {
    return ftxui::Event::TabReverse;
  }

  // Ctrl+A through Ctrl+Z (except Ctrl+C which we don't treat as quit)
  if (key.size() == 1 && key[0] >= 1 && key[0] <= 26) {
    return ftxui::Event::Special(key);
  }

  // Printable characters and multibyte UTF-8
  if (!key.empty()) {
    return ftxui::Event::Character(key);
  }

  return ftxui::Event::Custom;
}

// ── Per-client state
// ──────────────────────────────────────────────────────────

struct ClientSession {
  int fd = -1;
  std::atomic<bool> upgraded{false};
  std::atomic<bool> closed{false};

  bool SendRaw(const std::string& data) {
    if (fd < 0 || closed.load()) {
      return false;
    }
    size_t sent = 0;
    while (sent < data.size()) {
      const ssize_t n =
          ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
      if (n <= 0) {
        closed = true;
        return false;
      }
      sent += static_cast<size_t>(n);
    }
    return true;
  }

  bool SendWS(const std::string& payload) {
    return SendRaw(WSEncodeTextFrame(payload));
  }
};

}  // namespace

// ── WebBridge::Impl
// ───────────────────────────────────────────────────────────

struct WebBridge::Impl {
  WebBridgeConfig cfg;
  ftxui::Component root;

  int server_fd = -1;
  int actual_port = 0;
  std::atomic<bool> running{false};

  std::thread server_thread;
  std::thread render_thread;

  std::mutex clients_mutex;
  std::vector<std::shared_ptr<ClientSession>> clients;

  std::mutex events_mutex;
  std::vector<ftxui::Event> pending_events;

  void AddEvent(ftxui::Event e) {
    std::lock_guard<std::mutex> lock(events_mutex);
    pending_events.push_back(std::move(e));
  }

  void BroadcastANSI(const std::string& ansi) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& c : clients) {
      if (!c->closed.load() && c->upgraded.load()) {
        c->SendWS(ansi);
      }
    }
    clients.erase(
        std::remove_if(clients.begin(), clients.end(),
                       [](const auto& c) { return c->closed.load(); }),
        clients.end());
  }

  bool StartServer() {
    server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
      return false;
    }

    int opt = 1;
    ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Try the configured port and up to 10 alternates on conflict.
    for (int port = cfg.port; port <= cfg.port + 10; ++port) {
      struct sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(static_cast<uint16_t>(port));
      addr.sin_addr.s_addr = INADDR_ANY;

      if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) ==
          0) {
        ::listen(server_fd, 10);
        actual_port = port;
        return true;
      }
    }

    ::close(server_fd);
    server_fd = -1;
    return false;
  }

  void HandleClient(int client_fd, std::shared_ptr<ClientSession> session) {
    // Read until we have the full HTTP request headers.
    std::string recv_buf;
    recv_buf.reserve(4096);
    {
      char tmp[1024];
      while (true) {
        const ssize_t n = ::recv(client_fd, tmp, sizeof(tmp), 0);
        if (n <= 0) {
          ::close(client_fd);
          return;
        }
        recv_buf.append(tmp, static_cast<size_t>(n));
        if (recv_buf.find("\r\n\r\n") != std::string::npos) {
          break;
        }
      }
    }

    const auto req = ParseHTTPRequest(recv_buf);
    if (!req.complete) {
      ::close(client_fd);
      return;
    }

    if (req.path == "/ws" && req.is_ws_upgrade && !req.ws_key.empty()) {
      // WebSocket upgrade handshake
      const std::string accept = WebSocketAcceptKey(req.ws_key);
      const std::string response =
          "HTTP/1.1 101 Switching Protocols\r\n"
          "Upgrade: websocket\r\n"
          "Connection: Upgrade\r\n"
          "Sec-WebSocket-Accept: " +
          accept + "\r\n\r\n";
      ::send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
      session->upgraded = true;

      // WebSocket read loop
      std::vector<uint8_t> ws_buf;
      ws_buf.reserve(4096);
      while (!session->closed.load() && running.load()) {
        char buf[512];
        const ssize_t n = ::recv(client_fd, buf, sizeof(buf), 0);
        if (n <= 0) {
          break;
        }
        ws_buf.insert(ws_buf.end(), buf, buf + n);

        size_t consumed = 0;
        while (consumed < ws_buf.size()) {
          WSFrame frame;
          const size_t used = WSDecodeFrame(ws_buf.data() + consumed,
                                            ws_buf.size() - consumed, frame);
          if (used == 0) {
            break;
          }
          consumed += used;

          if (!frame.valid) {
            continue;
          }
          if (frame.opcode == 8) {  // Close
            session->SendRaw(WSCloseFrame());
            session->closed = true;
            break;
          } else if (frame.opcode == 9) {  // Ping → pong
            std::string pong;
            pong += '\x8A';
            pong += '\x00';
            session->SendRaw(pong);
          } else if (frame.opcode == 1) {  // Text
            const auto evt = KeystrokeToEvent(frame.payload);
            if (evt != ftxui::Event::Custom) {
              AddEvent(evt);
            }
          }
        }
        if (consumed > 0) {
          ws_buf.erase(ws_buf.begin(),
                       ws_buf.begin() + static_cast<ptrdiff_t>(consumed));
        }
      }
    } else {
      // Serve the xterm.js HTML page for any other GET request.
      const std::string body = BuildHTMLPage(cfg.title);
      const std::string response =
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/html; charset=utf-8\r\n"
          "Content-Length: " +
          std::to_string(body.size()) +
          "\r\n"
          "Connection: close\r\n"
          "\r\n" +
          body;
      ::send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
    }

    session->closed = true;
    ::close(client_fd);
  }

  void ServerThread() {
    while (running.load()) {
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(server_fd, &fds);
      struct timeval tv{0, 100000};  // 100 ms
      if (::select(server_fd + 1, &fds, nullptr, nullptr, &tv) <= 0) {
        continue;
      }

      struct sockaddr_in client_addr{};
      socklen_t client_len = sizeof(client_addr);
      const int client_fd = ::accept(
          server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
      if (client_fd < 0) {
        continue;
      }

      auto session = std::make_shared<ClientSession>();
      session->fd = client_fd;
      {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back(session);
      }

      std::thread([this, client_fd, session]() {
        HandleClient(client_fd, session);
      }).detach();
    }
  }

  void RenderThread() {
    const auto interval =
        std::chrono::milliseconds(1000 / std::max(1, cfg.fps));
    ftxui::Screen screen(cfg.cols, cfg.rows);

    while (running.load()) {
      const auto frame_start = std::chrono::steady_clock::now();

      // Drain and dispatch pending events.
      {
        std::vector<ftxui::Event> evts;
        {
          std::lock_guard<std::mutex> lock(events_mutex);
          evts = std::move(pending_events);
          pending_events.clear();
        }
        for (auto& e : evts) {
          root->OnEvent(e);
        }
      }

      // Render component → ANSI string.
      screen.Clear();
      auto doc = root->Render();
      ftxui::Render(screen, doc.get());
      const std::string ansi = screen.ToString();

      // Prepend cursor-home so every frame overwrites the previous one.
      BroadcastANSI("\x1b[H" + ansi);

      // Sleep for the remainder of the frame budget.
      const auto elapsed = std::chrono::steady_clock::now() - frame_start;
      const auto sleep_time = interval - elapsed;
      if (sleep_time > std::chrono::nanoseconds(0)) {
        std::this_thread::sleep_for(sleep_time);
      }
    }
  }
};

// ── WebBridge public API
// ──────────────────────────────────────────────────────

WebBridge::WebBridge(WebBridgeConfig cfg) : impl_(std::make_unique<Impl>()) {
  impl_->cfg = std::move(cfg);
}

WebBridge::~WebBridge() {
  Stop();
}

void WebBridge::Start(ftxui::Component root) {
  if (impl_->running.load()) {
    return;
  }
  impl_->root = std::move(root);

  if (!impl_->StartServer()) {
    return;
  }

  impl_->running = true;
  impl_->server_thread = std::thread([this]() { impl_->ServerThread(); });
  impl_->render_thread = std::thread([this]() { impl_->RenderThread(); });

  if (impl_->cfg.open_browser) {
    const std::string cmd = "xdg-open " + url() + " 2>/dev/null &";
    // NOLINTNEXTLINE(cert-env33-c)
    ::system(cmd.c_str());
  }
}

void WebBridge::Run(ftxui::Component root) {
  Start(std::move(root));
  if (!impl_->running.load()) {
    return;
  }

  std::cerr << "\xf0\x9f\x90\xba BadWolf Web Bridge — " << url() << "\n";
  std::cerr << "Press Ctrl+C to stop.\n";

  while (impl_->running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  Stop();
}

void WebBridge::Stop() {
  if (!impl_->running.load()) {
    return;
  }
  impl_->running = false;

  if (impl_->server_fd >= 0) {
    ::close(impl_->server_fd);
    impl_->server_fd = -1;
  }

  if (impl_->server_thread.joinable()) {
    impl_->server_thread.join();
  }
  if (impl_->render_thread.joinable()) {
    impl_->render_thread.join();
  }

  {
    std::lock_guard<std::mutex> lock(impl_->clients_mutex);
    for (auto& c : impl_->clients) {
      c->closed = true;
      if (c->fd >= 0) {
        ::close(c->fd);
      }
    }
    impl_->clients.clear();
  }
}

bool WebBridge::IsRunning() const {
  return impl_->running.load();
}

int WebBridge::port() const {
  return impl_->actual_port;
}

std::string WebBridge::url() const {
  return "http://localhost:" + std::to_string(port());
}

void WebBridge::InjectEvent(ftxui::Event e) {
  impl_->AddEvent(std::move(e));
}

// ── RunWebOrTerminal
// ──────────────────────────────────────────────────────────

void RunWebOrTerminal(int argc,
                      char** argv,
                      std::function<ftxui::Component()> factory,
                      WebBridgeConfig cfg) {
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--web") {
      auto bridge = WebBridge(std::move(cfg));
      auto root = factory();
      bridge.Run(std::move(root));
      return;
    }
  }
  // Normal terminal mode.
  RunFullscreen(factory());
}

}  // namespace ftxui::ui
