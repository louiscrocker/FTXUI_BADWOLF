// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
//
// Distributed State Demo — Redis, Kafka, gRPC reactive sources.
// Run with a Redis server on localhost:6379 for live PubSub.
// Without a server, the demo shows "Unavailable" status gracefully.

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/distributed.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Status badge helper
// ───────────────────────────────────────────────────────
Element StatusBadge(bool connected, bool started) {
  if (!started) {
    return text(" ◌ Not Started ") | color(Color::GrayLight);
  }
  if (connected) {
    return text(" ● Connected ") | color(Color::Green) | bold;
  }
  return text(" ○ Disconnected ") | color(Color::Red);
}

// ── Shared log buffer
// ─────────────────────────────────────────────────────────
struct LogBuffer {
  std::mutex mtx;
  std::deque<std::string> lines;
  int count{0};
  void Push(const std::string& s) {
    std::lock_guard<std::mutex> lock(mtx);
    lines.push_back(s);
    count++;
    if (lines.size() > 50) {
      lines.pop_front();
    }
  }
  std::vector<std::string> Snapshot() {
    std::lock_guard<std::mutex> lock(mtx);
    return {lines.begin(), lines.end()};
  }
  int Count() {
    std::lock_guard<std::mutex> lock(mtx);
    return count;
  }
};

// ── Tab 1: Redis PubSub
// ───────────────────────────────────────────────────────
Component MakeRedisPubSubTab(std::shared_ptr<RedisSource<std::string>> src,
                             std::shared_ptr<LogBuffer> log) {
  src->OnData([log](const std::string& msg) { log->Push(msg); });
  src->Start();

  return Renderer([src, log] {
    auto msgs = log->Snapshot();
    std::vector<Element> lines;
    for (const auto& m : msgs) {
      lines.push_back(text(m));
    }
    if (lines.empty()) {
      lines.push_back(text("Waiting for messages on badwolf:events...") |
                      color(Color::GrayLight) | italic);
    }

    return vbox({
               hbox({text("Redis PubSub") | bold, filler(),
                     StatusBadge(src->IsConnected(), true),
                     text(" msgs: " + std::to_string(log->Count())) |
                         color(Color::Yellow)}),
               separator(),
               text("Channel: badwolf:events") | color(Color::GrayLight),
               text("Publish test: redis-cli PUBLISH badwolf:events 'hello'") |
                   color(Color::GrayLight) | italic,
               separator(),
               vbox(lines) | yframe | flex,
               separator(),
               hbox({text("Error: "),
                     text(src->last_error()) | color(Color::Red)}),
           }) |
           border;
  });
}

// ── Tab 2: Redis Key Poll
// ─────────────────────────────────────────────────────
Component MakeRedisPollTab(std::shared_ptr<RedisSource<std::string>> src,
                           std::shared_ptr<LogBuffer> log) {
  auto latest_val = std::make_shared<std::string>();
  auto val_mtx = std::make_shared<std::mutex>();

  src->OnData([log, latest_val, val_mtx](const std::string& val) {
    log->Push(val);
    std::lock_guard<std::mutex> lock(*val_mtx);
    *latest_val = val;
  });
  src->Start();

  return Renderer([src, log, latest_val, val_mtx] {
    std::string cur_val;
    {
      std::lock_guard<std::mutex> lock(*val_mtx);
      cur_val = *latest_val;
    }
    auto msgs = log->Snapshot();
    std::vector<Element> lines;
    for (auto it = msgs.rbegin(); it != msgs.rend() && lines.size() < 5; ++it) {
      lines.push_back(text(*it));
    }

    Element value_display;
    if (!cur_val.empty()) {
      value_display = hbox(
          {text("Current value: ") | bold, text(cur_val) | color(Color::Cyan)});
    } else {
      value_display = text("Set: redis-cli SET badwolf:counter 42") |
                      color(Color::GrayLight) | italic;
    }

    return vbox({
               hbox({text("Redis Key Poll") | bold, filler(),
                     StatusBadge(src->IsConnected(), true),
                     text(" polls: " + std::to_string(log->Count())) |
                         color(Color::Yellow)}),
               separator(),
               text("Key: badwolf:counter  (500ms interval)") |
                   color(Color::GrayLight),
               separator(),
               value_display,
               separator(),
               vbox(lines),
           }) |
           border;
  });
}

// ── Tab 3: Kafka
// ──────────────────────────────────────────────────────────────
Component MakeKafkaTab(std::shared_ptr<KafkaSource> src,
                       std::shared_ptr<LogBuffer> log) {
  src->OnData([log](const std::string& msg) { log->Push(msg); });
  src->Start();

  return Renderer([src, log] {
    auto msgs = log->Snapshot();
    std::vector<Element> lines;
    for (const auto& m : msgs) {
      lines.push_back(text(m));
    }
    if (lines.empty()) {
      lines.push_back(text("Waiting for messages on badwolf-topic...") |
                      color(Color::GrayLight) | italic);
    }

    return vbox({
               hbox({text("Kafka Consumer") | bold, filler(),
                     StatusBadge(src->IsConnected(), true),
                     text(" msgs: " + std::to_string(log->Count())) |
                         color(Color::Yellow)}),
               separator(),
               text("Topic: badwolf-topic  Broker: localhost:9092") |
                   color(Color::GrayLight),
               separator(),
               vbox(lines) | yframe | flex,
               separator(),
               hbox({text("Error: "),
                     text(src->last_error()) | color(Color::Red)}),
           }) |
           border;
  });
}

