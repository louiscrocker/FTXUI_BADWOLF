// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/video.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── VideoFrame
// ────────────────────────────────────────────────────────────────

uint8_t VideoFrame::GrayAt(int x, int y) const {
  const uint8_t r = pixels[(y * width + x) * 3 + 0];
  const uint8_t g = pixels[(y * width + x) * 3 + 1];
  const uint8_t b = pixels[(y * width + x) * 3 + 2];
  return static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
}

VideoFrame VideoFrame::Scale(int target_w, int target_h) const {
  if (empty() || target_w <= 0 || target_h <= 0) {
    return VideoFrame{target_w, target_h,
                      std::vector<uint8_t>(target_w * target_h * 3, 0),
                      timestamp, frame_num};
  }

  VideoFrame out;
  out.width = target_w;
  out.height = target_h;
  out.timestamp = timestamp;
  out.frame_num = frame_num;
  out.pixels.resize(target_w * target_h * 3);

  for (int ty = 0; ty < target_h; ++ty) {
    for (int tx = 0; tx < target_w; ++tx) {
      // Nearest-neighbor source pixel
      const int sx = tx * width / target_w;
      const int sy = ty * height / target_h;
      const int si = (sy * width + sx) * 3;
      const int di = (ty * target_w + tx) * 3;
      out.pixels[di + 0] = pixels[si + 0];
      out.pixels[di + 1] = pixels[si + 1];
      out.pixels[di + 2] = pixels[si + 2];
    }
  }
  return out;
}

ftxui::Element VideoFrame::ToBraille(int cols, int rows) const {
  if (cols <= 0 || rows <= 0) {
    return ftxui::text("");
  }
  const int dot_w = cols * 2;
  const int dot_h = rows * 4;
  VideoFrame scaled = Scale(dot_w, dot_h);
  return ftxui::canvas(dot_w, dot_h,
                       [scaled = std::move(scaled)](ftxui::Canvas& c) {
                         for (int y = 0; y < scaled.height; ++y) {
                           for (int x = 0; x < scaled.width; ++x) {
                             c.DrawPoint(x, y, scaled.GrayAt(x, y) > 128);
                           }
                         }
                       });
}

ftxui::Element VideoFrame::ToColoredBlock(int cols, int rows) const {
  if (cols <= 0 || rows <= 0) {
    return ftxui::text("");
  }
  // Scale to cols × rows*2 pixels (2 pixel rows map to 1 terminal row via ▀)
  VideoFrame scaled = Scale(cols, rows * 2);

  ftxui::Elements row_elems;
  row_elems.reserve(rows);

  for (int ry = 0; ry < rows; ++ry) {
    ftxui::Elements cells;
    cells.reserve(cols);

    for (int cx = 0; cx < cols; ++cx) {
      const int py0 = ry * 2;
      const int py1 = std::min(ry * 2 + 1, scaled.height - 1);

      const uint8_t r0 = scaled.pixels[(py0 * scaled.width + cx) * 3 + 0];
      const uint8_t g0 = scaled.pixels[(py0 * scaled.width + cx) * 3 + 1];
      const uint8_t b0 = scaled.pixels[(py0 * scaled.width + cx) * 3 + 2];

      const uint8_t r1 = scaled.pixels[(py1 * scaled.width + cx) * 3 + 0];
      const uint8_t g1 = scaled.pixels[(py1 * scaled.width + cx) * 3 + 1];
      const uint8_t b1 = scaled.pixels[(py1 * scaled.width + cx) * 3 + 2];

      cells.push_back(ftxui::text("▀") |
                      ftxui::color(ftxui::Color::RGB(r0, g0, b0)) |
                      ftxui::bgcolor(ftxui::Color::RGB(r1, g1, b1)));
    }
    row_elems.push_back(ftxui::hbox(std::move(cells)));
  }
  return ftxui::vbox(std::move(row_elems));
}

