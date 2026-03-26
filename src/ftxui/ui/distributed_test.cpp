// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/distributed.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── RedisConfig defaults ─────────────────────────────────────────────────────

TEST(RedisConfigTest, DefaultHost) {
  RedisConfig cfg;
  EXPECT_EQ(cfg.host, "localhost");
}

TEST(RedisConfigTest, DefaultPort) {
  RedisConfig cfg;
  EXPECT_EQ(cfg.port, 6379);
}

TEST(RedisConfigTest, DefaultDb) {
  RedisConfig cfg;
  EXPECT_EQ(cfg.db, 0);
}

TEST(RedisConfigTest, DefaultMaxRetries) {
  RedisConfig cfg;
  EXPECT_EQ(cfg.max_retries, -1);
}

// ── RedisSource::BuildCommand
// ─────────────────────────────────────────────────

TEST(RedisSourceTest, BuildCommandSimple) {
  auto cmd = RedisSource<>::BuildCommand({"PING"});
  EXPECT_EQ(cmd, "*1\r\n$4\r\nPING\r\n");
}

TEST(RedisSourceTest, BuildCommandMultiArg) {
  auto cmd = RedisSource<>::BuildCommand({"SUBSCRIBE", "mychannel"});
  EXPECT_EQ(cmd, "*2\r\n$9\r\nSUBSCRIBE\r\n$9\r\nmychannel\r\n");
}

// ── RedisSource::ParseResp
// ────────────────────────────────────────────────────

TEST(RedisSourceTest, ParseRespSimpleString) {
  auto result = RedisSource<>::ParseResp("+OK\r\n");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], "OK");
}

TEST(RedisSourceTest, ParseRespBulkString) {
  auto result = RedisSource<>::ParseResp("$5\r\nhello\r\n");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], "hello");
}

TEST(RedisSourceTest, ParseRespInteger) {
  auto result = RedisSource<>::ParseResp(":42\r\n");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], "42");
}

TEST(RedisSourceTest, ParseRespArray) {
  auto result = RedisSource<>::ParseResp("*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "foo");
  EXPECT_EQ(result[1], "bar");
}

// ── RedisSource construction
// ──────────────────────────────────────────────────

TEST(RedisSourceTest, SubscribeConstructsWithoutCrash) {
  auto src = RedisSource<std::string>::Subscribe("test-channel");
  EXPECT_NE(src, nullptr);
  EXPECT_FALSE(src->IsRunning());
}

TEST(RedisSourceTest, PollKeyConstructsWithoutCrash) {
  auto src = RedisSource<std::string>::PollKey("test-key");
  EXPECT_NE(src, nullptr);
  EXPECT_FALSE(src->IsRunning());
}

TEST(RedisSourceTest, ListenQueueConstructsWithoutCrash) {
  auto src = RedisSource<std::string>::ListenQueue("test-list");
  EXPECT_NE(src, nullptr);
  EXPECT_FALSE(src->IsRunning());
}

TEST(RedisSourceTest, ConnectToInvalidHostFailsGracefully) {
  RedisConfig cfg;
  cfg.host = "192.0.2.1";  // TEST-NET, should not be reachable
  cfg.port = 6379;
  cfg.connect_timeout = std::chrono::milliseconds(200);
  cfg.reconnect_delay = std::chrono::milliseconds(100);
  cfg.max_retries = 0;

  auto src = RedisSource<std::string>::Subscribe("channel", cfg);
  src->Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  src->Stop();
  EXPECT_FALSE(src->IsConnected());
}

// ── KafkaConfig defaults
// ──────────────────────────────────────────────────────

TEST(KafkaConfigTest, DefaultBrokers) {
  KafkaConfig cfg;
  EXPECT_EQ(cfg.brokers, "localhost:9092");
}

TEST(KafkaConfigTest, DefaultGroupId) {
  KafkaConfig cfg;
  EXPECT_EQ(cfg.group_id, "badwolf");
}

// ── KafkaSource
// ───────────────────────────────────────────────────────────────

TEST(KafkaSourceTest, ConstructsWithoutCrash) {
  KafkaSource src("test-topic");
  EXPECT_FALSE(src.IsRunning());
}

TEST(KafkaSourceTest, BuildFetchRequestNonEmpty) {
  KafkaSource src("test-topic");
  auto req = src.BuildFetchRequest(0);
  EXPECT_FALSE(req.empty());
  EXPECT_GE(req.size(), 4u);
}

TEST(KafkaSourceTest, ConnectToInvalidBrokerFailsGracefully) {
  KafkaConfig cfg;
  cfg.brokers = "192.0.2.1:9092";
  cfg.reconnect_delay = std::chrono::milliseconds(100);
  KafkaSource src("test-topic", cfg);
  src.Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  src.Stop();
  EXPECT_FALSE(src.IsConnected());
}

// ── GrpcStreamConfig defaults
// ─────────────────────────────────────────────────

TEST(GrpcStreamConfigTest, DefaultHost) {
  GrpcStreamConfig cfg;
  EXPECT_EQ(cfg.host, "localhost");
}

TEST(GrpcStreamConfigTest, DefaultPort) {
  GrpcStreamConfig cfg;
  EXPECT_EQ(cfg.port, 50051);
}

// ── GrpcStreamSource
// ──────────────────────────────────────────────────────────

TEST(GrpcStreamSourceTest, ConstructsWithoutCrash) {
  GrpcStreamSource src;
  EXPECT_FALSE(src.IsRunning());
}

// ── DistributedState
// ──────────────────────────────────────────────────────────

TEST(DistributedStateTest, ConstructsWithSource) {
  auto src = RedisSource<std::string>::Subscribe("test");
  DistributedState<std::string> state("my-key", src, "initial");
  EXPECT_EQ(state.key(), "my-key");
  src->Stop();
}

TEST(DistributedStateTest, GetReturnsInitialValue) {
  auto src = RedisSource<std::string>::Subscribe("test");
  DistributedState<std::string> state("key", src, "hello");
  EXPECT_EQ(state.Get(), "hello");
  src->Stop();
}

// ── DistributedDashboard
// ──────────────────────────────────────────────────────

TEST(DistributedDashboardTest, Constructs) {
  DistributedDashboard dashboard;
  (void)dashboard;
}

TEST(DistributedDashboardTest, BuildReturnsNonNull) {
  DistributedDashboard dashboard;
  auto src = RedisSource<std::string>::Subscribe("test");
  dashboard.AddSource("Test Source", src);
  auto component = dashboard.Build();
  EXPECT_NE(component, nullptr);
  src->Stop();
}

}  // namespace
}  // namespace ftxui::ui
