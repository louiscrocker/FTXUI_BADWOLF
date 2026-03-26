// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/multiplayer.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/json.hpp"

namespace ftxui::ui {

// ── Wire protocol helpers (same framing as network_reactive / collab)
// ─────────────── [4-byte LE length][payload bytes]

namespace {

bool WireSend(int fd, const std::string& payload) {
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
    ssize_t n = ::send(fd, payload.data() + sent,
                       static_cast<size_t>(len) - static_cast<size_t>(sent),
                       MSG_NOSIGNAL);
    if (n <= 0) {
      return false;
    }
    sent += n;
  }
  return true;
}

bool WireRecv(int fd, std::string& out) {
  uint8_t hdr[4] = {};
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
  if (len > 4 * 1024 * 1024) {
    return false;  // sanity limit
  }
  out.resize(static_cast<size_t>(len));
  ssize_t recvd = 0;
  while (recvd < static_cast<ssize_t>(len)) {
    ssize_t n =
        ::recv(fd, out.data() + recvd,
               static_cast<size_t>(len) - static_cast<size_t>(recvd), 0);
    if (n <= 0) {
      return false;
    }
    recvd += n;
  }
  return true;
}

}  // namespace

// ── Color helpers
// ─────────────────────────────────────────────────────────────
namespace {

// Convert Color to/from an index in our 8-color palette for JSON transport.
int ColorToInt(ftxui::Color c) {
  // Map known named colors to palette indices
  static const ftxui::Color palette[] = {
      ftxui::Color::Cyan,   ftxui::Color::Red,     ftxui::Color::Green,
      ftxui::Color::Yellow, ftxui::Color::Blue,    ftxui::Color::Magenta,
      ftxui::Color::White,  ftxui::Color::Orange1,
  };
  for (int i = 0; i < 8; ++i) {
    if (c == palette[i]) {
      return i;
    }
  }
  return 6;  // default to White
}

ftxui::Color IntToColor(int v) {
  static const ftxui::Color palette[] = {
      ftxui::Color::Cyan,   ftxui::Color::Red,     ftxui::Color::Green,
      ftxui::Color::Yellow, ftxui::Color::Blue,    ftxui::Color::Magenta,
      ftxui::Color::White,  ftxui::Color::Orange1,
  };
  if (v >= 0 && v < 8) {
    return palette[v];
  }
  return ftxui::Color::White;
}

// Map player_id → a consistent palette color
ftxui::Color PlayerColor(int id) {
  static const ftxui::Color palette[] = {
      ftxui::Color::Cyan,   ftxui::Color::Red,     ftxui::Color::Green,
      ftxui::Color::Yellow, ftxui::Color::Blue,    ftxui::Color::Magenta,
      ftxui::Color::White,  ftxui::Color::Orange1,
  };
  return palette[id % 8];
}

}  // namespace

// ── GameState serialization
// ───────────────────────────────────────────────────

JsonValue GameState::ToJson() const {
  auto obj = JsonValue::Object();
  obj.set("f", JsonValue(static_cast<int>(frame)));
  obj.set("ph", JsonValue(game_phase));
  obj.set("r", JsonValue(round));
  obj.set("tr", JsonValue(static_cast<double>(time_remaining)));

  auto ppos = JsonValue::Object();
  for (const auto& [id, v] : player_positions) {
    auto vobj = JsonValue::Object();
    vobj.set("x", JsonValue(static_cast<double>(v.x)));
    vobj.set("y", JsonValue(static_cast<double>(v.y)));
    ppos.set(std::to_string(id), vobj);
  }
  obj.set("pp", ppos);

  auto pvel = JsonValue::Object();
  for (const auto& [id, v] : player_velocities) {
    auto vobj = JsonValue::Object();
    vobj.set("x", JsonValue(static_cast<double>(v.x)));
    vobj.set("y", JsonValue(static_cast<double>(v.y)));
    pvel.set(std::to_string(id), vobj);
  }
  obj.set("pv", pvel);

  auto psc = JsonValue::Object();
  for (const auto& [id, s] : player_scores) {
    psc.set(std::to_string(id), JsonValue(s));
  }
  obj.set("ps", psc);

  auto pnames = JsonValue::Object();
  for (const auto& [id, n] : player_names) {
    pnames.set(std::to_string(id), JsonValue(n));
  }
  obj.set("pn", pnames);

  auto pcolors = JsonValue::Object();
  for (const auto& [id, c] : player_colors) {
    pcolors.set(std::to_string(id), JsonValue(ColorToInt(c)));
  }
  obj.set("pc", pcolors);

  auto proj = JsonValue::Array();
  for (const auto& [pos, vel, col] : projectiles) {
    auto p = JsonValue::Object();
    p.set("px", JsonValue(static_cast<double>(pos.x)));
    p.set("py", JsonValue(static_cast<double>(pos.y)));
    p.set("vx", JsonValue(static_cast<double>(vel.x)));
    p.set("vy", JsonValue(static_cast<double>(vel.y)));
    p.set("c", JsonValue(ColorToInt(col)));
    proj.push(p);
  }
  obj.set("prj", proj);

  return obj;
}

