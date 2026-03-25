// Demo: FTXUI BadWolf single-header usage
//
// Compile (no cmake, no linking, just one file):
//   g++ -std=c++20 -pthread -o demo examples/single_header_demo.cpp
//
// Or from the repo root (header is at dist/ftxui.hpp):
//   g++ -std=c++20 -pthread -Idist -DBADWOLF_IMPLEMENTATION \
//       examples/single_header_demo.cpp -o demo

#define BADWOLF_IMPLEMENTATION
#include "../dist/ftxui.hpp"

int main() {
  using namespace ftxui;
  using namespace ftxui::ui;

  auto screen = ScreenInteractive::TerminalOutput();

  auto console = Console::Create(100);
  console->Info("BadWolf single-header demo");
  console->Warn("No cmake required!");
  console->Success("Just #define BADWOLF_IMPLEMENTATION and go.");

  auto ui = console->Build("Single-Header Demo");
  screen.Loop(ui);
  return 0;
}