// ── FfmpegVideoSource
// ─────────────────────────────────────────────────────────

FfmpegVideoSource::FfmpegVideoSource(std::string path, double scale_factor)
    : path_(std::move(path)), scale_factor_(scale_factor) {}

FfmpegVideoSource::~FfmpegVideoSource() {
  Close();
}

void FfmpegVideoSource::ProbeVideo() {
  std::string cmd =
      "ffprobe -v error -select_streams v:0 "
      "-show_entries stream=width,height,r_frame_rate "
      "-of csv=p=0 \"" +
      path_ + "\" 2>/dev/null";

  FILE* p = ::popen(cmd.c_str(), "r");
  if (!p) {
    width_ = 640;
    height_ = 480;
    fps_ = 25.0;
    return;
  }

  int native_w = 640, native_h = 480;
  int fps_num = 25, fps_den = 1;
  // Expected format: "width,height,fps_num/fps_den\n"
  // fscanf format handles the slash-separated fps
  // Some builds emit "width,height,fps_numfps_den" so be permissive.
  int rc = ::fscanf(p, "%d,%d,%d/%d", &native_w, &native_h, &fps_num, &fps_den);
  if (rc < 3) {
    // Fallback: try comma-only
    ::rewind(p);
    rc = ::fscanf(p, "%d,%d,%d", &native_w, &native_h, &fps_num);
    fps_den = 1;
  }
  ::pclose(p);

  width_ = std::max(1, static_cast<int>(native_w * scale_factor_));
  height_ = std::max(1, static_cast<int>(native_h * scale_factor_));
  fps_ = (fps_den > 0) ? static_cast<double>(fps_num) / fps_den : 25.0;
}

bool FfmpegVideoSource::Open() {
  if (pipe_) {
    return true;
  }

  ProbeVideo();

  std::ostringstream cmd;
  cmd << "ffmpeg -i \"" << path_ << "\""
      << " -f rawvideo -pix_fmt rgb24"
      << " -vf scale=" << width_ << ":" << height_ << " pipe:1 2>/dev/null";

  pipe_ = ::popen(cmd.str().c_str(), "r");
  frame_count_ = 0;
  return pipe_ != nullptr;
}

void FfmpegVideoSource::Close() {
  if (pipe_) {
    ::pclose(pipe_);
    pipe_ = nullptr;
  }
}

bool FfmpegVideoSource::IsOpen() const {
  return pipe_ != nullptr;
}

bool FfmpegVideoSource::ReadFrame(VideoFrame& out) {
  if (!pipe_) {
    return false;
  }

  const size_t frame_bytes = static_cast<size_t>(width_ * height_ * 3);
  if (frame_bytes == 0) {
    return false;
  }

  std::vector<uint8_t> buf(frame_bytes);
  size_t total = 0;
  while (total < frame_bytes) {
    const size_t n = ::fread(buf.data() + total, 1, frame_bytes - total, pipe_);
    if (n == 0) {
      return false;  // EOF or error
    }
    total += n;
  }

  out.width = width_;
  out.height = height_;
  out.pixels = std::move(buf);
  out.frame_num = frame_count_;
  out.timestamp = (fps_ > 0.0) ? static_cast<double>(frame_count_) / fps_ : 0.0;
  ++frame_count_;
  return true;
}

double FfmpegVideoSource::fps() const {
  return fps_;
}

int FfmpegVideoSource::width() const {
  return width_;
}

int FfmpegVideoSource::height() const {
  return height_;
}

bool FfmpegVideoSource::FfmpegAvailable() {
  return ::system("ffmpeg -version > /dev/null 2>&1") == 0;
}

std::string FfmpegVideoSource::FfmpegVersion() {
  FILE* p = ::popen("ffmpeg -version 2>&1 | head -1", "r");
  if (!p) {
    return {};
  }
  char buf[256] = {};
  if (::fgets(buf, sizeof(buf), p) != nullptr) {
    // strip trailing newline
    const size_t len = ::strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
      buf[len - 1] = '\0';
    }
  }
  ::pclose(p);
  return std::string(buf);
}

