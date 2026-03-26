// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_MULTIPLAYER_HPP
#define FTXUI_UI_MULTIPLAYER_HPP

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/json.hpp"
#include "ftxui/ui/physics.hpp"  // Vec2

namespace ftxui::ui {

// ── PlayerInput
// ───────────────────────────────────────────────────────────────
struct PlayerInput {
  int player_id = 0;
  struct {
    bool up = false, down = false, left = false, right = false;
    bool action1 = false, action2 = false;  // fire, jump, etc.
  } keys;
  float aim_x = 0, aim_y = 0;  // mouse/analog aim
  uint64_t frame = 0;          // frame number for reconciliation
};

// ── GameState
// ─────────────────────────────────────────────────────────────────
struct GameState {
  uint64_t frame = 0;
  std::map<int, Vec2> player_positions;
  std::map<int, Vec2> player_velocities;
  std::map<int, int> player_scores;
  std::map<int, std::string> player_names;
  std::map<int, ftxui::Color> player_colors;
  // pos, vel, color
  std::vector<std::tuple<Vec2, Vec2, ftxui::Color>> projectiles;
  std::string game_phase;  // "waiting", "playing", "round_end", "game_over"
  int round = 1;
  float time_remaining = 0;

  JsonValue ToJson() const;
  static GameState FromJson(const JsonValue& j);
};

// ── MultiplayerServer
// ─────────────────────────────────────────────────────────
struct MultiplayerServerConfig {
  int port = 29910;
  int max_players = 8;
  int target_fps = 30;
  float world_width = 100.0f;
  float world_height = 60.0f;
};

struct MultiplayerClientConfig {
  std::string host = "localhost";
  int port = 29910;
  std::string player_name = "Player";
  ftxui::Color player_color = ftxui::Color::Cyan;
};

class MultiplayerServer {
 public:
  using Config = MultiplayerServerConfig;

  explicit MultiplayerServer(Config cfg = {});
  ~MultiplayerServer();

  void Start();
  void Stop();
  bool IsRunning() const;
  int PlayerCount() const;
  int port() const;

  void OnTick(
      std::function<void(GameState&, const std::vector<PlayerInput>&)> fn);
  void BroadcastState(const GameState& state);
  const GameState& CurrentState() const;

 private:
  Config cfg_;
  std::unique_ptr<PhysicsWorld> world_;
  GameState state_;
  std::vector<PlayerInput> pending_inputs_;
  std::thread server_thread_;
  std::thread game_thread_;
  std::atomic<bool> running_{false};
  int listen_sock_{-1};
  std::vector<int> client_sockets_;
  std::mutex clients_mutex_;
  std::mutex inputs_mutex_;
  std::mutex state_mutex_;
  std::function<void(GameState&, const std::vector<PlayerInput>&)> tick_fn_;
  std::atomic<int> player_count_{0};

  void AcceptLoop();
  void GameLoop();
  void ProcessClient(int sock, int player_id);
  void SendToAll(const std::string& msg);
};

// ── MultiplayerClient
// ─────────────────────────────────────────────────────────
class MultiplayerClient {
 public:
  using Config = MultiplayerClientConfig;

  explicit MultiplayerClient(Config cfg = {});
  ~MultiplayerClient();

  bool Connect();
  void Disconnect();
  bool IsConnected() const;
  int player_id() const;

  void SendInput(const PlayerInput& input);
  const GameState& State() const;
  int OnStateUpdate(std::function<void(const GameState&)> fn);
  void RemoveCallback(int id);
  std::string last_error() const;

 private:
  Config cfg_;
  int sock_{-1};
  int player_id_{-1};
  GameState latest_state_;
  std::thread recv_thread_;
  std::atomic<bool> connected_{false};
  std::map<int, std::function<void(const GameState&)>> callbacks_;
  std::mutex state_mutex_;
  std::mutex cb_mutex_;
  std::string last_error_;
  int next_cb_id_{0};

  void RecvLoop();
};

// ── Built-in multiplayer games
// ────────────────────────────────────────────────
namespace MultiGames {

struct GameOptions {
  int port = 29910;
  bool is_server = false;
  std::string host = "localhost";
  std::string player_name = "Player";
  ftxui::Color color = ftxui::Color::Cyan;
};

// Battle Royale pong: last ball standing wins
ftxui::Component MultiPong(GameOptions opts = {});

// Top-down shooter: move + shoot, last player alive wins
ftxui::Component TopDownShooter(GameOptions opts = {});

// Cooperative: all players push a ball to a goal
ftxui::Component CoopPush(GameOptions opts = {});

// Racing: 8 cars on a braille canvas track
ftxui::Component Racing(GameOptions opts = {});

// Lobby screen: shows connected players, ready state, game selection
ftxui::Component Lobby(MultiplayerServer* server = nullptr,
                       MultiplayerClient* client = nullptr);

}  // namespace MultiGames

// ── MultiplayerRenderer
// ───────────────────────────────────────────────────────
class MultiplayerRenderer {
 public:
  explicit MultiplayerRenderer(float world_w, float world_h);

  ftxui::Element Render(const GameState& state) const;
  Vec2 WorldToCanvas(Vec2 world, int canvas_w, int canvas_h) const;

 private:
  float world_w_, world_h_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_MULTIPLAYER_HPP
