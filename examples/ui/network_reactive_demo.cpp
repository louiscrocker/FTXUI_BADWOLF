// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file network_reactive_demo.cpp
///
/// Demonstrates NetworkReactive<std::string> shared state over TCP.
///
/// Run as server:  ./network_reactive_demo --server
/// Run as client:  ./network_reactive_demo --client 127.0.0.1
///
/// The server updates a "Fleet Status" string every second. Clients mirror
/// the state and display it in real time.

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/network_reactive.hpp"
#include "ftxui/ui/reactive.hpp"

using namespace ftxui;
using namespace ftxui::ui;

static constexpr uint16_t kPort = 8765;

// ── Server mode ───────────────────────────────────────────────────────────────

int RunServer() {
  auto state =
      std::make_shared<Reactive<std::string>>("Fleet Status: NOMINAL");
  auto server = NetworkReactiveServer::Create(state, kPort);
  server->Start();

  std::atomic<bool> running{true};
  int tick = 0;

  // Background thread: update state every second
  std::thread updater([&]() {
    const std::string statuses[] = {
        "Fleet Status: NOMINAL",
        "Fleet Status: YELLOW ALERT",
        "Fleet Status: RED ALERT",
        "Fleet Status: SHIELDS UP",
        "Fleet Status: WARP 9",
    };
    constexpr int kNumStatuses = 5;
    while (running.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      ++tick;
      std::string new_status =
          statuses[static_cast<size_t>(tick) % kNumStatuses];
      new_status += "  [tick=" + std::to_string(tick) + "]";
      state->Set(new_status);
      if (App* a = App::Active()) a->Post([] {});
    }
  });

  auto root = Renderer([&]() -> Element {
    return vbox({
        text(" ★  NETWORK REACTIVE — SERVER MODE  ★") | bold |
            color(Color::CyanLight) | hcenter,
        separator(),
        hbox({
            text(" Port: ") | color(Color::GrayLight),
            text(std::to_string(kPort)) | bold | color(Color::Yellow),
            filler(),
            text(" Clients: ") | color(Color::GrayLight),
            text(std::to_string(server->ConnectedClients())) | bold |
                color(Color::GreenLight),
            text(" "),
        }),
        separator(),
        text(" Current state:") | color(Color::GrayLight),
        text("  " + state->Get()) | bold | color(Color::White),
        separator(),
        text(" Tick: " + std::to_string(tick)) | color(Color::GrayDark),
        separator(),
        text(" q = quit") | color(Color::GrayDark),
    }) | border;
  });

  auto app_comp = CatchEvent(root, [&](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      running.store(false);
      if (App* a = App::Active()) a->Exit();
      return true;
    }
    return false;
  });

  Run(app_comp);

  running.store(false);
  server->Stop();
  if (updater.joinable()) updater.join();
  return 0;
}

// ── Client mode ───────────────────────────────────────────────────────────────

int RunClient(const std::string& host) {
  auto client = NetworkReactiveClient::Connect(host, kPort);
  auto state = client->State();

  // Subscribe to trigger UI refresh
  state->OnChange([](const std::string&) {
    if (App* a = App::Active()) a->Post([] {});
  });

  auto root = Renderer([&]() -> Element {
    std::string conn_str = client->Connected() ? "CONNECTED" : "CONNECTING...";
    Color conn_color =
        client->Connected() ? Color::GreenLight : Color::YellowLight;

    return vbox({
        text(" ★  NETWORK REACTIVE — CLIENT MODE  ★") | bold |
            color(Color::CyanLight) | hcenter,
        separator(),
        hbox({
            text(" Server: ") | color(Color::GrayLight),
            text(host + ":" + std::to_string(kPort)) | bold |
                color(Color::Yellow),
            filler(),
            text(" Status: ") | color(Color::GrayLight),
            text(conn_str) | bold | color(conn_color),
            text(" "),
        }),
        separator(),
        text(" Mirrored state:") | color(Color::GrayLight),
        text("  " + state->Get()) | bold | color(Color::White),
        separator(),
        text(" q = quit") | color(Color::GrayDark),
    }) | border;
  });

  auto app_comp = CatchEvent(root, [&](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      if (App* a = App::Active()) a->Exit();
      return true;
    }
    return false;
  });

  Run(app_comp);

  client->Disconnect();
  return 0;
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
  if (argc >= 2 && std::string(argv[1]) == "--server") {
    return RunServer();
  }
  if (argc >= 3 && std::string(argv[1]) == "--client") {
    return RunClient(argv[2]);
  }

  // Default: show usage
  auto root = Renderer([]() -> Element {
    return vbox({
        text(" Network Reactive Demo") | bold | color(Color::CyanLight) |
            hcenter,
        separator(),
        text("  Run as server:  ./network_reactive_demo --server"),
        text("  Run as client:  ./network_reactive_demo --client 127.0.0.1"),
        separator(),
        text(" q = quit") | color(Color::GrayDark),
    }) | border;
  });

  auto app_comp = CatchEvent(root, [](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      if (App* a = App::Active()) a->Exit();
      return true;
    }
    return false;
  });

  Run(app_comp);
  return 0;
}