/*static*/ GameState GameState::FromJson(const JsonValue& j) {
  GameState gs;
  if (!j.is_object()) {
    return gs;
  }
  gs.frame = static_cast<uint64_t>(j["f"].get_int(0));
  gs.game_phase = j["ph"].get_string("waiting");
  gs.round = j["r"].get_int(1);
  gs.time_remaining = static_cast<float>(j["tr"].get_number(0.0));

  if (j.has("pp") && j["pp"].is_object()) {
    for (const auto& key : j["pp"].keys()) {
      const auto& v = j["pp"][key];
      Vec2 pos;
      pos.x = static_cast<float>(v["x"].get_number(0.0));
      pos.y = static_cast<float>(v["y"].get_number(0.0));
      gs.player_positions[std::stoi(key)] = pos;
    }
  }

  if (j.has("pv") && j["pv"].is_object()) {
    for (const auto& key : j["pv"].keys()) {
      const auto& v = j["pv"][key];
      Vec2 vel;
      vel.x = static_cast<float>(v["x"].get_number(0.0));
      vel.y = static_cast<float>(v["y"].get_number(0.0));
      gs.player_velocities[std::stoi(key)] = vel;
    }
  }

  if (j.has("ps") && j["ps"].is_object()) {
    for (const auto& key : j["ps"].keys()) {
      gs.player_scores[std::stoi(key)] = j["ps"][key].get_int(0);
    }
  }

  if (j.has("pn") && j["pn"].is_object()) {
    for (const auto& key : j["pn"].keys()) {
      gs.player_names[std::stoi(key)] = j["pn"][key].get_string("");
    }
  }

  if (j.has("pc") && j["pc"].is_object()) {
    for (const auto& key : j["pc"].keys()) {
      gs.player_colors[std::stoi(key)] = IntToColor(j["pc"][key].get_int(0));
    }
  }

  if (j.has("prj") && j["prj"].is_array()) {
    for (size_t i = 0; i < j["prj"].as_array().size(); ++i) {
      const auto& p = j["prj"][i];
      Vec2 pos{static_cast<float>(p["px"].get_number(0)),
               static_cast<float>(p["py"].get_number(0))};
      Vec2 vel{static_cast<float>(p["vx"].get_number(0)),
               static_cast<float>(p["vy"].get_number(0))};
      auto col = IntToColor(p["c"].get_int(0));
      gs.projectiles.emplace_back(pos, vel, col);
    }
  }

  return gs;
}

// ── MultiplayerServer
// ─────────────────────────────────────────────────────────

MultiplayerServer::MultiplayerServer(Config cfg)
    : cfg_(cfg), world_(std::make_unique<PhysicsWorld>(Vec2{0, 0})) {
  state_.game_phase = "waiting";
}

MultiplayerServer::~MultiplayerServer() {
  Stop();
}

void MultiplayerServer::Start() {
  if (running_.load()) {
    return;
  }

  // Create listening socket
  listen_sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock_ < 0) {
    return;
  }
  int opt = 1;
  ::setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(cfg_.port));
  if (::bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) <
      0) {
    ::close(listen_sock_);
    listen_sock_ = -1;
    return;
  }
  if (::listen(listen_sock_, cfg_.max_players) < 0) {
    ::close(listen_sock_);
    listen_sock_ = -1;
    return;
  }

  running_.store(true);
  server_thread_ = std::thread([this]() { AcceptLoop(); });
  game_thread_ = std::thread([this]() { GameLoop(); });
}

void MultiplayerServer::Stop() {
  if (!running_.load()) {
    return;
  }
  running_.store(false);

  if (listen_sock_ >= 0) {
    ::close(listen_sock_);
    listen_sock_ = -1;
  }

  // Close client sockets
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (int s : client_sockets_) {
      ::close(s);
    }
    client_sockets_.clear();
  }

  if (server_thread_.joinable()) {
    server_thread_.join();
  }
  if (game_thread_.joinable()) {
    game_thread_.join();
  }
}

bool MultiplayerServer::IsRunning() const {
  return running_.load();
}

int MultiplayerServer::PlayerCount() const {
  return player_count_.load();
}

int MultiplayerServer::port() const {
  return cfg_.port;
}

void MultiplayerServer::OnTick(
    std::function<void(GameState&, const std::vector<PlayerInput>&)> fn) {
  tick_fn_ = std::move(fn);
}

void MultiplayerServer::BroadcastState(const GameState& state) {
  auto j = state.ToJson();
  std::string msg = Json::StringifyCompact(j);
  SendToAll(msg);
}

const GameState& MultiplayerServer::CurrentState() const {
  return state_;
}

void MultiplayerServer::AcceptLoop() {
  int next_player_id = 0;
  while (running_.load()) {
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int client_fd =
        ::accept(listen_sock_, reinterpret_cast<sockaddr*>(&client_addr), &len);
    if (client_fd < 0) {
      break;
    }
    {
      std::lock_guard<std::mutex> lock(clients_mutex_);
      if (static_cast<int>(client_sockets_.size()) >= cfg_.max_players) {
        ::close(client_fd);
        continue;
      }
      client_sockets_.push_back(client_fd);
    }
    player_count_.fetch_add(1);
    int pid = next_player_id++;
    // Tell client their player_id
    auto welcome = JsonValue::Object();
    welcome.set("type", JsonValue(std::string("welcome")));
    welcome.set("pid", JsonValue(pid));
    WireSend(client_fd, Json::StringifyCompact(welcome));

    std::thread([this, client_fd, pid]() {
      ProcessClient(client_fd, pid);
    }).detach();
  }
}

void MultiplayerServer::GameLoop() {
  const int ms_per_frame = 1000 / cfg_.target_fps;
  while (running_.load()) {
    auto frame_start = std::chrono::steady_clock::now();

    // Collect and clear pending inputs
    std::vector<PlayerInput> inputs;
    {
      std::lock_guard<std::mutex> lock(inputs_mutex_);
      inputs.swap(pending_inputs_);
    }

    // Advance frame
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      state_.frame++;
      if (tick_fn_) {
        tick_fn_(state_, inputs);
      }
      BroadcastState(state_);
    }

    // Sleep for the remainder of the frame
    auto elapsed = std::chrono::steady_clock::now() - frame_start;
    auto sleep_ms =
        ms_per_frame -
        static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                .count());
    if (sleep_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
  }
}

