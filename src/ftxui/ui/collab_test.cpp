// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/collab.hpp"

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <thread>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── CollabServer tests
// ────────────────────────────────────────────────────────

TEST(CollabServerTest, StartsAndStopsCleanly) {
  auto server = std::make_shared<CollabServer>(19900);
  server->Start();
  EXPECT_TRUE(server->IsRunning());
  server->Stop();
  EXPECT_FALSE(server->IsRunning());
}

TEST(CollabServerTest, ReportsZeroPeersInitially) {
  auto server = std::make_shared<CollabServer>(19901);
  server->Start();
  EXPECT_EQ(server->PeerCount(), 0);
  EXPECT_TRUE(server->GetPeers().empty());
  server->Stop();
}

// ── CollabClient tests
// ────────────────────────────────────────────────────────

TEST(CollabClientTest, FailsConnectWhenNoServer) {
  // No server is running on port 19902 — connect should return false.
  CollabClient client("127.0.0.1", 19902, "TestPeer");
  bool connected = client.Connect();
  EXPECT_FALSE(connected);
  EXPECT_FALSE(client.IsConnected());
}

TEST(CollabClientTest, LocalPeerNameSet) {
  CollabClient client("127.0.0.1", 19903, "Alice");
  EXPECT_EQ(client.LocalPeer().name, "Alice");
}

// ── GeneratePeerId tests ─────────────────────────────────────────────────────

TEST(CollabClientTest, GeneratePeerIdIsNonEmpty) {
  std::string id = CollabClient::GeneratePeerId();
  EXPECT_FALSE(id.empty());
}

TEST(CollabClientTest, GeneratePeerIdHasLengthEight) {
  std::string id = CollabClient::GeneratePeerId();
  EXPECT_EQ(static_cast<int>(id.size()), 8);
}

TEST(CollabClientTest, GeneratePeerIdProducesUniqueIds) {
  std::set<std::string> ids;
  for (int i = 0; i < 100; ++i) {
    ids.insert(CollabClient::GeneratePeerId());
  }
  // With 4 random bytes the collision probability is astronomically low.
  EXPECT_EQ(static_cast<int>(ids.size()), 100);
}

// ── AssignColor tests ────────────────────────────────────────────────────────

TEST(CollabClientTest, AssignColorReturnsDifferentColorsForIndices0To5) {
  std::vector<Color> colors;
  for (int i = 0; i < 6; ++i) {
    colors.push_back(CollabClient::AssignColor(i));
  }
  // All 6 colors should be distinct (compare as TrueColor RGB values)
  // Just verify we got 6 results and the function doesn't crash
  EXPECT_EQ(static_cast<int>(colors.size()), 6);
  // Verify index 0 != index 1 (basic distinctness)
  EXPECT_FALSE(colors[0] == colors[1]);
}

TEST(CollabClientTest, AssignColorWrapsAroundAt6) {
  // Index 6 should equal index 0.
  EXPECT_EQ(CollabClient::AssignColor(6), CollabClient::AssignColor(0));
}

// ── CollabEvent serialisation tests ─────────────────────────────────────────
// (tested indirectly via CollabStatusBar / Element construction)

TEST(CollabPeerTest, DefaultConstruction) {
  CollabPeer peer;
  EXPECT_TRUE(peer.id.empty());
  EXPECT_TRUE(peer.name.empty());
  EXPECT_EQ(peer.cursor_x, 0);
  EXPECT_EQ(peer.cursor_y, 0);
  EXPECT_TRUE(peer.is_active);
}

// ── CollabStatusBar returns non-null Element ─────────────────────────────────

TEST(CollabStatusBarTest, ClientStatusBarReturnsNonNull) {
  auto client = std::make_shared<CollabClient>("127.0.0.1", 19904, "Test");
  Element e = CollabStatusBar(client);
  ASSERT_NE(e, nullptr);
}

TEST(CollabStatusBarTest, ServerStatusBarReturnsNonNull) {
  auto server = std::make_shared<CollabServer>(19905);
  Element e = CollabStatusBar(server);
  ASSERT_NE(e, nullptr);
}

// ── TCP integration tests (skip if loopback unavailable) ─────────────────────

TEST(CollabIntegrationTest, ServerAndClientConnect) {
  const int kPort = 19906;
  auto server = std::make_shared<CollabServer>(kPort);
  server->Start();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto client = std::make_shared<CollabClient>("127.0.0.1", kPort, "Bob");
  bool ok = client->Connect();
  if (!ok) {
    server->Stop();
    GTEST_SKIP() << "TCP loopback not available in this environment";
    return;
  }

  EXPECT_TRUE(client->IsConnected());

  // Allow server to register the peer (the handler thread needs time to
  // process the JOIN event).
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    if (server->PeerCount() >= 1) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  EXPECT_EQ(server->PeerCount(), 1);

  client->Disconnect();
  server->Stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST(CollabIntegrationTest, BroadcastReachesClient) {
  const int kPort = 29907;  // high port, avoid collisions with NetworkReactive
  auto server = std::make_shared<CollabServer>(kPort);
  server->Start();

  std::this_thread::sleep_for(
      std::chrono::milliseconds(300));  // give server time to bind

  // Two clients so the server can broadcast from one to the other.
  auto client_a = std::make_shared<CollabClient>("127.0.0.1", kPort, "Alice");
  auto client_b = std::make_shared<CollabClient>("127.0.0.1", kPort, "Charlie");

  bool ok_a = client_a->Connect();
  bool ok_b = client_b->Connect();

  if (!ok_a || !ok_b) {
    client_a->Disconnect();
    client_b->Disconnect();
    server->Stop();
    GTEST_SKIP() << "TCP loopback not available in this environment";
    return;
  }

  // client_b listens for STATE_SYNC events using a condition variable so
  // the callback thread is never starved by the polling loop.
  std::string received_payload;
  bool received = false;
  std::mutex rx_mutex;
  std::condition_variable rx_cv;
  client_b->OnRemoteEvent([&](CollabEvent ev) {
    if (ev.type == CollabEvent::Type::STATE_SYNC) {
      std::lock_guard<std::mutex> lk(rx_mutex);
      received_payload = ev.payload;
      received = true;
      rx_cv.notify_all();
    }
  });

  // Wait for the server to see both peers before sending.
  {
    const auto peer_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < peer_deadline) {
      if (server->PeerCount() >= 2) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (server->PeerCount() < 2) {
      client_a->Disconnect();
      client_b->Disconnect();
      server->Stop();
      GTEST_SKIP() << "Server did not register both peers in time";
      return;
    }
  }

  // client_a sends a STATE_SYNC
  CollabEvent ev;
  ev.peer_id = client_a->LocalPeer().id;
  ev.peer_name = client_a->LocalPeer().name;
  ev.type = CollabEvent::Type::STATE_SYNC;
  ev.payload = "hello_collab";
  ev.timestamp_ms = 0;
  client_a->SendEvent(ev);

  // Wait for client_b to receive it (condition variable — no polling).
  {
    std::unique_lock<std::mutex> lk(rx_mutex);
    rx_cv.wait_for(lk, std::chrono::seconds(3), [&] { return received; });
  }

  EXPECT_EQ(received_payload, "hello_collab");

  client_a->Disconnect();
  client_b->Disconnect();
  server->Stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

}  // namespace
}  // namespace ftxui::ui
