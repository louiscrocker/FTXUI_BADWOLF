// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @brief Demo: Event Recorder / Playback
///
/// Shows a simple counter form.
///   Ctrl+R — start / stop recording
///   Ctrl+P — replay last recording
/// Status bar shows recording state. Session saved to "badwolf_session.txt".

#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/event_recorder.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  int counter = 0;
  std::string status = "Ready  |  Ctrl+R=record  Ctrl+P=replay  q=quit";

  auto& rec = EventRecorder::Instance();

  auto increment = Button("  +  ", [&] { ++counter; });
  auto decrement = Button("  -  ", [&] { --counter; });
  auto controls = Container::Horizontal({increment, decrement});

  // Wrap controls to intercept events for recording.
  auto recorded_controls = rec.WithRecording(controls);

  auto root = CatchEvent(
      Renderer(recorded_controls,
               [&] {
                 std::string state_str;
                 if (rec.IsRecording()) {
                   state_str =
                       "● REC  |  Ctrl+R=stop  (" +
                       std::to_string(rec.EventCount()) + " events)";
                 } else if (rec.IsReplaying()) {
                   state_str = "▶ REPLAY …";
                 } else {
                   state_str = status;
                 }

                 return vbox({
                     text("  Event Recorder Demo  ") | bold | center,
                     separator(),
                     text("Counter: " + std::to_string(counter)) | center,
                     separator(),
                     recorded_controls->Render() | center,
                     separator(),
                     text("  " + state_str + "  ") | inverted,
                 });
               }),
      [&](Event event) -> bool {
        // Ctrl+R: toggle recording.
        if (event == Event::CtrlR) {
          if (rec.IsRecording()) {
            rec.StopRecording();
            rec.Save("badwolf_session.txt");
            status = "Recording stopped. Saved to badwolf_session.txt  "
                     "(" +
                     std::to_string(rec.EventCount()) + " events)";
          } else {
            rec.StartRecording();
            status = "● Recording…";
          }
          return true;
        }
        // Ctrl+P: replay.
        if (event == Event::CtrlP) {
          if (rec.IsReplaying()) {
            rec.StopReplay();
            status = "Replay stopped.";
          } else if (rec.EventCount() > 0) {
            status = "▶ Replaying…";
            rec.Replay([&] { status = "Replay complete."; });
          } else {
            // Try loading from file.
            if (rec.Load("badwolf_session.txt")) {
              status = "Loaded session. Press Ctrl+P again to replay.";
            } else {
              status = "No session recorded. Press Ctrl+R first.";
            }
          }
          return true;
        }
        // 'q': quit.
        if (event == Event::Character('q')) {
          auto* app = App::Active();
          if (app) {
            app->Exit();
          }
          return true;
        }
        return false;
      });

  RunFullscreen(root);
  return 0;
}
