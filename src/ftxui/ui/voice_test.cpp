// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/voice.hpp"

#include <memory>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── VoiceConfig
// ────────────────────────────────────────────────────────────────

TEST(VoiceConfigTest, Defaults) {
  VoiceConfig cfg;
  EXPECT_EQ(cfg.whisper_path, "whisper");
  EXPECT_EQ(cfg.model_path, "ggml-base.bin");
  EXPECT_EQ(cfg.language, "en");
  EXPECT_EQ(cfg.sample_rate, 16000);
  EXPECT_EQ(cfg.channels, 1);
  EXPECT_FLOAT_EQ(cfg.silence_threshold, 0.01f);
  EXPECT_EQ(cfg.continuous, false);
  EXPECT_EQ(cfg.echo_transcription, true);
}

// ── AudioCapture
// ──────────────────────────────────────────────────────────────

TEST(AudioCaptureTest, ConstructsWithoutCrash) {
  EXPECT_NO_THROW({ AudioCapture cap; });
}

TEST(AudioCaptureTest, ConstructsWithConfig) {
  VoiceConfig cfg;
  cfg.sample_rate = 44100;
  EXPECT_NO_THROW({ AudioCapture cap(cfg); });
}

TEST(AudioCaptureTest, IsRecordingFalseInitially) {
  AudioCapture cap;
  EXPECT_FALSE(cap.IsRecording());
}

TEST(AudioCaptureTest, CurrentLevelZeroInitially) {
  AudioCapture cap;
  EXPECT_FLOAT_EQ(cap.CurrentLevel(), 0.0f);
}

TEST(AudioCaptureTest, MicrophoneAvailableReturnsBool) {
  // Should not crash — result depends on environment
  bool result = false;
  EXPECT_NO_THROW({ result = AudioCapture::MicrophoneAvailable(); });
  (void)result;
}

// ── WhisperTranscriber
// ────────────────────────────────────────────────────────

TEST(WhisperTranscriberTest, ConstructsWithoutCrash) {
  EXPECT_NO_THROW({ WhisperTranscriber wt; });
}

TEST(WhisperTranscriberTest, WhisperAvailableReturnsBool) {
  bool result = false;
  EXPECT_NO_THROW({ result = WhisperTranscriber::WhisperAvailable(); });
  (void)result;
}

TEST(WhisperTranscriberTest, TranscribeNonExistentFileReturnsFailure) {
  WhisperTranscriber wt;
  auto result = wt.Transcribe("/nonexistent/path/audio.wav");
  // Should not crash and must report failure gracefully
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error.empty());
}

TEST(WhisperTranscriberTest, TranscribeEmptyPathReturnsFailure) {
  WhisperTranscriber wt;
  auto result = wt.Transcribe("");
  EXPECT_FALSE(result.success);
}

// ── VoiceCommandEngine
// ────────────────────────────────────────────────────────

TEST(VoiceCommandEngineTest, ConstructsWithoutCrash) {
  EXPECT_NO_THROW({ VoiceCommandEngine engine; });
}

TEST(VoiceCommandEngineTest, StatusIsIdleInitially) {
  VoiceCommandEngine engine;
  EXPECT_EQ(engine.status(), VoiceStatus::Idle);
}

TEST(VoiceCommandEngineTest, IsListeningFalseInitially) {
  VoiceCommandEngine engine;
  EXPECT_FALSE(engine.IsListening());
}

TEST(VoiceCommandEngineTest, OnCommandRegistersHandler) {
  VoiceCommandEngine engine;
  bool called = false;
  EXPECT_NO_THROW({
    engine.OnCommand("table", [&called](const std::string&) { called = true; });
  });
  (void)called;
}

TEST(VoiceCommandEngineTest, OnTranscriptionRegistersHandler) {
  VoiceCommandEngine engine;
  bool called = false;
  EXPECT_NO_THROW({
    engine.OnTranscription([&called](const std::string&) { called = true; });
  });
  (void)called;
}

// ── VoiceInputBar
// ─────────────────────────────────────────────────────────────

TEST(VoiceInputBarTest, BuildsWithoutCrash) {
  ftxui::Component bar;
  EXPECT_NO_THROW({ bar = VoiceInputBar(); });
  EXPECT_NE(bar, nullptr);
}

TEST(VoiceInputBarTest, BuildsWithEngine) {
  auto engine = std::make_shared<VoiceCommandEngine>();
  ftxui::Component bar;
  EXPECT_NO_THROW({ bar = VoiceInputBar(engine); });
  EXPECT_NE(bar, nullptr);
}

// ── WithVoiceControl
// ──────────────────────────────────────────────────────────

TEST(WithVoiceControlTest, ReturnsNonNull) {
  auto inner = ftxui::Renderer([] { return ftxui::text("hello"); });
  ftxui::Component wrapped;
  EXPECT_NO_THROW({ wrapped = WithVoiceControl(inner); });
  EXPECT_NE(wrapped, nullptr);
}

// ── VoiceStatusIndicator
// ──────────────────────────────────────────────────────

TEST(VoiceStatusIndicatorTest, ReturnsNonNullForAllStatuses) {
  const VoiceStatus statuses[] = {
      VoiceStatus::Idle,  VoiceStatus::Listening, VoiceStatus::Processing,
      VoiceStatus::Ready, VoiceStatus::Error,     VoiceStatus::Unavailable,
  };
  for (auto status : statuses) {
    ftxui::Element elem;
    EXPECT_NO_THROW({ elem = VoiceStatusIndicator(status, 0.5f); });
    EXPECT_NE(elem, nullptr);
  }
}

}  // namespace
}  // namespace ftxui::ui
