// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_VIDEO_HPP
#define FTXUI_UI_VIDEO_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

// ── VideoFrame
// ────────────────────────────────────────────────────────────────
struct VideoFrame {
  int width{0};
  int height{0};
  std::vector<uint8_t> pixels;  // RGB24: width * height * 3 bytes
  double timestamp{0.0};
  uint64_t frame_num{0};

  /// Convert a pixel to grayscale intensity [0,255].
  uint8_t GrayAt(int x, int y) const;

  /// Scale to target dimensions (nearest-neighbor).
  VideoFrame Scale(int target_w, int target_h) const;

  /// Convert to braille Element at given terminal size.
  /// Each braille cell covers 2×4 dots → cols*2 × rows*4 pixels sampled.
  ftxui::Element ToBraille(int cols, int rows) const;

  /// Convert to colored half-block Element.
  /// Uses ▀ (U+2580): foreground = upper half pixel, background = lower half.
  ftxui::Element ToColoredBlock(int cols, int rows) const;

  bool empty() const { return pixels.empty(); }
};

// ── VideoSource
// ───────────────────────────────────────────────────────────────
/// Abstract base for video sources.
class VideoSource {
 public:
  virtual ~VideoSource() = default;
  virtual bool Open() = 0;
  virtual void Close() = 0;
  virtual bool IsOpen() const = 0;
  virtual bool ReadFrame(VideoFrame& out) = 0;
  virtual double fps() const = 0;
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual bool HasAudio() const { return false; }
};

// ── FfmpegVideoSource
// ─────────────────────────────────────────────────────────
/// Opens a file or URL via an ffmpeg pipe (requires ffmpeg in PATH).
/// No libav headers or libraries are used; communication is via popen().
class FfmpegVideoSource : public VideoSource {
 public:
  explicit FfmpegVideoSource(std::string path, double scale_factor = 1.0);
  ~FfmpegVideoSource() override;

  bool Open() override;
  void Close() override;
  bool IsOpen() const override;
  bool ReadFrame(VideoFrame& out) override;
  double fps() const override;
  int width() const override;
  int height() const override;

  /// @return true if the `ffmpeg` binary is found in PATH.
  static bool FfmpegAvailable();
  /// @return ffmpeg version string, or empty if not found.
  static std::string FfmpegVersion();

 protected:
  std::string path_;
  double scale_factor_;
  FILE* pipe_{nullptr};
  int width_{0};
  int height_{0};
  double fps_{25.0};
  uint64_t frame_count_{0};

  /// Run ffprobe on path_ to populate width_, height_, fps_.
  void ProbeVideo();
};

// ── CameraSource
// ──────────────────────────────────────────────────────────────
/// Webcam feed delivered via an ffmpeg pipe.
/// macOS uses -f avfoundation; Linux uses -f v4l2.
class CameraSource : public FfmpegVideoSource {
 public:
  /// @param device  macOS: AVFoundation device index ("0", "1", …).
  ///                Linux: V4L2 device path ("/dev/video0", …).
  explicit CameraSource(std::string device = "0");
  bool Open() override;
};

// ── VideoPlayer
// ───────────────────────────────────────────────────────────────

/// Render modes for VideoPlayer.
enum class VideoRenderMode { Braille, ColorBlock, GrayBlock };

/// Configuration for VideoPlayer.
struct VideoPlayerConfig {
  VideoRenderMode mode = VideoRenderMode::Braille;
  bool loop = false;
  bool show_controls = true;
  bool show_timestamp = true;
  bool show_fps = false;
  double playback_speed = 1.0;
};

class VideoPlayer {
 public:
  /// Convenience aliases.
  using RenderMode = VideoRenderMode;
  using Config = VideoPlayerConfig;

  explicit VideoPlayer(std::shared_ptr<VideoSource> source,
                       VideoPlayerConfig cfg = VideoPlayerConfig{});
  ~VideoPlayer();

  /// Build the interactive FTXUI component.
  /// Keyboard bindings: Space=play/pause, Q=stop, L=toggle loop,
  /// B=cycle render mode.
  ftxui::Component Build();

  void Play();
  void Pause();
  void Stop();
  bool IsPlaying() const;

  void SetRenderMode(RenderMode mode);
  void SetLoop(bool loop);
  double CurrentTime() const;
  uint64_t FrameCount() const;

 private:
  std::shared_ptr<VideoSource> source_;
  Config cfg_;
  VideoFrame current_frame_;
  std::thread decode_thread_;
  std::atomic<bool> playing_{false};
  std::atomic<bool> stop_requested_{false};
  mutable std::mutex frame_mutex_;
  double current_time_{0.0};
  uint64_t frame_count_{0};

  void DecodeLoop();
};

// ── AsciiArtRenderer
// ──────────────────────────────────────────────────────────
/// Static helpers for rendering VideoFrame to FTXUI elements.
class AsciiArtRenderer {
 public:
  /// ASCII art string using characters from a luminance palette.
  static std::string Render(const VideoFrame& frame,
                            int cols,
                            int rows,
                            bool use_color = false);

  /// Braille-based rendering (highest detail, monochrome).
  static ftxui::Element RenderBraille(const VideoFrame& frame,
                                      int cols,
                                      int rows);

  /// Half-block rendering using ▀▄ (good colour fidelity).
  static ftxui::Element RenderBlocks(const VideoFrame& frame,
                                     int cols,
                                     int rows,
                                     bool colored = true);
};

// ── Free functions
// ────────────────────────────────────────────────────────────

/// One-liner: build a VideoPlayer component for a local file or URL.
ftxui::Component VideoPlayerFromFile(const std::string& path,
                                     VideoPlayer::Config cfg = {});

/// Live webcam feed component.
ftxui::Component CameraFeed(const std::string& device = "0",
                            VideoPlayer::Config cfg = {});

/// GIF player (ffmpeg decodes GIFs to raw frames).
ftxui::Component GifPlayer(const std::string& path, bool loop = true);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_VIDEO_HPP