void MultiplayerServer::ProcessClient(int sock, int player_id) {
  while (running_.load()) {
    std::string msg;
    if (!WireRecv(sock, msg)) {
      break;
    }
    // Parse PlayerInput JSON
    try {
      auto j = Json::Parse(msg);
      if (!j.is_object()) {
        continue;
      }
      PlayerInput inp;
      inp.player_id = player_id;
      inp.frame = static_cast<uint64_t>(j["frame"].get_int(0));
      if (j.has("keys") && j["keys"].is_object()) {
        inp.keys.up = j["keys"]["up"].get_bool(false);
        inp.keys.down = j["keys"]["down"].get_bool(false);
        inp.keys.left = j["keys"]["left"].get_bool(false);
        inp.keys.right = j["keys"]["right"].get_bool(false);
        inp.keys.action1 = j["keys"]["a1"].get_bool(false);
        inp.keys.action2 = j["keys"]["a2"].get_bool(false);
      }
      inp.aim_x = static_cast<float>(j["ax"].get_number(0.0));
      inp.aim_y = static_cast<float>(j["ay"].get_number(0.0));
      {
        std::lock_guard<std::mutex> lock(inputs_mutex_);
        pending_inputs_.push_back(inp);
      }
    } catch (...) {
      // malformed input — ignore
    }
  }

  // Remove from client list
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = std::find(client_sockets_.begin(), client_sockets_.end(), sock);
    if (it != client_sockets_.end()) {
      client_sockets_.erase(it);
    }
  }
  ::close(sock);
  player_count_.fetch_sub(1);
}

void MultiplayerServer::SendToAll(const std::string& msg) {
  std::lock_guard<std::mutex> lock(clients_mutex_);
  for (int s : client_sockets_) {
    WireSend(s, msg);
  }
}

// ── MultiplayerClient
// ─────────────────────────────────────────────────────────

MultiplayerClient::MultiplayerClient(Config cfg) : cfg_(std::move(cfg)) {}

MultiplayerClient::~MultiplayerClient() {
  Disconnect();
}

bool MultiplayerClient::Connect() {
  if (connected_.load()) {
    return true;
  }

  sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock_ < 0) {
    last_error_ = "Failed to create socket";
    return false;
  }

  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  int gai = ::getaddrinfo(cfg_.host.c_str(), std::to_string(cfg_.port).c_str(),
                          &hints, &res);
  if (gai != 0 || res == nullptr) {
    last_error_ = std::string("getaddrinfo failed: ") + ::gai_strerror(gai);
    ::close(sock_);
    sock_ = -1;
    return false;
  }

  // Set a short connect timeout via non-blocking + select
  if (::connect(sock_, res->ai_addr, res->ai_addrlen) < 0) {
    ::freeaddrinfo(res);
    last_error_ = std::string("connect failed: ") + ::strerror(errno);
    ::close(sock_);
    sock_ = -1;
    return false;
  }
  ::freeaddrinfo(res);

  connected_.store(true);

  // Read welcome message to get player_id
  std::string wmsg;
  if (WireRecv(sock_, wmsg)) {
    try {
      auto j = Json::Parse(wmsg);
      if (j.has("pid")) {
        player_id_ = j["pid"].get_int(-1);
      }
    } catch (...) {
    }
  }

  recv_thread_ = std::thread([this]() { RecvLoop(); });
  return true;
}

void MultiplayerClient::Disconnect() {
  if (!connected_.load()) {
    return;
  }
  connected_.store(false);
  if (sock_ >= 0) {
    ::close(sock_);
    sock_ = -1;
  }
  if (recv_thread_.joinable()) {
    recv_thread_.join();
  }
}

bool MultiplayerClient::IsConnected() const {
  return connected_.load();
}

int MultiplayerClient::player_id() const {
  return player_id_;
}

void MultiplayerClient::SendInput(const PlayerInput& input) {
  if (!connected_.load() || sock_ < 0) {
    return;
  }
  auto j = JsonValue::Object();
  j.set("frame", JsonValue(static_cast<int>(input.frame)));
  auto keys = JsonValue::Object();
  keys.set("up", JsonValue(input.keys.up));
  keys.set("down", JsonValue(input.keys.down));
  keys.set("left", JsonValue(input.keys.left));
  keys.set("right", JsonValue(input.keys.right));
  keys.set("a1", JsonValue(input.keys.action1));
  keys.set("a2", JsonValue(input.keys.action2));
  j.set("keys", keys);
  j.set("ax", JsonValue(static_cast<double>(input.aim_x)));
  j.set("ay", JsonValue(static_cast<double>(input.aim_y)));
  WireSend(sock_, Json::StringifyCompact(j));
}

const GameState& MultiplayerClient::State() const {
  return latest_state_;
}

int MultiplayerClient::OnStateUpdate(std::function<void(const GameState&)> fn) {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  int id = next_cb_id_++;
  callbacks_[id] = std::move(fn);
  return id;
}

void MultiplayerClient::RemoveCallback(int id) {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  callbacks_.erase(id);
}

std::string MultiplayerClient::last_error() const {
  return last_error_;
}

void MultiplayerClient::RecvLoop() {
  while (connected_.load()) {
    std::string msg;
    if (!WireRecv(sock_, msg)) {
      break;
    }
    try {
      auto j = Json::Parse(msg);
      auto state = GameState::FromJson(j);
      {
        std::lock_guard<std::mutex> lock(state_mutex_);
        latest_state_ = state;
      }
      // Fire callbacks
      std::map<int, std::function<void(const GameState&)>> cbs;
      {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        cbs = callbacks_;
      }
      for (auto& [id, fn] : cbs) {
        fn(state);
      }
    } catch (...) {
    }
  }
  connected_.store(false);
}

