// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
//
// video_demo.cpp — 4-tab terminal video player demo.
//
// Tabs:
//   1. File Player  — loads sample.mp4 via ffmpeg pipe
//   2. Camera       — live webcam feed (device "0")
//   3. GIF          — animated GIF via ffmpeg
//   4. Render Modes — side-by-side Braille / ColorBlock / GrayBlock
//
// All tabs degrade gracefully when ffmpeg is not installed.

#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/video.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Helpers
// ───────────────────────────────────────────────────────────────────

static Component PlaceholderPanel(const std::string& icon,
                                  const std::string& title,
                                  const std::string& detail) {
  return Renderer([icon, title, detail]() -> Element {
    return vbox({
               text(""),
               text("  " + icon + "  " + title) | bold | color(Color::Cyan),
               text(""),
               text("  " + detail) | color(Color::GrayLight),
               text(""),
               text("  Install ffmpeg and provide the required input file.") |
                   color(Color::GrayDark),
           }) |
           border | flex;
  });
}

// ── Tab 1: File Player
// ────────────────────────────────────────────────────────

static Component MakeFilePlayerTab() {
  if (!FfmpegVideoSource::FfmpegAvailable()) {
    return PlaceholderPanel("📹", "File Player",
                            "ffmpeg not found in PATH — cannot play video.");
  }

  const std::string path = "sample.mp4";

  VideoPlayer::Config cfg;
  cfg.loop = false;
  cfg.show_controls = true;
  cfg.show_timestamp = true;
  cfg.show_fps = true;
  cfg.mode = VideoPlayer::RenderMode::Braille;

  auto source = std::make_shared<FfmpegVideoSource>(path, 0.5);
  auto player = std::make_shared<VideoPlayer>(source, cfg);
  auto video_comp = player->Build();

  // Wrap to keep player alive alongside the component.
  return Renderer(video_comp, [player, video_comp]() {
    return vbox({
        text(" 📹 File Player — sample.mp4") | bold | color(Color::Cyan) |
            bgcolor(Color::Black),
        separator(),
        video_comp->Render() | flex,
    });
  });
}

// ── Tab 2: Camera
// ─────────────────────────────────────────────────────────────

static Component MakeCameraTab() {
  if (!FfmpegVideoSource::FfmpegAvailable()) {
    return PlaceholderPanel(
        "📷", "Camera", "ffmpeg not found in PATH — cannot capture camera.");
  }

  VideoPlayer::Config cfg;
  cfg.loop = false;
  cfg.show_controls = true;
  cfg.show_timestamp = false;
  cfg.show_fps = true;
  cfg.mode = VideoPlayer::RenderMode::ColorBlock;

  auto source = std::make_shared<CameraSource>("0");
  auto player = std::make_shared<VideoPlayer>(source, cfg);
  auto video_comp = player->Build();

  return Renderer(video_comp, [player, video_comp]() {
    return vbox({
        text(" 📷 Camera — device 0") | bold | color(Color::Green) |
            bgcolor(Color::Black),
        separator(),
        video_comp->Render() | flex,
    });
  });
}

// ── Tab 3: GIF
// ────────────────────────────────────────────────────────────────

static Component MakeGifTab() {
  if (!FfmpegVideoSource::FfmpegAvailable()) {
    return PlaceholderPanel("🎞", "GIF Player",
                            "ffmpeg not found in PATH — cannot decode GIF.");
  }

  auto gif_comp = GifPlayer("sample.gif", /*loop=*/true);

  return Renderer(gif_comp, [gif_comp]() {
    return vbox({
        text(" 🎞 GIF Player — sample.gif (looping)") | bold |
            color(Color::Yellow) | bgcolor(Color::Black),
        separator(),
        gif_comp->Render() | flex,
    });
  });
}

// ── Tab 4: Render Modes side-by-side
// ──────────────────────────────────────────