// ── Tab 4: gRPC Stream
// ────────────────────────────────────────────────────────
Component MakeGrpcTab(std::shared_ptr<GrpcStreamSource> src,
                      std::shared_ptr<LogBuffer> log) {
  src->OnData([log](const JsonValue& val) {
    if (!val.is_null()) {
      log->Push(val.is_string() ? val.as_string() : "(json frame)");
    }
  });
  src->Start();

  return Renderer([src, log] {
    auto msgs = log->Snapshot();
    std::vector<Element> lines;
    for (const auto& m : msgs) {
      lines.push_back(text(m));
    }
    if (lines.empty()) {
      lines.push_back(text("Waiting for gRPC stream...") |
                      color(Color::GrayLight) | italic);
      lines.push_back(text("Needs: grpc-gateway or gRPC-Web server") |
                      color(Color::GrayLight) | italic);
    }
    return vbox({
               hbox({text("gRPC Stream") | bold, filler(),
                     StatusBadge(false, true),
                     text(" frames: " + std::to_string(log->Count())) |
                         color(Color::Yellow)}),
               separator(),
               text(
                   "grpc://localhost:50051/helloworld.Greeter/SayHelloStream") |
                   color(Color::GrayLight),
               text("Falls back gracefully if server unavailable") |
                   color(Color::GrayLight) | italic,
               separator(),
               vbox(lines) | yframe | flex,
               separator(),
               hbox({text("Error: "),
                     text(src->last_error()) | color(Color::Red)}),
           }) |
           border;
  });
}

// ── Tab 5: Dashboard
// ──────────────────────────────────────────────────────────
Component MakeDashboardTab(std::shared_ptr<RedisSource<std::string>> redis_sub,
                           std::shared_ptr<RedisSource<std::string>> redis_poll,
                           std::shared_ptr<KafkaSource> kafka) {
  DistributedDashboard dashboard;
  dashboard.AddSource("Redis PubSub (badwolf:events)", redis_sub)
      .AddSource("Redis Poll (badwolf:counter)", redis_poll)
      .AddSource("Kafka (badwolf-topic)",
                 std::static_pointer_cast<LiveSource<std::string>>(kafka))
      .WithLayout(2);
  auto component = dashboard.Build();

  return Renderer([component] {
    return vbox({
               text("Distributed Dashboard") | bold | center,
               separator(),
               component->Render(),
           }) |
           border;
  });
}

// ── Main
// ──────────────────────────────────────────────────────────────────────
int main() {
  auto screen = ScreenInteractive::Fullscreen();

  // Create sources
  RedisConfig redis_cfg;
  redis_cfg.connect_timeout = std::chrono::milliseconds(500);
  redis_cfg.reconnect_delay = std::chrono::milliseconds(2000);
  redis_cfg.max_retries = -1;

  auto redis_sub =
      RedisSource<std::string>::Subscribe("badwolf:events", redis_cfg);
  auto redis_poll = RedisSource<std::string>::PollKey(
      "badwolf:counter", std::chrono::milliseconds(500), redis_cfg);

  KafkaConfig kafka_cfg;
  kafka_cfg.reconnect_delay = std::chrono::milliseconds(2000);
  auto kafka = std::make_shared<KafkaSource>("badwolf-topic", kafka_cfg);

  GrpcStreamConfig grpc_cfg;
  grpc_cfg.service = "helloworld.Greeter";
  grpc_cfg.method = "SayHelloStream";
  grpc_cfg.reconnect_delay = std::chrono::milliseconds(5000);
  auto grpc_src = std::make_shared<GrpcStreamSource>(grpc_cfg);

  // Log buffers per tab
  auto log1 = std::make_shared<LogBuffer>();
  auto log2 = std::make_shared<LogBuffer>();
  auto log3 = std::make_shared<LogBuffer>();
  auto log4 = std::make_shared<LogBuffer>();

  // Build tabs
  int selected_tab = 0;
  std::vector<std::string> tab_labels = {"  Redis PubSub  ", "  Redis Poll  ",
                                         "  Kafka  ", "  gRPC  ",
                                         "  Dashboard  "};

  auto tab_toggle = Toggle(&tab_labels, &selected_tab);

  auto tab_content = Container::Tab(
      {
          MakeRedisPubSubTab(redis_sub, log1),
          MakeRedisPollTab(redis_poll, log2),
          MakeKafkaTab(kafka, log3),
          MakeGrpcTab(grpc_src, log4),
          MakeDashboardTab(redis_sub, redis_poll, kafka),
      },
      &selected_tab);

  auto layout = Container::Vertical({tab_toggle, tab_content});

  auto root = Renderer(layout, [&] {
    return vbox({
               text("FTXUI BadWolf — Distributed State") | bold | center,
               separator(),
               tab_toggle->Render(),
               separator(),
               tab_content->Render() | flex,
               separator(),
               text("q: quit  tab: switch  Redis: localhost:6379  Kafka: "
                    "localhost:9092") |
                   color(Color::GrayLight) | center,
           }) |
           border;
  });

  root = CatchEvent(root, [&](Event e) -> bool {
    if (e == Event::Character('q')) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });

  screen.Loop(root);

  // Cleanup
  redis_sub->Stop();
  redis_poll->Stop();
  kafka->Stop();
  grpc_src->Stop();

  return 0;
}