// ── MultiplayerRenderer
// ───────────────────────────────────────────────────────

MultiplayerRenderer::MultiplayerRenderer(float world_w, float world_h)
    : world_w_(world_w), world_h_(world_h) {}

Vec2 MultiplayerRenderer::WorldToCanvas(Vec2 world,
                                        int canvas_w,
                                        int canvas_h) const {
  float cx = (world.x / world_w_) * static_cast<float>(canvas_w);
  float cy = (world.y / world_h_) * static_cast<float>(canvas_h);
  return {cx, cy};
}

ftxui::Element MultiplayerRenderer::Render(const GameState& state) const {
  return ftxui::canvas([state, this](ftxui::Canvas& c) {
           const int cw = c.width();
           const int ch = c.height();

           // Draw world border
           c.DrawBlockLine(0, 0, cw - 1, 0, ftxui::Color::GrayDark);
           c.DrawBlockLine(0, ch - 1, cw - 1, ch - 1, ftxui::Color::GrayDark);
           c.DrawBlockLine(0, 0, 0, ch - 1, ftxui::Color::GrayDark);
           c.DrawBlockLine(cw - 1, 0, cw - 1, ch - 1, ftxui::Color::GrayDark);

           // Draw players
           for (const auto& [id, pos] : state.player_positions) {
             auto cp = WorldToCanvas(pos, cw, ch);
             int x = static_cast<int>(cp.x);
             int y = static_cast<int>(cp.y);
             ftxui::Color col = ftxui::Color::White;
             {
               auto it = state.player_colors.find(id);
               if (it != state.player_colors.end()) {
                 col = it->second;
               }
             }
             // Draw filled circle (radius 3)
             c.DrawPointCircleFilled(x, y, 3, col);

             // Draw player label
             std::string label = std::to_string(id);
             {
               auto it = state.player_names.find(id);
               if (it != state.player_names.end() && !it->second.empty()) {
                 label = it->second.substr(0, 3);
               }
             }
             c.DrawText(x - static_cast<int>(label.size()), y - 5, label, col);
           }

           // Draw projectiles
           for (const auto& [pos, vel, col] : state.projectiles) {
             auto cp = WorldToCanvas(pos, cw, ch);
             int x = static_cast<int>(cp.x);
             int y = static_cast<int>(cp.y);
             c.DrawPoint(x, y, true, col);
           }
         }) |
         ftxui::flex;
}

// ── Score panel element
// ───────────────────────────────────────────────────────
namespace {

ftxui::Element ScorePanel(const GameState& state) {
  std::vector<ftxui::Element> entries;
  entries.push_back(ftxui::text("  Score  ") | ftxui::bold);
  entries.push_back(ftxui::separator());
  for (const auto& [id, score] : state.player_scores) {
    ftxui::Color col = ftxui::Color::White;
    {
      auto it = state.player_colors.find(id);
      if (it != state.player_colors.end()) {
        col = it->second;
      }
    }
    std::string name = "P" + std::to_string(id);
    {
      auto it = state.player_names.find(id);
      if (it != state.player_names.end() && !it->second.empty()) {
        name = it->second;
      }
    }
    entries.push_back(ftxui::hbox({
        ftxui::text("■ ") | ftxui::color(col),
        ftxui::text(name) | ftxui::color(col),
        ftxui::text(": "),
        ftxui::text(std::to_string(score)) | ftxui::bold,
    }));
  }
  return ftxui::vbox(entries) | ftxui::border;
}

}  // namespace

// ── MultiGames
// ────────────────────────────────────────────────────────────────

