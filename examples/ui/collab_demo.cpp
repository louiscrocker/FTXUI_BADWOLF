// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file collab_demo.cpp
///
/// Demonstrates real-time collaborative TUI sessions with multiple peers.
///
/// Server mode:  ./collab_demo --server [PORT]
/// Client mode:  ./collab_demo --client HOST[:PORT] --name NAME

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/collab.hpp"
#include "ftxui/ui/log_panel.hpp"

using namespace ftxui;
using namespace ftxui::ui;

static constexpr int kDefaultPort = 7777;

// ── Helpers ───────────────────────────────────────────────────────────────────

struct Args {
  bool is_server = false;
  std::string host = "127.0.0.1";
  int port = kDefaultPort;
  std::string name = "Peer";
};

Args ParseArgs(int argc, char** argv) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--server") {
      args.is_server = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        args.port = std::stoi(argv[++i]);
      }
    } else if (a == "--client" && i + 1 < argc) {
      std::string hostport = argv[++i];
      auto colon = hostport.rfind(':');
      if (colon != std::string::npos) {
        args.host = hostport.substr(0, colon);
        args.port = std::stoi(hostport.substr(colon + 1));
      } else {
        args.host = hostport;
      }
    } else if (a == "--name" && i + 1 < argc) {
      args.name = argv[++i];
    }
  }
  return args;
}

// ── Server mode ───────────────────────────────────────────────────────────────

int RunServer(const Args& args) {
  int split_right = 24;
  int split_bottom = 12;

  auto server = std::make_shared<CollabServer>(args.port);
  server->Start();

  auto log = LogPanel::Create(40);
  log->Log("Server started on port " + std::to_string(args.port));

  // Client-side "server peer" for the notepad (server acts as a peer too)
  auto client = std::make_shared<CollabClient>("127.0.0.1", args.port, "Host");
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  client->Connect();

  // Listen for join/leave events to update the log
  client->OnRemoteEvent([&log](CollabEvent ev) {
    std::string type_str;
    switch (ev.type) {
      case CollabEvent::Type::JOIN:
        type_str = "JOIN";
        break;
      case CollabEvent::Type::LEAVE:
        type_str = "LEAVE";
        break;
      case CollabEvent::Type::KEY_PRESS:
        type_str = "KEY";
        break;
      case CollabEvent::Type::STATE_SYNC:
        type_str = "SYNC";
        break;
      default:
        type_str = "EVT";
    }
    log->Log("[" + type_str + "] " + ev.peer_name + " (" + ev.peer_id + ")");
    if (App* a = App::Active()) {
      a->Post([] {});
    }
  });

  auto peer_list = CollabPeerList(server);
  auto notepad = CollabNotepad(client);
  auto log_comp = log->Build("Event Log");

  auto layout = ResizableSplitRight(
      peer_list,
      ResizableSplitBottom(
          Renderer(log_comp, [log_comp]() { return log_comp->Render(); }),
          Renderer(notepad, [notepad]() { return notepad->Render(); }),
          &split_bottom),
      &split_right);

  auto root = Renderer(layout, [&, layout]() -> Element {
    return vbox({
        hbox({
            text(" ★  COLLAB SERVER  ★") | bold | color(Color::CyanLight) |
                flex,
            text(" Port: ") | color(Color::GrayLight),
            text(std::to_string(args.port)) | bold | color(Color::Yellow),
            text("  Peers: "),
            text(std::to_string(server->PeerCount())) | bold |
                color(Color::GreenLight),
            text("  "),
        }) | border,
        layout->Render() | flex,
        CollabStatusBar(server) | border,
        text(" q = quit | Ctrl+C = quit") | color(Color::GrayDark) | hcenter,
    });
  });

  auto app_comp = CatchEvent(root, [&](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    return false;
  });

  RunFullscreen(app_comp);

  client->Disconnect();
  server->Stop();
  return 0;
}

// ── Client mode ───────────────────────────────────────────────────────────────

int RunClient(const Args& args) {
  int split_right = 20;
  auto client =
      std::make_shared<CollabClient>(args.host, args.port, args.name);

  if (!client->Connect()) {
    fprintf(stderr, "Failed to connect to %s:%d\n", args.host.c_str(),
            args.port);
    return 1;
  }

  auto peer_list = CollabPeerList(client);
  auto notepad = CollabNotepad(client);
  auto session_notepad = WithCollabSession(notepad, client);

  auto layout = ResizableSplitRight(
      peer_list,
      Renderer(session_notepad, [session_notepad]() {
        return session_notepad->Render();
      }),
      &split_right);

  auto root = Renderer(layout, [&, layout]() -> Element {
    return vbox({
        hbox({
            text(" ★  COLLAB CLIENT  ★") | bold | color(Color::CyanLight) |
                flex,
            text(" Server: ") | color(Color::GrayLight),
            text(args.host + ":" + std::to_string(args.port)) | bold |
                color(Color::Yellow),
            text("  "),
        }) | border,
        layout->Render() | flex,
        CollabStatusBar(client) | border,
        text(" q = quit | Ctrl+C = quit") | color(Color::GrayDark) | hcenter,
    });
  });

  auto app_comp = CatchEvent(root, [&](Event e) -> bool {
    if (e == Event::Character('q')) {
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    return false;
  });

  RunFullscreen(app_comp);

  client->Disconnect();
  return 0;
}

// ── Usage screen ──────────────────────────────────────────────────────────────

int ShowUsage() {
  auto root = Renderer([]() -> Element {
    return vbox({
               text(" ★  FTXUI Collaborative TUI Demo  ★") | bold |
                   color(Color::CyanLight) | hcenter,
               separator(),
               text("  Start server:") | bold,
               text("    ./collab_demo --server [PORT]"),
               text(""),
               text("  Connect as client:") | bold,
               text("    ./collab_demo --client HOST[:PORT] --name YOUR_NAME"),
               text(""),
               text("  Example:") | bold,
               text("    Terminal 1: ./collab_demo --server 7777"),
               text("    Terminal 2: ./collab_demo --client 127.0.0.1 --name Alice"),
               text("    Terminal 3: ./collab_demo --client 127.0.0.1:7777 --name Bob"),
               separator(),
               text(" q = quit") | color(Color::GrayDark),
           }) |
           border;
  });

  auto app_comp = CatchEvent(root, [](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    return false;
  });

  Run(app_comp);
  return 0;
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
  if (argc < 2) {
    return ShowUsage();
  }
  auto args = ParseArgs(argc, argv);
  if (args.is_server) {
    return RunServer(args);
  }
  return RunClient(args);
}
