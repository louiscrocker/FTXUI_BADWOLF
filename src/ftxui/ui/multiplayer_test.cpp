// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file multiplayer_test.cpp
/// Unit tests for ftxui::ui multiplayer: PlayerInput, GameState,
/// MultiplayerServer, MultiplayerClient, MultiplayerRenderer, MultiGames.

#include "ftxui/ui/multiplayer.hpp"

#include <cmath>
#include <memory>

#include "gtest/gtest.h"

using namespace ftxui::ui;

// ── PlayerInput tests ────────────────────────────────────────────────────────

TEST(PlayerInput, Defaults) {
  PlayerInput inp;
  EXPECT_EQ(inp.player_id, 0);
  EXPECT_FALSE(inp.keys.up);
  EXPECT_FALSE(inp.keys.down);
  EXPECT_FALSE(inp.keys.left);
  EXPECT_FALSE(inp.keys.right);
  EXPECT_FALSE(inp.keys.action1);
  EXPECT_FALSE(inp.keys.action2);
  EXPECT_FLOAT_EQ(inp.aim_x, 0.0f);
  EXPECT_FLOAT_EQ(inp.aim_y, 0.0f);
  EXPECT_EQ(inp.frame, 0u);
}

// ── GameState tests
// ───────────────────────────────────────────────────────────

TEST(GameState, DefaultFrame) {
  GameState gs;
  EXPECT_EQ(gs.frame, 0u);
}

TEST(GameState, DefaultPhase) {
  GameState gs;
  EXPECT_TRUE(gs.game_phase.empty());
}

TEST(GameState, ToJsonReturnsObject) {
  GameState gs;
  gs.frame = 42;
  gs.game_phase = "playing";
  auto j = gs.ToJson();
  EXPECT_TRUE(j.is_object());
}

TEST(GameState, FromJsonRoundTrip) {
  GameState orig;
  orig.frame = 99;
  orig.game_phase = "round_end";
  orig.round = 3;
  orig.time_remaining = 12.5f;
  orig.player_scores[0] = 5;
  orig.player_scores[1] = 3;
  orig.player_names[0] = "Alice";
  orig.player_names[1] = "Bob";
  orig.player_positions[0] = {10.0f, 20.0f};
  orig.player_positions[1] = {50.0f, 30.0f};

  auto j = orig.ToJson();
  auto restored = GameState::FromJson(j);

  EXPECT_EQ(restored.frame, orig.frame);
  EXPECT_EQ(restored.game_phase, orig.game_phase);
  EXPECT_EQ(restored.round, orig.round);
  EXPECT_NEAR(restored.time_remaining, orig.time_remaining, 0.01f);
  EXPECT_EQ(restored.player_scores[0], 5);
  EXPECT_EQ(restored.player_scores[1], 3);
  EXPECT_EQ(restored.player_names[0], "Alice");
  EXPECT_EQ(restored.player_names[1], "Bob");
  EXPECT_NEAR(restored.player_positions[0].x, 10.0f, 0.001f);
  EXPECT_NEAR(restored.player_positions[0].y, 20.0f, 0.001f);
}

TEST(GameState, PlayerPositionsSerialization) {
  GameState gs;
  gs.player_positions[0] = {1.5f, 2.5f};
  gs.player_positions[2] = {99.0f, 55.0f};
  auto j = gs.ToJson();
  auto r = GameState::FromJson(j);
  ASSERT_EQ(r.player_positions.size(), 2u);
  EXPECT_NEAR(r.player_positions[0].x, 1.5f, 0.001f);
  EXPECT_NEAR(r.player_positions[2].y, 55.0f, 0.001f);
}

TEST(GameState, ProjectilesSerialization) {
  GameState gs;
  gs.projectiles.emplace_back(Vec2{10.0f, 20.0f}, Vec2{5.0f, -3.0f},
                              ftxui::Color::Red);
  gs.projectiles.emplace_back(Vec2{50.0f, 30.0f}, Vec2{-2.0f, 1.0f},
                              ftxui::Color::Cyan);
  auto j = gs.ToJson();
  auto r = GameState::FromJson(j);
  ASSERT_EQ(r.projectiles.size(), 2u);
  EXPECT_NEAR(std::get<0>(r.projectiles[0]).x, 10.0f, 0.001f);
  EXPECT_NEAR(std::get<1>(r.projectiles[0]).x, 5.0f, 0.001f);
  EXPECT_NEAR(std::get<0>(r.projectiles[1]).y, 30.0f, 0.001f);
}

// ── Vec2 operations (via multiplayer's use of physics Vec2) ──────────────────

TEST(Vec2Multiplayer, AdditionPreserved) {
  Vec2 a{3.0f, 4.0f};
  Vec2 b{1.0f, -1.0f};
  Vec2 c = a + b;
  EXPECT_FLOAT_EQ(c.x, 4.0f);
  EXPECT_FLOAT_EQ(c.y, 3.0f);
}

TEST(Vec2Multiplayer, ScalarMultiply) {
  Vec2 a{2.0f, 5.0f};
  Vec2 b = a * 3.0f;
  EXPECT_FLOAT_EQ(b.x, 6.0f);
  EXPECT_FLOAT_EQ(b.y, 15.0f);
}