namespace MultiGames {

// ── Lobby
// ─────────────────────────────────────────────────────────────────────

ftxui::Component Lobby(MultiplayerServer* server, MultiplayerClient* client) {
  return ftxui::Renderer([server, client]() -> ftxui::Element {
    std::vector<ftxui::Element> rows;

    rows.push_back(ftxui::text("🎮 BadWolf Multiplayer Lobby") | ftxui::bold |
                   ftxui::center);
    rows.push_back(ftxui::separator());

    if (server) {
      rows.push_back(ftxui::hbox({
          ftxui::text("Server: ") | ftxui::bold,
          ftxui::text(server->IsRunning() ? "Running" : "Stopped") |
              ftxui::color(server->IsRunning() ? ftxui::Color::Green
                                               : ftxui::Color::Red),
          ftxui::text("  Port: "),
          ftxui::text(std::to_string(server->port())),
          ftxui::text("  Players: "),
          ftxui::text(std::to_string(server->PlayerCount())),
      }));
      rows.push_back(ftxui::separator());
      const auto& state = server->CurrentState();
      if (!state.player_names.empty()) {
        rows.push_back(ftxui::text("Connected Players:"));
        for (const auto& [id, name] : state.player_names) {
          ftxui::Color col = ftxui::Color::White;
          auto it = state.player_colors.find(id);
          if (it != state.player_colors.end()) {
            col = it->second;
          }
          rows.push_back(ftxui::hbox({
              ftxui::text("  ■ ") | ftxui::color(col),
              ftxui::text(name),
          }));
        }
      }
    }

    if (client) {
      rows.push_back(ftxui::hbox({
          ftxui::text("Client: ") | ftxui::bold,
          ftxui::text(client->IsConnected() ? "Connected" : "Disconnected") |
              ftxui::color(client->IsConnected() ? ftxui::Color::Green
                                                 : ftxui::Color::Red),
          ftxui::text("  Player ID: "),
          ftxui::text(std::to_string(client->player_id())),
      }));
      rows.push_back(ftxui::separator());
      const auto& state = client->State();
      rows.push_back(ftxui::hbox({
          ftxui::text("Phase: "),
          ftxui::text(state.game_phase),
          ftxui::text("  Round: "),
          ftxui::text(std::to_string(state.round)),
      }));
    }

    if (!server && !client) {
      rows.push_back(ftxui::text("No server or client connected.") |
                     ftxui::color(ftxui::Color::GrayLight) | ftxui::center);
    }

    rows.push_back(ftxui::separator());
    rows.push_back(ftxui::text("Games: TopDownShooter · MultiPong · CoopPush "
                               "· Racing") |
                   ftxui::center);

    return ftxui::vbox(rows) | ftxui::border;
  });
}

// ── TopDownShooter
// ────────────────────────────────────────────────────────────

ftxui::Component TopDownShooter(GameOptions opts) {
  struct State {
    std::shared_ptr<MultiplayerServer> server;
    std::shared_ptr<MultiplayerClient> client;
    MultiplayerRenderer renderer{100.0f, 60.0f};
    GameState local_state;
    std::string status = "Initializing...";
    bool keys_up = false, keys_down = false, keys_left = false,
         keys_right = false, keys_fire = false;
    uint64_t frame = 0;
  };
  auto s = std::make_shared<State>();

  if (opts.is_server) {
    MultiplayerServer::Config cfg;
    cfg.port = opts.port;
    cfg.world_width = 100.0f;
    cfg.world_height = 60.0f;
    s->server = std::make_shared<MultiplayerServer>(cfg);

    s->server->OnTick(
        [](GameState& gs, const std::vector<PlayerInput>& inputs) {
          const float dt = 1.0f / 30.0f;
          const float speed = 15.0f;
          const float bullet_speed = 40.0f;
          const float world_w = 100.0f;
          const float world_h = 60.0f;

          gs.game_phase = "playing";

          // Ensure all players have positions
          for (const auto& inp : inputs) {
            int id = inp.player_id;
            if (gs.player_positions.find(id) == gs.player_positions.end()) {
              std::mt19937 rng(static_cast<unsigned>(id * 12345 + 67));
              std::uniform_real_distribution<float> rx(5.0f, world_w - 5.0f);
              std::uniform_real_distribution<float> ry(5.0f, world_h - 5.0f);
              gs.player_positions[id] = {rx(rng), ry(rng)};
              gs.player_velocities[id] = {0, 0};
              gs.player_scores[id] = 0;
              gs.player_colors[id] = PlayerColor(id);
            }
            auto& pos = gs.player_positions[id];
            auto& vel = gs.player_velocities[id];

            // Movement
            vel = {0, 0};
            if (inp.keys.left) {
              vel.x = -speed;
            }
            if (inp.keys.right) {
              vel.x = speed;
            }
            if (inp.keys.up) {
              vel.y = -speed;
            }
            if (inp.keys.down) {
              vel.y = speed;
            }
            pos += vel * dt;
            pos.x = std::max(1.0f, std::min(pos.x, world_w - 1.0f));
            pos.y = std::max(1.0f, std::min(pos.y, world_h - 1.0f));

            // Fire
            if (inp.keys.action1 && (gs.frame % 15 == 0)) {
              Vec2 dir{inp.aim_x, inp.aim_y};
              if (dir.length() < 0.1f) {
                dir = {1.0f, 0.0f};
              }
              dir = dir.normalized();
              gs.projectiles.emplace_back(pos, dir * bullet_speed,
                                          gs.player_colors[id]);
            }
          }

          // Advance projectiles
          std::vector<std::tuple<Vec2, Vec2, ftxui::Color>> live;
          for (auto& [ppos, pvel, pcol] : gs.projectiles) {
            ppos += pvel * dt;
            if (ppos.x >= 0 && ppos.x <= world_w && ppos.y >= 0 &&
                ppos.y <= world_h) {
              // Check hit
              bool hit = false;
              for (auto& [id, plpos] : gs.player_positions) {
                Vec2 diff = plpos - ppos;
                if (diff.length() < 2.0f) {
                  gs.player_scores[id]--;
                  hit = true;
                  break;
                }
              }
              if (!hit) {
                live.emplace_back(ppos, pvel, pcol);
              }
            }
          }
          gs.projectiles = std::move(live);
        });

    s->server->Start();
    s->status = "Server started on port " + std::to_string(opts.port);
  } else {
    MultiplayerClient::Config cfg;
    cfg.host = opts.host;
    cfg.port = opts.port;
    cfg.player_name = opts.player_name;
    cfg.player_color = opts.color;
    s->client = std::make_shared<MultiplayerClient>(cfg);

    s->client->OnStateUpdate([s](const GameState& gs) { s->local_state = gs; });

    if (s->client->Connect()) {
      s->status =
          "Connected as player " + std::to_string(s->client->player_id());
    } else {
      s->status = "Failed to connect: " + s->client->last_error();
    }
  }

  auto component = ftxui::Renderer([s]() -> ftxui::Element {
    const GameState& gs =
        s->server ? s->server->CurrentState() : s->local_state;

    return ftxui::vbox({
               ftxui::hbox({
                   s->renderer.Render(gs) | ftxui::flex,
                   ScorePanel(gs) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 18),
               }) | ftxui::flex,
               ftxui::hbox({
                   ftxui::text("Phase: "),
                   ftxui::text(gs.game_phase),
                   ftxui::text("  Frame: "),
                   ftxui::text(std::to_string(gs.frame)),
                   ftxui::text("  "),
                   ftxui::text(s->status) |
                       ftxui::color(ftxui::Color::GrayLight),
               }) | ftxui::border,
               ftxui::text("WASD: move  Space: fire  Q: quit") | ftxui::center |
                   ftxui::color(ftxui::Color::GrayLight),
           }) |
           ftxui::flex;
  });