static Component MakeRenderModesTab() {
  // Build three players from the same source so we can show the same frame
  // rendered in three different modes side-by-side.

  if (!FfmpegVideoSource::FfmpegAvailable()) {
    return PlaceholderPanel("⊞", "Render Modes", "ffmpeg not found in PATH.");
  }

  const std::string path = "sample.mp4";

  auto make_player = [&](VideoPlayer::RenderMode mode) {
    VideoPlayer::Config cfg;
    cfg.mode = mode;
    cfg.loop = true;
    cfg.show_controls = false;
    cfg.show_timestamp = false;
    auto src = std::make_shared<FfmpegVideoSource>(path, 0.25);
    auto player = std::make_shared<VideoPlayer>(src, cfg);
    return std::make_pair(player, player->Build());
  };

  auto [p_b, c_b] = make_player(VideoPlayer::RenderMode::Braille);
  auto [p_c, c_c] = make_player(VideoPlayer::RenderMode::ColorBlock);
  auto [p_g, c_g] = make_player(VideoPlayer::RenderMode::GrayBlock);

  // Play all three immediately
  p_b->Play();
  p_c->Play();
  p_g->Play();

  auto container = Container::Horizontal({c_b, c_c, c_g});

  return Renderer(container, [p_b, p_c, p_g, c_b, c_c, c_g]() -> Element {
    return vbox({
        text(" ⊞ Render Modes — same source, three renderers") | bold |
            color(Color::Magenta) | bgcolor(Color::Black),
        separator(),
        hbox({
            vbox({text(" Braille") | bold | color(Color::White), separator(),
                  c_b->Render() | flex}) |
                border | flex,
            vbox({text(" ColorBlock") | bold | color(Color::GreenLight),
                  separator(), c_c->Render() | flex}) |
                border | flex,
            vbox({text(" GrayBlock") | bold | color(Color::GrayLight),
                  separator(), c_g->Render() | flex}) |
                border | flex,
        }) | flex,
    });
  });
}

// ── Status bar
// ────────────────────────────────────────────────────────────────

static Element StatusBar() {
  const bool has_ffmpeg = FfmpegVideoSource::FfmpegAvailable();
  const std::string ffmpeg_status =
      has_ffmpeg ? ("ffmpeg: ✓ " + FfmpegVideoSource::FfmpegVersion())
                 : "ffmpeg: ✗ not found";
  const Color ffmpeg_color = has_ffmpeg ? Color::GreenLight : Color::RedLight;

  return hbox({
             text(" " + ffmpeg_status) | color(ffmpeg_color),
             filler(),
             text(" q:quit ") | color(Color::GrayDark),
         }) |
         bgcolor(Color::Black);
}

// ── Main
// ──────────────────────────────────────────────────────────────────────

int main() {
  // Build tabs
  std::vector<std::string> tab_labels = {
      " 📹 File ",
      " 📷 Camera ",
      " 🎞 GIF ",
      " ⊞ Modes ",
  };
  int selected_tab = 0;

  auto tab_file = MakeFilePlayerTab();
  auto tab_cam = MakeCameraTab();
  auto tab_gif = MakeGifTab();
  auto tab_modes = MakeRenderModesTab();

  auto tab_toggle = Toggle(&tab_labels, &selected_tab);

  auto tab_content =
      Container::Tab({tab_file, tab_cam, tab_gif, tab_modes}, &selected_tab);

  auto main_container = Container::Vertical({
      tab_toggle,
      tab_content,
  });

  auto renderer = Renderer(main_container, [&]() -> Element {
    return vbox({
        // Header
        text("  📺 Terminal Video Player — FTXUI BadWolf") | bold |
            color(Color::Cyan),
        separator(),
        // Tab bar
        tab_toggle->Render(),
        separator(),
        // Tab content
        tab_content->Render() | flex,
        separator(),
        // Status bar
        StatusBar(),
    });
  });

  // Global quit on 'q'
  renderer |= CatchEvent([&](Event event) -> bool {
    if (event == Event::Character('q') || event == Event::Character('Q')) {
      App::Active()->Exit();
      return true;
    }
    return false;
  });

  App::Fullscreen().Loop(renderer);
  return 0;
}
