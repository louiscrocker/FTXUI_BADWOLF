// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/video.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace ftxui;
using namespace ftxui::ui;

// ── Mock source
// ───────────────────────────────────────────────────────────────

class MockVideoSource : public VideoSource {
 public:
  bool Open() override { return true; }
  void Close() override {}
  bool IsOpen() const override { return true; }
  bool ReadFrame(VideoFrame& out) override {
    out = VideoFrame{4, 4, std::vector<uint8_t>(48, 128), 0.0, 0};
    return true;
  }
  double fps() const override { return 30.0; }
  int width() const override { return 4; }
  int height() const override { return 4; }
};

// ── VideoFrame tests
// ──────────────────────────────────────────────────────────

TEST(VideoFrameTest, DefaultConstruction) {
  VideoFrame f;
  EXPECT_EQ(f.width, 0);
  EXPECT_EQ(f.height, 0);
  EXPECT_TRUE(f.pixels.empty());
  EXPECT_DOUBLE_EQ(f.timestamp, 0.0);
  EXPECT_EQ(f.frame_num, 0u);
  EXPECT_TRUE(f.empty());
}

TEST(VideoFrameTest, GrayAtCorrect) {
  // Pure red pixel → luminance ≈ 0.299 * 255 ≈ 76
  VideoFrame f;
  f.width = 1;
  f.height = 1;
  f.pixels = {255, 0, 0};
  const uint8_t gray = f.GrayAt(0, 0);
  EXPECT_NEAR(static_cast<int>(gray), static_cast<int>(0.299f * 255), 2);
}

TEST(VideoFrameTest, GrayAtGreenChannel) {
  // Pure green → luminance ≈ 0.587 * 255 ≈ 150
  VideoFrame f;
  f.width = 1;
  f.height = 1;
  f.pixels = {0, 255, 0};
  const uint8_t gray = f.GrayAt(0, 0);
  EXPECT_NEAR(static_cast<int>(gray), static_cast<int>(0.587f * 255), 2);
}

TEST(VideoFrameTest, ScaleProducesCorrectDimensions) {
  VideoFrame f;
  f.width = 8;
  f.height = 8;
  f.pixels.assign(8 * 8 * 3, 200);

  const VideoFrame scaled = f.Scale(4, 4);
  EXPECT_EQ(scaled.width, 4);
  EXPECT_EQ(scaled.height, 4);
  EXPECT_EQ(static_cast<int>(scaled.pixels.size()), 4 * 4 * 3);
}

TEST(VideoFrameTest, ScalePreservesPixelData) {
  VideoFrame f;
  f.width = 2;
  f.height = 2;
  // Solid colour: bright magenta
  f.pixels = {255, 0, 255, 255, 0, 255, 255, 0, 255, 255, 0, 255};

  const VideoFrame scaled = f.Scale(4, 4);
  EXPECT_EQ(scaled.pixels[0], 255);
  EXPECT_EQ(scaled.pixels[1], 0);
  EXPECT_EQ(scaled.pixels[2], 255);
}

TEST(VideoFrameTest, ToBrailleReturnsNonNull) {
  VideoFrame f;
  f.width = 8;
  f.height = 8;
  f.pixels.assign(8 * 8 * 3, 200);

  auto elem = f.ToBraille(4, 4);
  EXPECT_NE(elem, nullptr);
}

TEST(VideoFrameTest, ToBrailleEmptyFrame) {
  VideoFrame f;
  auto elem = f.ToBraille(4, 4);
  EXPECT_NE(elem, nullptr);
}

TEST(VideoFrameTest, ToColoredBlockReturnsNonNull) {
  VideoFrame f;
  f.width = 4;
  f.height = 4;
  f.pixels.assign(4 * 4 * 3, 100);

  auto elem = f.ToColoredBlock(2, 2);
  EXPECT_NE(elem, nullptr);
}

// ── FfmpegVideoSource tests
// ───────────────────────────────────────────────────

TEST(FfmpegVideoSourceTest, ConstructsWithoutCrash) {
  FfmpegVideoSource src("/nonexistent/video.mp4");
  EXPECT_FALSE(src.IsOpen());
}