  return ftxui::CatchEvent(component, [s](ftxui::Event ev) -> bool {
    if (!s->client) {
      return false;
    }
    bool changed = false;
    if (ev == ftxui::Event::Character('w') || ev == ftxui::Event::ArrowUp) {
      s->keys_up = true;
      changed = true;
    }
    if (ev == ftxui::Event::Character('s') || ev == ftxui::Event::ArrowDown) {
      s->keys_down = true;
      changed = true;
    }
    if (ev == ftxui::Event::Character('a') || ev == ftxui::Event::ArrowLeft) {
      s->keys_left = true;
      changed = true;
    }
    if (ev == ftxui::Event::Character('d') || ev == ftxui::Event::ArrowRight) {
      s->keys_right = true;
      changed = true;
    }
    if (ev == ftxui::Event::Character(' ')) {
      s->keys_fire = true;
      changed = true;
    }
    if (changed) {
      PlayerInput inp;
      inp.player_id = s->client->player_id();
      inp.frame = s->frame++;
      inp.keys.up = s->keys_up;
      inp.keys.down = s->keys_down;
      inp.keys.left = s->keys_left;
      inp.keys.right = s->keys_right;
      inp.keys.action1 = s->keys_fire;
      s->keys_up = s->keys_down = s->keys_left = s->keys_right = s->keys_fire =
          false;
      s->client->SendInput(inp);
      return true;
    }
    return false;
  });
}

// ── MultiPong
// ─────────────────────────────────────────────────────────────────

ftxui::Component MultiPong(GameOptions opts) {
  struct State {
    std::shared_ptr<MultiplayerServer> server;
    std::shared_ptr<MultiplayerClient> client;
    MultiplayerRenderer renderer{100.0f, 60.0f};
    GameState local_state;
    std::string status;
    uint64_t frame = 0;
  };
  auto s = std::make_shared<State>();

  if (opts.is_server) {
    MultiplayerServer::Config cfg;
    cfg.port = opts.port;
    s->server = std::make_shared<MultiplayerServer>(cfg);

    // Ball state stored in projectiles[0]
    s->server->OnTick([](GameState& gs,
                         const std::vector<PlayerInput>& inputs) {
      const float dt = 1.0f / 30.0f;
      const float paddle_speed = 20.0f;
      const float world_w = 100.0f;
      const float world_h = 60.0f;

      gs.game_phase = "playing";

      // Initialize ball if needed
      if (gs.projectiles.empty()) {
        gs.projectiles.emplace_back(Vec2{world_w / 2, world_h / 2},
                                    Vec2{15.0f, 10.0f}, ftxui::Color::White);
      }

      // Initialize paddles
      for (const auto& inp : inputs) {
        int id = inp.player_id;
        if (gs.player_positions.find(id) == gs.player_positions.end()) {
          float x = (id % 2 == 0) ? 5.0f : world_w - 5.0f;
          gs.player_positions[id] = {x, world_h / 2};
          gs.player_velocities[id] = {0, 0};
          gs.player_scores[id] = 0;
          gs.player_colors[id] = PlayerColor(id);
        }

        auto& pos = gs.player_positions[id];
        if (inp.keys.up) {
          pos.y -= paddle_speed * dt;
        }
        if (inp.keys.down) {
          pos.y += paddle_speed * dt;
        }
        pos.y = std::max(3.0f, std::min(pos.y, world_h - 3.0f));
      }

      // Move ball
      auto& [bpos, bvel, bcol] = gs.projectiles[0];
      bpos += bvel * dt;

      // Bounce off top/bottom
      if (bpos.y <= 0 || bpos.y >= world_h) {
        bvel.y = -bvel.y;
        bpos.y = std::max(0.0f, std::min(bpos.y, world_h));
      }

      // Check paddle collisions
      for (auto& [id, ppos] : gs.player_positions) {
        Vec2 diff = bpos - ppos;
        if (std::abs(diff.x) < 3.0f && std::abs(diff.y) < 6.0f) {
          bvel.x = -bvel.x * 1.05f;  // slight speed increase
          bvel.y += diff.y * 0.3f;
        }
      }

      // Score if ball out of bounds
      if (bpos.x < 0 || bpos.x > world_w) {
        // Find which side and award point to opposite player
        for (auto& [id, score] : gs.player_scores) {
          if ((bpos.x < 0 && id % 2 == 1) ||
              (bpos.x > world_w && id % 2 == 0)) {
            score++;
          }
        }
        // Reset ball
        bpos = {world_w / 2, world_h / 2};
        bvel = {(bvel.x > 0 ? -15.0f : 15.0f), 10.0f};
      }
    });

    s->server->Start();
    s->status = "Pong server on port " + std::to_string(opts.port);
  } else {
    MultiplayerClient::Config cfg;
    cfg.host = opts.host;
    cfg.port = opts.port;
    cfg.player_name = opts.player_name;
    s->client = std::make_shared<MultiplayerClient>(cfg);
    s->client->OnStateUpdate([s](const GameState& gs) { s->local_state = gs; });
    if (s->client->Connect()) {
      s->status = "Connected as P" + std::to_string(s->client->player_id());
    } else {
      s->status = "Failed: " + s->client->last_error();
    }
  }

  auto component = ftxui::Renderer([s]() -> ftxui::Element {
    const GameState& gs =
        s->server ? s->server->CurrentState() : s->local_state;
    return ftxui::vbox({
               ftxui::hbox({
                   s->renderer.Render(gs) | ftxui::flex,
                   ScorePanel(gs) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 18),
               }) | ftxui::flex,
               ftxui::text(s->status) | ftxui::center,
               ftxui::text("↑↓: move paddle  Q: quit") | ftxui::center |
                   ftxui::color(ftxui::Color::GrayLight),
           }) |
           ftxui::flex;
  });

  return ftxui::CatchEvent(component, [s](ftxui::Event ev) -> bool {
    if (!s->client) {
      return false;
    }
    PlayerInput inp;
    inp.player_id = s->client->player_id();
    inp.frame = s->frame++;
    bool fire = false;
    if (ev == ftxui::Event::ArrowUp) {
      inp.keys.up = true;
      fire = true;
    }
    if (ev == ftxui::Event::ArrowDown) {
      inp.keys.down = true;
      fire = true;
    }
    if (fire) {
      s->client->SendInput(inp);
      return true;
    }
    return false;
  });
}