// ── MultiplayerServer tests
// ───────────────────────────────────────────────────

TEST(MultiplayerServer, ConstructsWithoutCrash) {
  MultiplayerServer::Config cfg;
  cfg.port = 29910;
  EXPECT_NO_THROW({ MultiplayerServer server(cfg); });
}

TEST(MultiplayerServer, PortReturnsConfigured) {
  MultiplayerServer::Config cfg;
  cfg.port = 29910;
  MultiplayerServer server(cfg);
  EXPECT_EQ(server.port(), 29910);
}

TEST(MultiplayerServer, NotRunningInitially) {
  MultiplayerServer server;
  EXPECT_FALSE(server.IsRunning());
}

TEST(MultiplayerServer, PlayerCountZeroInitially) {
  MultiplayerServer server;
  EXPECT_EQ(server.PlayerCount(), 0);
}

TEST(MultiplayerServer, StartStop) {
  MultiplayerServer::Config cfg;
  cfg.port = 29911;  // use a separate port
  MultiplayerServer server(cfg);
  server.Start();
  if (!server.IsRunning()) {
    GTEST_SKIP() << "TCP server could not bind (likely port in use or no TCP)";
  }
  EXPECT_TRUE(server.IsRunning());
  server.Stop();
  EXPECT_FALSE(server.IsRunning());
}

TEST(MultiplayerServer, CurrentStateDefaultFrame) {
  MultiplayerServer server;
  EXPECT_EQ(server.CurrentState().frame, 0u);
}

// ── MultiplayerClient tests
// ───────────────────────────────────────────────────

TEST(MultiplayerClient, ConstructsWithoutCrash) {
  MultiplayerClient::Config cfg;
  cfg.host = "localhost";
  cfg.port = 29910;
  EXPECT_NO_THROW({ MultiplayerClient client(cfg); });
}

TEST(MultiplayerClient, NotConnectedInitially) {
  MultiplayerClient client;
  EXPECT_FALSE(client.IsConnected());
}

TEST(MultiplayerClient, ConnectToInvalidHostReturnsFalse) {
  MultiplayerClient::Config cfg;
  cfg.host = "invalid.host.badwolf.test";
  cfg.port = 29910;
  MultiplayerClient client(cfg);
  bool ok = client.Connect();
  EXPECT_FALSE(ok);
}

TEST(MultiplayerClient, LastErrorSetOnFailure) {
  MultiplayerClient::Config cfg;
  cfg.host = "127.0.0.1";
  cfg.port = 29909;  // nothing listening
  MultiplayerClient client(cfg);
  bool ok = client.Connect();
  EXPECT_FALSE(ok);
  EXPECT_FALSE(client.last_error().empty());
}

TEST(MultiplayerClient, PlayerIdNegativeBeforeConnect) {
  MultiplayerClient client;
  EXPECT_LT(client.player_id(), 0);
}

TEST(MultiplayerClient, OnStateUpdateRegistersCallback) {
  MultiplayerClient client;
  bool called = false;
  int id = client.OnStateUpdate([&](const GameState&) { called = true; });
  EXPECT_GE(id, 0);
  // Callback not called until state update arrives — just checks registration
  EXPECT_FALSE(called);
  client.RemoveCallback(id);
}

// ── MultiplayerRenderer tests
// ─────────────────────────────────────────────────

TEST(MultiplayerRenderer, ConstructsWithoutCrash) {
  EXPECT_NO_THROW({ MultiplayerRenderer r(100.0f, 60.0f); });
}

TEST(MultiplayerRenderer, WorldToCanvasTransform) {
  MultiplayerRenderer r(100.0f, 60.0f);
  // Center of world → center of canvas
  Vec2 c = r.WorldToCanvas({50.0f, 30.0f}, 200, 120);
  EXPECT_NEAR(c.x, 100.0f, 0.5f);
  EXPECT_NEAR(c.y, 60.0f, 0.5f);
}

TEST(MultiplayerRenderer, WorldToCanvasOrigin) {
  MultiplayerRenderer r(100.0f, 60.0f);
  Vec2 c = r.WorldToCanvas({0.0f, 0.0f}, 100, 60);
  EXPECT_NEAR(c.x, 0.0f, 0.001f);
  EXPECT_NEAR(c.y, 0.0f, 0.001f);
}

TEST(MultiplayerRenderer, RenderEmptyStateReturnsElement) {
  MultiplayerRenderer r(100.0f, 60.0f);
  GameState gs;
  // Should not crash even with empty state
  EXPECT_NO_THROW({
    auto elem = r.Render(gs);
    EXPECT_NE(elem, nullptr);
  });
}

// ── MultiGames::Lobby
// ─────────────────────────────────────────────────────────

TEST(MultiGames, LobbyBuildsWithoutCrash) {
  EXPECT_NO_THROW({
    auto lobby = MultiGames::Lobby(nullptr, nullptr);
    EXPECT_NE(lobby, nullptr);
  });
}
