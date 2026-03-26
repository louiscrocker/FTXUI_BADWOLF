// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file multiplayer_demo.cpp
/// BadWolf Multiplayer Terminal Games demo.
/// Run as server: ./multiplayer_demo --server [--port 29910] [--game shooter]
/// Run as client: ./multiplayer_demo --host localhost [--port 29910]

#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/multiplayer.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main(int argc, char* argv[]) {
  // Parse args
  bool is_server = false;
  std::string host = "localhost";
  int port = 29910;
  std::string game_name = "shooter";
  std::string player_name = "Player";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--server") {
      is_server = true;
    } else if (arg == "--host" && i + 1 < argc) {
      host = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      port = std::stoi(argv[++i]);
    } else if (arg == "--game" && i + 1 < argc) {
      game_name = argv[++i];
    } else if (arg == "--name" && i + 1 < argc) {
      player_name = argv[++i];
    }
  }

  // ── Startup / menu screen ────────────────────────────────────────────────
  struct MenuState {
    int selected_game = 0;
    std::string port_str = "29910";
    std::string host_str = "localhost";
    std::string name_str = "Player";
    bool launched = false;
    bool as_server = false;
    std::string launch_game;
  };
  auto menu_state = std::make_shared<MenuState>();
  menu_state->port_str = std::to_string(port);
  menu_state->host_str = host;
  menu_state->name_str = player_name;
  menu_state->as_server = is_server;
  menu_state->launch_game = game_name;

  auto screen = ScreenInteractive::Fullscreen();

  // If args were provided, skip menu
  if (argc > 1) {
    menu_state->launched = true;
  }

  std::vector<std::string> game_names = {"TopDownShooter", "MultiPong",
                                         "CoopPush", "Racing"};

  auto port_input = Input(&menu_state->port_str, "port");
  auto host_input = Input(&menu_state->host_str, "host");
  auto name_input = Input(&menu_state->name_str, "name");
  auto game_menu = Menu(&game_names, &menu_state->selected_game);

  auto host_btn = Button("Host Game (Server)", [&] {
    menu_state->as_server = true;
    menu_state->launch_game = game_names[menu_state->selected_game];
    menu_state->launched = true;
    screen.ExitLoopClosure()();
  });
  auto join_btn = Button("Join Game (Client)", [&] {
    menu_state->as_server = false;
    menu_state->launch_game = game_names[menu_state->selected_game];
    menu_state->launched = true;
    screen.ExitLoopClosure()();
  });
  auto quit_btn = Button("Quit", [&] { screen.ExitLoopClosure()(); });

  auto menu_container = Container::Vertical({
      host_input,
      port_input,
      name_input,
      game_menu,
      Container::Horizontal({host_btn, join_btn, quit_btn}),
  });

  auto menu_renderer = Renderer(menu_container, [&]() -> Element {
    return vbox({
               text("🎮 BadWolf Multiplayer") | bold | color(Color::Cyan) |
                   center,
               text("Real-time terminal games over TCP") |
                   color(Color::GrayLight) | center,
               separator(),
               hbox({
                   vbox({
                       text("Player Name:"),
                       name_input->Render() | border,
                       separator(),
                       text("Server Host (join):"),
                       host_input->Render() | border,
                       separator(),
                       text("Port:"),
                       port_input->Render() | border,
                   }) | size(WIDTH, EQUAL, 30),
                   separator(),
                   vbox({
                       text("Select Game:") | bold,
                       game_menu->Render() | border | flex,
                   }) | flex,
               }),
               separator(),
               hbox({
                   host_btn->Render() | flex,
                   join_btn->Render() | flex,
                   quit_btn->Render(),
               }),
               separator(),
               text("TopDownShooter: WASD move, Space fire") |
                   color(Color::GrayLight) | center,
               text("MultiPong: ↑↓ move paddle") | color(Color::GrayLight) |
                   center,
               text("CoopPush: WASD push ball to goal") |
                   color(Color::GrayLight) | center,
               text("Racing: ↑↓←→ drive") | color(Color::GrayLight) | center,
           }) |
           border | center;
  });

  auto menu_with_exit = CatchEvent(menu_renderer, [&](Event ev) -> bool {
    if (ev == Event::Character('q') || ev == Event::Escape) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });

  if (!menu_state->launched) {
    screen.Loop(menu_with_exit);
  }

  if (!menu_state->launched) {
    return 0;
  }

  // ── Launch game ──────────────────────────────────────────────────────────
  MultiGames::GameOptions opts;
  opts.is_server = menu_state->as_server;
  opts.host = menu_state->host_str;
  try {
    opts.port = std::stoi(menu_state->port_str);
  } catch (...) {
    opts.port = 29910;
  }
  opts.player_name = menu_state->name_str;
  opts.color = Color::Cyan;

  Component game;
  if (menu_state->launch_game == "MultiPong") {
    game = MultiGames::MultiPong(opts);
  } else if (menu_state->launch_game == "CoopPush") {
    game = MultiGames::CoopPush(opts);
  } else if (menu_state->launch_game == "Racing") {
    game = MultiGames::Racing(opts);
  } else {
    game = MultiGames::TopDownShooter(opts);
  }

  auto game_with_exit = CatchEvent(game, [&](Event ev) -> bool {
    if (ev == Event::Character('q') || ev == Event::Escape) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });

  auto game_screen = ScreenInteractive::Fullscreen();
  game_screen.Loop(game_with_exit);

  return 0;
}