// ── CoopPush
// ──────────────────────────────────────────────────────────────────

ftxui::Component CoopPush(GameOptions opts) {
  struct State {
    std::shared_ptr<MultiplayerServer> server;
    std::shared_ptr<MultiplayerClient> client;
    MultiplayerRenderer renderer{100.0f, 60.0f};
    GameState local_state;
    std::string status;
    uint64_t frame = 0;
  };
  auto s = std::make_shared<State>();

  if (opts.is_server) {
    MultiplayerServer::Config cfg;
    cfg.port = opts.port;
    s->server = std::make_shared<MultiplayerServer>(cfg);

    s->server->OnTick(
        [](GameState& gs, const std::vector<PlayerInput>& inputs) {
          const float dt = 1.0f / 30.0f;
          const float player_speed = 15.0f;
          const float world_w = 100.0f;
          const float world_h = 60.0f;
          const Vec2 goal{world_w - 5.0f, world_h / 2};

          gs.game_phase = "playing";

          // Ball in projectiles[0]
          if (gs.projectiles.empty()) {
            gs.projectiles.emplace_back(Vec2{world_w / 2, world_h / 2},
                                        Vec2{0, 0}, ftxui::Color::Yellow);
          }

          for (const auto& inp : inputs) {
            int id = inp.player_id;
            if (gs.player_positions.find(id) == gs.player_positions.end()) {
              std::mt19937 rng(static_cast<unsigned>(id * 98765));
              std::uniform_real_distribution<float> rx(10.0f, 40.0f);
              std::uniform_real_distribution<float> ry(5.0f, world_h - 5.0f);
              gs.player_positions[id] = {rx(rng), ry(rng)};
              gs.player_velocities[id] = {0, 0};
              gs.player_scores[id] = 0;
              gs.player_colors[id] = PlayerColor(id);
            }

            auto& pos = gs.player_positions[id];
            Vec2 vel{0, 0};
            if (inp.keys.left) {
              vel.x = -player_speed;
            }
            if (inp.keys.right) {
              vel.x = player_speed;
            }
            if (inp.keys.up) {
              vel.y = -player_speed;
            }
            if (inp.keys.down) {
              vel.y = player_speed;
            }
            pos += vel * dt;
            pos.x = std::max(1.0f, std::min(pos.x, world_w - 1.0f));
            pos.y = std::max(1.0f, std::min(pos.y, world_h - 1.0f));

            // Push ball on collision
            auto& [bpos, bvel, bcol] = gs.projectiles[0];
            Vec2 diff = bpos - pos;
            if (diff.length() < 4.0f && diff.length() > 0.1f) {
              bvel += diff.normalized() * 5.0f;
            }
          }

          // Update ball
          auto& [bpos, bvel, bcol] = gs.projectiles[0];
          bpos += bvel * dt;
          bvel = bvel * 0.98f;  // friction
          // Bounce walls
          if (bpos.x < 1 || bpos.x > world_w - 1) {
            bvel.x = -bvel.x;
          }
          if (bpos.y < 1 || bpos.y > world_h - 1) {
            bvel.y = -bvel.y;
          }
          bpos.x = std::max(1.0f, std::min(bpos.x, world_w - 1.0f));
          bpos.y = std::max(1.0f, std::min(bpos.y, world_h - 1.0f));

          // Goal check
          Vec2 gdiff = bpos - goal;
          if (gdiff.length() < 5.0f) {
            for (auto& [id, sc] : gs.player_scores) {
              sc++;
            }
            bpos = {world_w / 2, world_h / 2};
            bvel = {0, 0};
            gs.round++;
          }
        });

    s->server->Start();
    s->status = "CoopPush server on port " + std::to_string(opts.port);
  } else {
    MultiplayerClient::Config cfg;
    cfg.host = opts.host;
    cfg.port = opts.port;
    cfg.player_name = opts.player_name;
    s->client = std::make_shared<MultiplayerClient>(cfg);
    s->client->OnStateUpdate([s](const GameState& gs) { s->local_state = gs; });
    if (s->client->Connect()) {
      s->status = "Connected as P" + std::to_string(s->client->player_id());
    } else {
      s->status = "Failed: " + s->client->last_error();
    }
  }

  auto component = ftxui::Renderer([s]() -> ftxui::Element {
    const GameState& gs =
        s->server ? s->server->CurrentState() : s->local_state;
    return ftxui::vbox({
               ftxui::hbox({
                   s->renderer.Render(gs) | ftxui::flex,
                   ScorePanel(gs) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 18),
               }) | ftxui::flex,
               ftxui::text(s->status) | ftxui::center,
               ftxui::text(
                   "WASD: move  Push the ball to the goal! ►  Q: quit") |
                   ftxui::center | ftxui::color(ftxui::Color::GrayLight),
           }) |
           ftxui::flex;
  });

  return ftxui::CatchEvent(component, [s](ftxui::Event ev) -> bool {
    if (!s->client) {
      return false;
    }
    PlayerInput inp;
    inp.player_id = s->client->player_id();
    inp.frame = s->frame++;
    bool fire = false;
    if (ev == ftxui::Event::Character('w') || ev == ftxui::Event::ArrowUp) {
      inp.keys.up = true;
      fire = true;
    }
    if (ev == ftxui::Event::Character('s') || ev == ftxui::Event::ArrowDown) {
      inp.keys.down = true;
      fire = true;
    }
    if (ev == ftxui::Event::Character('a') || ev == ftxui::Event::ArrowLeft) {
      inp.keys.left = true;
      fire = true;
    }
    if (ev == ftxui::Event::Character('d') || ev == ftxui::Event::ArrowRight) {
      inp.keys.right = true;
      fire = true;
    }
    if (fire) {
      s->client->SendInput(inp);
      return true;
    }
    return false;
  });
}

