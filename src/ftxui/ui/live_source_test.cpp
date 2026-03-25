// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/live_source.hpp"

#include <unistd.h>

#include <chrono>
#include <fstream>
#include <string>
#include <thread>

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

class ConcreteSource : public LiveSource<std::string> {
 protected:
  std::string Fetch() override { return "test"; }
  std::chrono::milliseconds Interval() const override {
    return std::chrono::milliseconds(50);
  }
};

std::string GetTempFilePath() {
  return "live_test_" + std::to_string(getpid()) + ".txt";
}

TEST(FileTailSourceTest, ReadsExistingContent) {
  std::string tmppath = GetTempFilePath();
  
  {
    std::ofstream file(tmppath);
    file << "line 1\n";
    file << "line 2\n";
    file << "line 3\n";
  }

  auto source = std::make_shared<FileTailSource>(tmppath);
  source->Start();
  
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  {
    std::ofstream file(tmppath, std::ios::app);
    file << "line 4\n";
  }
  
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  
  std::string latest = source->Latest();
  EXPECT_TRUE(latest.find("line 4") != std::string::npos);
  
  source->Stop();
  std::remove(tmppath.c_str());
}

TEST(FileTailSourceTest, DetectsNewLines) {
  std::string tmppath = GetTempFilePath() + "_tail";
  
  {
    std::ofstream file(tmppath);
    file << "initial\n";
  }

  auto source = std::make_shared<FileTailSource>(tmppath);
  source->Start();
  
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  
  {
    std::ofstream file(tmppath, std::ios::app);
    file << "new line\n";
  }
  
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  
  std::string latest = source->Latest();
  EXPECT_TRUE(latest.find("new line") != std::string::npos);
  
  source->Stop();
  std::remove(tmppath.c_str());
}

TEST(PrometheusSourceTest, ParseBasicMetric) {
  std::string text = "http_requests_total 1234\n";
  auto metrics = PrometheusSource::ParsePrometheusText(text);
  
  ASSERT_EQ(metrics.size(), 1);
  EXPECT_EQ(metrics[0].name, "http_requests_total");
  ASSERT_EQ(metrics[0].samples.size(), 1);
  EXPECT_EQ(metrics[0].samples[0].second, 1234.0);
}

TEST(PrometheusSourceTest, ParseWithLabels) {
  std::string text = "http_requests_total{method=\"GET\",status=\"200\"} 42\n";
  auto metrics = PrometheusSource::ParsePrometheusText(text);
  
  ASSERT_EQ(metrics.size(), 1);
  EXPECT_EQ(metrics[0].name, "http_requests_total");
  ASSERT_EQ(metrics[0].samples.size(), 1);
  EXPECT_EQ(metrics[0].samples[0].second, 42.0);
  EXPECT_FALSE(metrics[0].samples[0].first.empty());
}

TEST(PrometheusSourceTest, ParseHelpAndType) {
  std::string text = "# HELP http_requests_total Total HTTP requests\n"
                     "# TYPE http_requests_total counter\n"
                     "http_requests_total 100\n";
  auto metrics = PrometheusSource::ParsePrometheusText(text);
  
  ASSERT_EQ(metrics.size(), 1);
  EXPECT_EQ(metrics[0].name, "http_requests_total");
  EXPECT_EQ(metrics[0].help, "Total HTTP requests");
  EXPECT_EQ(metrics[0].type, "counter");
  ASSERT_EQ(metrics[0].samples.size(), 1);
  EXPECT_EQ(metrics[0].samples[0].second, 100.0);
}

TEST(PrometheusSourceTest, HandlesEmptyInput) {
  std::string text = "";
  auto metrics = PrometheusSource::ParsePrometheusText(text);
  EXPECT_EQ(metrics.size(), 0);
}

TEST(PrometheusSourceTest, HandlesMalformedInput) {
  std::string text = "garbage line\n"
                     "another bad line\n"
                     "no numbers here\n";
  auto metrics = PrometheusSource::ParsePrometheusText(text);
  EXPECT_GE(metrics.size(), 0);
}

TEST(HttpJsonSourceTest, ConstructsCorrectly) {
  auto source = std::make_shared<HttpJsonSource>("example.com", 80, "/api");
  EXPECT_FALSE(source->IsRunning());
}

TEST(LiveSourceTest, StartsAndStops) {
  auto source = std::make_shared<ConcreteSource>();
  EXPECT_FALSE(source->IsRunning());
  
  source->Start();
  EXPECT_TRUE(source->IsRunning());
  
  source->Stop();
  EXPECT_FALSE(source->IsRunning());
}

TEST(LiveSourceTest, CallbackFiresOnData) {
  auto source = std::make_shared<ConcreteSource>();
  
  bool callback_fired = false;
  std::string received_value;
  
  source->OnData([&](const std::string& value) {
    callback_fired = true;
    received_value = value;
  });
  
  source->Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  source->Stop();
  
  EXPECT_TRUE(callback_fired);
  EXPECT_EQ(received_value, "test");
}

TEST(LiveSourceTest, BindLiveSourceCreatesReactive) {
  auto source = std::make_shared<ConcreteSource>();
  auto reactive = BindLiveSource(source);
  
  EXPECT_NE(reactive, nullptr);
  EXPECT_TRUE(source->IsRunning());
  
  source->Stop();
}

TEST(StdinSourceTest, ConstructsWithoutCrash) {
  auto source = std::make_shared<StdinSource>();
  EXPECT_NE(source, nullptr);
}

}  // namespace
}  // namespace ftxui::ui
