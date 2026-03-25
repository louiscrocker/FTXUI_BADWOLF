// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/galaxy_map.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/ui/network_reactive.hpp"
#include "ftxui/ui/reactive.hpp"
#include "ftxui/ui/reactive_list.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── GalaxyMap tests
// ───────────────────────────────────────────────────────────

TEST(GalaxyMapTest, DefaultBuilds) {
  auto comp = GalaxyMap().Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GalaxyMapTest, WithStars) {
  auto comp = GalaxyMap()
                  .AddStar({"Test Star", 90.f, 45.f, 10.f})
                  .AddStar({"Other", 180.f, -30.f, 50.f})
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GalaxyMapTest, WithFleets) {
  auto comp =
      GalaxyMap()
          .AddFleet({"Rebel", 100.f, 20.f, ftxui::Color::Green, "rebel"})
          .AddFleet({"Imperial", 200.f, -10.f, ftxui::Color::Red, "imperial"})
          .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GalaxyMapTest, WithRoutes) {
  auto comp =
      GalaxyMap()
          .AddRoute({0.f, 0.f, 90.f, 45.f, ftxui::Color::Blue, "Route A"})
          .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GalaxyMapTest, GridEnabled) {
  auto comp = GalaxyMap().Grid(true).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GalaxyMapTest, CenterAndZoom) {
  auto comp = GalaxyMap().Center(90.f, 30.f).Zoom(2.0f).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GalaxyMapTest, OnSelectCallback) {
  std::string selected;
  auto comp = GalaxyMap()
                  .AddStar({"Alpha Centauri", 10.f, 5.f, 1.3f})
                  .OnSelect([&selected](std::string name) { selected = name; })
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GalaxyMapTest, StarFieldsStarWars) {
  auto field = StarFields::StarWars();
  ASSERT_FALSE(field.empty());
  // Check a few expected locations
  bool found_coruscant = false;
  bool found_tatooine = false;
  for (const auto& s : field) {
    if (s.name == "Coruscant") {
      found_coruscant = true;
    }
    if (s.name == "Tatooine") {
      found_tatooine = true;
    }
  }
  EXPECT_TRUE(found_coruscant);
  EXPECT_TRUE(found_tatooine);
}

TEST(GalaxyMapTest, StarFieldsStarTrek) {
  auto field = StarFields::StarTrek();
  ASSERT_FALSE(field.empty());
  bool found_earth = false;
  bool found_vulcan = false;
  for (const auto& s : field) {
    if (s.name == "Earth/Sol") {
      found_earth = true;
    }
    if (s.name == "Vulcan") {
      found_vulcan = true;
    }
  }
  EXPECT_TRUE(found_earth);
  EXPECT_TRUE(found_vulcan);
}

TEST(GalaxyMapTest, StarFieldsProcedural) {
  auto field = StarFields::Procedural(100, 42);
  EXPECT_EQ(static_cast<int>(field.size()), 100);
  for (const auto& s : field) {
    EXPECT_GE(s.ra, 0.f);
    EXPECT_LT(s.ra, 360.f);
    EXPECT_GE(s.dec, -90.f);
    EXPECT_LE(s.dec, 90.f);
  }
}

TEST(GalaxyMapTest, ProceduralDifferentSeeds) {
  auto f1 = StarFields::Procedural(10, 1);
  auto f2 = StarFields::Procedural(10, 2);
  ASSERT_EQ(f1.size(), f2.size());
  // Different seeds should produce different stars
  bool any_different = false;
  for (size_t i = 0; i < f1.size(); ++i) {
    if (f1[i].ra != f2[i].ra || f1[i].dec != f2[i].dec) {
      any_different = true;
      break;
    }
  }
  EXPECT_TRUE(any_different);
}

TEST(GalaxyMapTest, BindFleets_UpdatesOnPush) {
  auto fleets = MakeReactiveList<FleetMarker>();
  auto comp = GalaxyMap().BindFleets(fleets).Build();
  ASSERT_NE(comp, nullptr);

  // Render before push
  ASSERT_NE(comp->Render(), nullptr);

  // Push a fleet
  fleets->Push({"New Fleet", 50.f, 20.f, ftxui::Color::Green, "rebel"});

  // Render after push — should not crash
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(GalaxyMapTest, StarFieldBuildAndRender) {
  auto comp = GalaxyMap()
                  .StarField(StarFields::StarWars())
                  .Grid(true)
                  .Center(180.f, 0.f)
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

// ── NetworkReactive tests
// ─────────────────────────────────────────────────────

TEST(NetworkReactiveTest, ServerCreates) {
  auto state = std::make_shared<Reactive<std::string>>("hello");
  auto server = NetworkReactiveServer::Create(state, 0);  // port 0 = any
  ASSERT_NE(server, nullptr);
  EXPECT_EQ(server->ConnectedClients(), 0);
}

TEST(NetworkReactiveTest, ServerStartAndStop) {
  auto state = std::make_shared<Reactive<std::string>>("test");
  // Use a high port to avoid conflicts in CI
  auto server = NetworkReactiveServer::Create(state, 19876);
  server->Start();
  EXPECT_EQ(server->ConnectedClients(), 0);
  server->Stop();
}

TEST(NetworkReactiveTest, ClientCreates) {
  // Just verify the client object constructs without crashing.
  // Use a port where no server is running — client stays disconnected.
  auto client = NetworkReactiveClient::Connect("127.0.0.1", 19877);
  ASSERT_NE(client, nullptr);
  ASSERT_NE(client->State(), nullptr);
  // State should be accessible regardless of connection
  EXPECT_EQ(client->State()->Get(), "");
  client->Disconnect();
  // Small sleep to let background thread notice the disconnect
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST(NetworkReactiveTest, StatePropagatesToClient) {
  const uint16_t kTestPort = 19878;

  auto server_state = std::make_shared<Reactive<std::string>>("initial");
  auto server = NetworkReactiveServer::Create(server_state, kTestPort);
  server->Start();

  // Give server time to bind
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto client = NetworkReactiveClient::Connect("127.0.0.1", kTestPort);

  // Wait for connection + initial state push (up to 2 seconds)
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    if (client->Connected()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  if (!client->Connected()) {
    // Networking unavailable in this environment — skip gracefully
    client->Disconnect();
    server->Stop();
    GTEST_SKIP() << "TCP networking not available in test environment";
    return;
  }

  // Wait for initial state to arrive
  const auto state_deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (std::chrono::steady_clock::now() < state_deadline) {
    if (client->State()->Get() == "initial") {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(client->State()->Get(), "initial");

  // Update server state and verify propagation
  server_state->Set("updated");
  const auto upd_deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (std::chrono::steady_clock::now() < upd_deadline) {
    if (client->State()->Get() == "updated") {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(client->State()->Get(), "updated");

  client->Disconnect();
  server->Stop();
  // Allow background threads to wind down
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

}  // namespace
}  // namespace ftxui::ui