// ── CameraSource
// ──────────────────────────────────────────────────────────────

CameraSource::CameraSource(std::string device)
    : FfmpegVideoSource(std::move(device), 1.0) {}

bool CameraSource::Open() {
  if (pipe_) {
    return true;
  }

  // Camera sources use platform-specific input formats.
  // Default to 640×480 @ 30 fps if not already set.
  if (width_ == 0) {
    width_ = 640;
  }
  if (height_ == 0) {
    height_ = 480;
  }
  if (fps_ <= 0.0) {
    fps_ = 30.0;
  }

  std::ostringstream cmd;
#ifdef __APPLE__
  // macOS: AVFoundation input.  path_ holds the device index ("0", "1", …).
  cmd << "ffmpeg -f avfoundation -framerate " << static_cast<int>(fps_)
      << " -video_size " << width_ << "x" << height_ << " -i \"" << path_
      << ":none\""
      << " -f rawvideo -pix_fmt rgb24 pipe:1 2>/dev/null";
#else
  // Linux: V4L2 input.  path_ holds the device path ("/dev/video0", …).
  cmd << "ffmpeg -f v4l2 -framerate " << static_cast<int>(fps_)
      << " -video_size " << width_ << "x" << height_ << " -i \"" << path_
      << "\""
      << " -f rawvideo -pix_fmt rgb24 pipe:1 2>/dev/null";
#endif

  pipe_ = ::popen(cmd.str().c_str(), "r");
  frame_count_ = 0;
  return pipe_ != nullptr;
}

// ── VideoPlayer
// ───────────────────────────────────────────────────────────────

VideoPlayer::VideoPlayer(std::shared_ptr<VideoSource> source,
                         VideoPlayerConfig cfg)
    : source_(std::move(source)), cfg_(cfg) {}

VideoPlayer::~VideoPlayer() {
  Stop();
  if (decode_thread_.joinable()) {
    decode_thread_.join();
  }
}

void VideoPlayer::DecodeLoop() {
  if (!source_->IsOpen()) {
    if (!source_->Open()) {
      return;
    }
  }

  while (!stop_requested_.load(std::memory_order_relaxed)) {
    if (!playing_.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
      continue;
    }

    VideoFrame frame;
    if (!source_->ReadFrame(frame)) {
      if (cfg_.loop) {
        source_->Close();
        if (!source_->Open()) {
          break;
        }
        continue;
      }
      playing_.store(false, std::memory_order_relaxed);
      break;
    }

    {
      std::lock_guard<std::mutex> lock(frame_mutex_);
      current_frame_ = std::move(frame);
      current_time_ = current_frame_.timestamp;
      ++frame_count_;
    }

    // Trigger UI redraw.
    if (App* app = App::Active()) {
      app->PostEvent(Event::Custom);
    }

    // Pace the decode loop to the source frame rate.
    const double fps_eff = (source_->fps() > 0.0) ? source_->fps() : 25.0;
    const double frame_ms =
        1000.0 / (fps_eff * std::max(0.1, cfg_.playback_speed));
    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<int>(frame_ms)));
  }
}

void VideoPlayer::Play() {
  playing_.store(true, std::memory_order_relaxed);
  stop_requested_.store(false, std::memory_order_relaxed);
  if (!decode_thread_.joinable()) {
    decode_thread_ = std::thread([this] { DecodeLoop(); });
  }
}

void VideoPlayer::Pause() {
  playing_.store(false, std::memory_order_relaxed);
}

void VideoPlayer::Stop() {
  stop_requested_.store(true, std::memory_order_relaxed);
  playing_.store(false, std::memory_order_relaxed);
}

bool VideoPlayer::IsPlaying() const {
  return playing_.load(std::memory_order_relaxed);
}