TEST(FfmpegVideoSourceTest, FfmpegAvailableReturnsBool) {
  // Must not crash; result depends on the host environment.
  const bool avail = FfmpegVideoSource::FfmpegAvailable();
  (void)avail;
  SUCCEED();
}

TEST(FfmpegVideoSourceTest, OpenOnNonexistentFileFails) {
  FfmpegVideoSource src("/nonexistent_path_xyz/video.mp4");
  // Open may or may not return false immediately (depends on ffmpeg);
  // ReadFrame must return false.
  if (!FfmpegVideoSource::FfmpegAvailable()) {
    GTEST_SKIP() << "ffmpeg not available";
  }
  // Even if popen succeeds, ffmpeg will exit quickly → ReadFrame returns false.
  bool opened = src.Open();
  if (opened) {
    VideoFrame frame;
    const bool got_frame = src.ReadFrame(frame);
    EXPECT_FALSE(got_frame);
  } else {
    EXPECT_FALSE(src.IsOpen());
  }
  src.Close();
}

// ── CameraSource tests
// ────────────────────────────────────────────────────────

TEST(CameraSourceTest, ConstructsWithoutCrash) {
  CameraSource cam("0");
  EXPECT_FALSE(cam.IsOpen());
}

// ── VideoPlayer tests
// ─────────────────────────────────────────────────────────

TEST(VideoPlayerTest, ConstructsWithMockSource) {
  auto source = std::make_shared<MockVideoSource>();
  VideoPlayer player(source);
  SUCCEED();
}

TEST(VideoPlayerTest, IsPlayingFalseInitially) {
  auto source = std::make_shared<MockVideoSource>();
  VideoPlayer player(source);
  EXPECT_FALSE(player.IsPlaying());
}

TEST(VideoPlayerTest, BuildReturnsNonNull) {
  auto source = std::make_shared<MockVideoSource>();
  VideoPlayer player(source);
  auto component = player.Build();
  EXPECT_NE(component, nullptr);
}

TEST(VideoPlayerTest, CurrentTimeInitiallyZero) {
  auto source = std::make_shared<MockVideoSource>();
  VideoPlayer player(source);
  EXPECT_DOUBLE_EQ(player.CurrentTime(), 0.0);
}

TEST(VideoPlayerTest, FrameCountInitiallyZero) {
  auto source = std::make_shared<MockVideoSource>();
  VideoPlayer player(source);
  EXPECT_EQ(player.FrameCount(), 0u);
}

// ── AsciiArtRenderer tests
// ────────────────────────────────────────────────────

TEST(AsciiArtRendererTest, RenderBrailleWithEmptyFrame) {
  VideoFrame f;
  auto elem = AsciiArtRenderer::RenderBraille(f, 10, 5);
  EXPECT_NE(elem, nullptr);
}

TEST(AsciiArtRendererTest, RenderBlocksWithEmptyFrame) {
  VideoFrame f;
  auto elem = AsciiArtRenderer::RenderBlocks(f, 10, 5);
  EXPECT_NE(elem, nullptr);
}

TEST(AsciiArtRendererTest, RenderBrailleWithData) {
  VideoFrame f;
  f.width = 8;
  f.height = 8;
  f.pixels.assign(8 * 8 * 3, 200);
  auto elem = AsciiArtRenderer::RenderBraille(f, 4, 2);
  EXPECT_NE(elem, nullptr);
}

TEST(AsciiArtRendererTest, RenderBlocksColoredWithData) {
  VideoFrame f;
  f.width = 4;
  f.height = 4;
  f.pixels.assign(4 * 4 * 3, 128);
  auto elem = AsciiArtRenderer::RenderBlocks(f, 4, 4, true);
  EXPECT_NE(elem, nullptr);
}

// ── Free function tests
// ───────────────────────────────────────────────────────

TEST(VideoFreeFunction, VideoPlayerFromFileReturnsNonNull) {
  // Must return a valid component even if ffmpeg is absent.
  auto comp = VideoPlayerFromFile("nonexistent.mp4");
  EXPECT_NE(comp, nullptr);
}