// ── Racing
// ────────────────────────────────────────────────────────────────────

ftxui::Component Racing(GameOptions opts) {
  struct State {
    std::shared_ptr<MultiplayerServer> server;
    std::shared_ptr<MultiplayerClient> client;
    MultiplayerRenderer renderer{100.0f, 60.0f};
    GameState local_state;
    std::string status;
    uint64_t frame = 0;
  };
  auto s = std::make_shared<State>();

  if (opts.is_server) {
    MultiplayerServer::Config cfg;
    cfg.port = opts.port;
    s->server = std::make_shared<MultiplayerServer>(cfg);

    s->server->OnTick(
        [](GameState& gs, const std::vector<PlayerInput>& inputs) {
          const float dt = 1.0f / 30.0f;
          const float max_speed = 30.0f;
          const float accel = 20.0f;
          const float world_w = 100.0f;
          const float world_h = 60.0f;

          gs.game_phase = "playing";

          for (const auto& inp : inputs) {
            int id = inp.player_id;
            if (gs.player_positions.find(id) == gs.player_positions.end()) {
              float lane = static_cast<float>(id) * 7.0f + 10.0f;
              gs.player_positions[id] = {5.0f, lane};
              gs.player_velocities[id] = {0, 0};
              gs.player_scores[id] = 0;
              gs.player_colors[id] = PlayerColor(id);
            }

            auto& pos = gs.player_positions[id];
            auto& vel = gs.player_velocities[id];

            if (inp.keys.up) {
              vel.x += accel * dt;
            }
            if (inp.keys.down) {
              vel.x -= accel * dt * 0.5f;
            }
            if (inp.keys.left) {
              vel.y -= accel * dt;
            }
            if (inp.keys.right) {
              vel.y += accel * dt;
            }

            vel.x = std::max(-5.0f, std::min(vel.x, max_speed));
            vel.y = std::max(-10.0f, std::min(vel.y, 10.0f));
            vel.x *= 0.99f;
            vel.y *= 0.95f;

            pos += vel * dt;
            pos.x = std::max(1.0f, std::min(pos.x, world_w - 1.0f));
            pos.y = std::max(1.0f, std::min(pos.y, world_h - 1.0f));

            // Lap detection (cross x = world_w - 5)
            if (pos.x >= world_w - 5.0f) {
              gs.player_scores[id]++;
              pos.x = 5.0f;
            }
          }
        });

    s->server->Start();
    s->status = "Racing server on port " + std::to_string(opts.port);
  } else {
    MultiplayerClient::Config cfg;
    cfg.host = opts.host;
    cfg.port = opts.port;
    cfg.player_name = opts.player_name;
    s->client = std::make_shared<MultiplayerClient>(cfg);
    s->client->OnStateUpdate([s](const GameState& gs) { s->local_state = gs; });
    if (s->client->Connect()) {
      s->status =
          "Racing: Connected as P" + std::to_string(s->client->player_id());
    } else {
      s->status = "Failed: " + s->client->last_error();
    }
  }

  auto component = ftxui::Renderer([s]() -> ftxui::Element {
    const GameState& gs =
        s->server ? s->server->CurrentState() : s->local_state;
    return ftxui::vbox({
               ftxui::hbox({
                   s->renderer.Render(gs) | ftxui::flex,
                   ScorePanel(gs) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 18),
               }) | ftxui::flex,
               ftxui::text(s->status) | ftxui::center,
               ftxui::text("↑: accelerate  ↓: brake  ←→: steer  Laps = score  "
                           "Q: quit") |
                   ftxui::center | ftxui::color(ftxui::Color::GrayLight),
           }) |
           ftxui::flex;
  });

  return ftxui::CatchEvent(component, [s](ftxui::Event ev) -> bool {
    if (!s->client) {
      return false;
    }
    PlayerInput inp;
    inp.player_id = s->client->player_id();
    inp.frame = s->frame++;
    bool fire = false;
    if (ev == ftxui::Event::ArrowUp) {
      inp.keys.up = true;
      fire = true;
    }
    if (ev == ftxui::Event::ArrowDown) {
      inp.keys.down = true;
      fire = true;
    }
    if (ev == ftxui::Event::ArrowLeft) {
      inp.keys.left = true;
      fire = true;
    }
    if (ev == ftxui::Event::ArrowRight) {
      inp.keys.right = true;
      fire = true;
    }
    if (fire) {
      s->client->SendInput(inp);
      return true;
    }
    return false;
  });
}

}  // namespace MultiGames

}  // namespace ftxui::ui