void VideoPlayer::SetRenderMode(RenderMode mode) {
  cfg_.mode = mode;
}

void VideoPlayer::SetLoop(bool loop) {
  cfg_.loop = loop;
}

double VideoPlayer::CurrentTime() const {
  std::lock_guard<std::mutex> lock(frame_mutex_);
  return current_time_;
}

uint64_t VideoPlayer::FrameCount() const {
  std::lock_guard<std::mutex> lock(frame_mutex_);
  return frame_count_;
}

ftxui::Component VideoPlayer::Build() {
  // Capture shared_ptr to the VideoPlayer itself so that the component
  // keeps the player alive.  But since the component is owned by the caller
  // who also owns VideoPlayer, a raw pointer is safe here for performance.
  VideoPlayer* self = this;

  // The Renderer reads the current frame under the mutex and returns an
  // Element that uses a flexible canvas so it fills its allocated space.
  auto renderer = ftxui::Renderer([self]() -> ftxui::Element {
    // Grab the current frame.
    VideoFrame frame;
    {
      std::lock_guard<std::mutex> lock(self->frame_mutex_);
      frame = self->current_frame_;
    }

    // ── Video area ────────────────────────────────────────────────────────
    ftxui::Element video;
    if (frame.empty()) {
      if (!self->source_->IsOpen() &&
          !self->playing_.load(std::memory_order_relaxed)) {
        video = ftxui::text("  ▶ Press Space to play") | ftxui::center |
                ftxui::flex;
      } else {
        video = ftxui::text("  Buffering…") | ftxui::center | ftxui::flex;
      }
    } else {
      // Use a flexible canvas so the video fills the available space.
      const RenderMode mode = self->cfg_.mode;
      VideoFrame captured = frame;  // copy for lambda capture

      switch (mode) {
        case RenderMode::ColorBlock:
          video =
              ftxui::canvas([cap = std::move(captured)](ftxui::Canvas& c) {
                const int cols = c.width() / 2;
                const int rows = c.height() / 4;
                // Draw half-block pixels directly onto the braille canvas.
                // We reuse the braille dot grid as pixel positions.
                if (cols <= 0 || rows <= 0) {
                  return;
                }
                VideoFrame scaled = cap.Scale(cols, rows * 2);
                for (int ry = 0; ry < rows; ++ry) {
                  for (int cx = 0; cx < cols; ++cx) {
                    const int py0 = ry * 2;
                    const int py1 = std::min(ry * 2 + 1, scaled.height - 1);
                    const uint8_t r0 =
                        scaled.pixels[(py0 * scaled.width + cx) * 3 + 0];
                    const uint8_t g0 =
                        scaled.pixels[(py0 * scaled.width + cx) * 3 + 1];
                    const uint8_t b0 =
                        scaled.pixels[(py0 * scaled.width + cx) * 3 + 2];
                    const uint8_t r1 =
                        scaled.pixels[(py1 * scaled.width + cx) * 3 + 0];
                    const uint8_t g1 =
                        scaled.pixels[(py1 * scaled.width + cx) * 3 + 1];
                    const uint8_t b1 =
                        scaled.pixels[(py1 * scaled.width + cx) * 3 + 2];
                    // Draw two block rows per terminal row using DrawBlock.
                    c.DrawBlock(cx * 2, ry * 4, true, Color::RGB(r0, g0, b0));
                    c.DrawBlock(cx * 2, ry * 4 + 2, true,
                                Color::RGB(r1, g1, b1));
                  }
                }
              }) |
              ftxui::flex;
          break;

        case RenderMode::GrayBlock:
          video =
              ftxui::canvas([cap = std::move(captured)](ftxui::Canvas& c) {
                const int cols = c.width() / 2;
                const int rows = c.height() / 4;
                if (cols <= 0 || rows <= 0) {
                  return;
                }
                VideoFrame scaled = cap.Scale(cols, rows * 2);
                for (int ry = 0; ry < rows; ++ry) {
                  for (int cx = 0; cx < cols; ++cx) {
                    const int py0 = ry * 2;
                    const int py1 = std::min(ry * 2 + 1, scaled.height - 1);
                    const uint8_t v0 = scaled.GrayAt(cx, py0);
                    const uint8_t v1 = scaled.GrayAt(cx, py1);
                    c.DrawBlock(cx * 2, ry * 4, true, Color::RGB(v0, v0, v0));
                    c.DrawBlock(cx * 2, ry * 4 + 2, true,
                                Color::RGB(v1, v1, v1));
                  }
                }
              }) |
              ftxui::flex;
          break;

        case RenderMode::Braille:
        default:
          video = ftxui::canvas([cap = std::move(captured)](ftxui::Canvas& c) {
                    const int dot_w = c.width();
                    const int dot_h = c.height();
                    if (dot_w <= 0 || dot_h <= 0) {
                      return;
                    }
                    VideoFrame scaled = cap.Scale(dot_w, dot_h);
                    for (int y = 0; y < dot_h; ++y) {
                      for (int x = 0; x < dot_w; ++x) {
                        c.DrawPoint(x, y, scaled.GrayAt(x, y) > 128);
                      }
                    }
                  }) |
                  ftxui::flex;
          break;
      }
    }

    // ── Status / controls overlay ────────────────────────────────────────
    if (!self->cfg_.show_controls && !self->cfg_.show_timestamp) {
      return video;
    }

    ftxui::Elements status_items;

    if (self->cfg_.show_controls) {
      const bool playing = self->playing_.load(std::memory_order_relaxed);
      status_items.push_back(ftxui::text(playing ? " ⏸ Space " : " ▶ Space ") |
                             ftxui::color(ftxui::Color::GreenLight));
      status_items.push_back(ftxui::text("■ Q ") |
                             ftxui::color(ftxui::Color::RedLight));
      status_items.push_back(ftxui::text("🔁 L ") |
                             ftxui::color(ftxui::Color::CyanLight));
      status_items.push_back(ftxui::text("⊞ B ") |
                             ftxui::color(ftxui::Color::YellowLight));
    }

    if (self->cfg_.show_timestamp) {
      double ts = 0.0;
      uint64_t fc = 0;
      {
        std::lock_guard<std::mutex> lock(self->frame_mutex_);
        ts = self->current_time_;
        fc = self->frame_count_;
      }
      const int sec = static_cast<int>(ts);
      std::string ts_str = std::to_string(sec / 60) + ":" +
                           (sec % 60 < 10 ? "0" : "") +
                           std::to_string(sec % 60);
      status_items.push_back(ftxui::text("  " + ts_str + "  ") |
                             ftxui::color(ftxui::Color::GrayLight));
      if (self->cfg_.show_fps) {
        const double src_fps =
            (self->source_->fps() > 0) ? self->source_->fps() : 25.0;
        status_items.push_back(
            ftxui::text(std::to_string(static_cast<int>(src_fps)) + "fps ") |
            ftxui::color(ftxui::Color::GrayDark));
      }
      (void)fc;
    }

    auto controls_bar = ftxui::hbox(std::move(status_items));
    return ftxui::vbox({
        video | ftxui::flex,
        controls_bar,
    });
  });

  // Keyboard event handling
  renderer |= ftxui::CatchEvent([self](ftxui::Event event) -> bool {
    if (event == ftxui::Event::Character(' ')) {
      if (self->IsPlaying()) {
        self->Pause();
      } else {
        self->Play();
      }
      return true;
    }
    if (event == ftxui::Event::Character('q') ||
        event == ftxui::Event::Character('Q')) {
      self->Stop();
      return true;
    }
    if (event == ftxui::Event::Character('l') ||
        event == ftxui::Event::Character('L')) {
      self->cfg_.loop = !self->cfg_.loop;
      return true;
    }
    if (event == ftxui::Event::Character('b') ||
        event == ftxui::Event::Character('B')) {
      // Cycle render mode
      const int next = (static_cast<int>(self->cfg_.mode) + 1) % 3;
      self->cfg_.mode = static_cast<RenderMode>(next);
      return true;
    }
    return false;
  });

  return renderer;
}

// ── AsciiArtRenderer
// ──────────────────────────────────────────────────────────

static constexpr char kAsciiPalette[] = " .:-=+*#%@";
static constexpr int kPaletteSize = static_cast<int>(sizeof(kAsciiPalette) - 1);

std::string AsciiArtRenderer::Render(const VideoFrame& frame,
                                     int cols,
                                     int rows,
                                     bool /*use_color*/) {
  if (frame.empty() || cols <= 0 || rows <= 0) {
    return {};
  }
  const VideoFrame scaled = frame.Scale(cols, rows);
  std::string out;
  out.reserve(static_cast<size_t>((cols + 1) * rows));
  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      const uint8_t gray = scaled.GrayAt(x, y);
      const int idx = gray * (kPaletteSize - 1) / 255;
      out += kAsciiPalette[idx];
    }
    out += '\n';
  }
  return out;
}

ftxui::Element AsciiArtRenderer::RenderBraille(const VideoFrame& frame,
                                               int cols,
                                               int rows) {
  return frame.ToBraille(cols, rows);
}

ftxui::Element AsciiArtRenderer::RenderBlocks(const VideoFrame& frame,
                                              int cols,
                                              int rows,
                                              bool colored) {
  if (colored) {
    return frame.ToColoredBlock(cols, rows);
  }
  // Greyscale blocks
  if (frame.empty() || cols <= 0 || rows <= 0) {
    return ftxui::text("");
  }
  const VideoFrame scaled = frame.Scale(cols, rows * 2);
  ftxui::Elements row_elems;
  row_elems.reserve(rows);
  for (int ry = 0; ry < rows; ++ry) {
    ftxui::Elements cells;
    cells.reserve(cols);
    for (int cx = 0; cx < cols; ++cx) {
      const int py0 = ry * 2;
      const int py1 = std::min(ry * 2 + 1, scaled.height - 1);
      const uint8_t v0 = scaled.GrayAt(cx, py0);
      const uint8_t v1 = scaled.GrayAt(cx, py1);
      cells.push_back(ftxui::text("▀") |
                      ftxui::color(ftxui::Color::RGB(v0, v0, v0)) |
                      ftxui::bgcolor(ftxui::Color::RGB(v1, v1, v1)));
    }
    row_elems.push_back(ftxui::hbox(std::move(cells)));
  }
  return ftxui::vbox(std::move(row_elems));
}

// ── Free functions
// ────────────────────────────────────────────────────────────

ftxui::Component VideoPlayerFromFile(const std::string& path,
                                     VideoPlayer::Config cfg) {
  auto source = std::make_shared<FfmpegVideoSource>(path);
  auto player = std::make_shared<VideoPlayer>(std::move(source), cfg);
  auto component = player->Build();
  // Keep player alive alongside the component using a shared_ptr capture.
  // Wrap so the player lifetime is tied to the component.
  return ftxui::Renderer(component,
                         [player, component]() { return component->Render(); });
}

ftxui::Component CameraFeed(const std::string& device,
                            VideoPlayer::Config cfg) {
  auto source = std::make_shared<CameraSource>(device);
  auto player = std::make_shared<VideoPlayer>(std::move(source), cfg);
  auto component = player->Build();
  return ftxui::Renderer(component,
                         [player, component]() { return component->Render(); });
}

ftxui::Component GifPlayer(const std::string& path, bool loop) {
  VideoPlayer::Config cfg;
  cfg.loop = loop;
  cfg.show_controls = false;
  cfg.show_timestamp = false;
  return VideoPlayerFromFile(path, cfg);
}

}  // namespace ftxui::ui
