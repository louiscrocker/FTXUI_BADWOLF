// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/charts.hpp"
#include "ftxui/ui/live_source.hpp"

using namespace ftxui;

#ifdef __APPLE__
#define IS_MACOS 1
#else
#define IS_MACOS 0
#endif

class CpuSource : public ui::LiveSource<double> {
 protected:
  double Fetch() override {
#if IS_MACOS
    FILE* pipe = popen("top -l 2 -n 0 | grep 'CPU usage' | tail -1", "r");
    if (!pipe)
      return 0.0;

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
      result += buffer;
    }
    pclose(pipe);

    size_t user_pos = result.find("user");
    if (user_pos != std::string::npos) {
      size_t start = result.rfind(' ', user_pos - 2);
      if (start != std::string::npos) {
        std::string num = result.substr(start + 1, user_pos - start - 2);
        try {
          return std::stod(num);
        } catch (...) {
          return 0.0;
        }
      }
    }
    return 0.0;
#else
    FILE* pipe = popen("cat /proc/stat 2>/dev/null | grep '^cpu ' | head -1", "r");
    if (!pipe)
      return 0.0;

    char buffer[256];
    if (!fgets(buffer, sizeof(buffer), pipe)) {
      pclose(pipe);
      return 0.0;
    }
    pclose(pipe);

    unsigned long long user, nice, system, idle;
    if (sscanf(buffer, "cpu %llu %llu %llu %llu", &user, &nice, &system,
               &idle) == 4) {
      static unsigned long long prev_total = 0;
      static unsigned long long prev_idle = 0;

      unsigned long long total = user + nice + system + idle;
      unsigned long long diff_total = total - prev_total;
      unsigned long long diff_idle = idle - prev_idle;

      prev_total = total;
      prev_idle = idle;

      if (diff_total == 0)
        return 0.0;

      return 100.0 * (1.0 - static_cast<double>(diff_idle) / diff_total);
    }
    return 0.0;
#endif
  }

  std::chrono::milliseconds Interval() const override {
    return std::chrono::milliseconds(2000);
  }
};

class MemorySource : public ui::LiveSource<std::string> {
 protected:
  std::string Fetch() override {
#if IS_MACOS
    FILE* pipe = popen("vm_stat 2>/dev/null", "r");
    if (!pipe)
      return "Memory: N/A";

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
      result += buffer;
    }
    pclose(pipe);

    if (result.find("Pages free") != std::string::npos) {
      return "Memory info available";
    }
    return "Memory: N/A";
#else
    FILE* pipe = popen("cat /proc/meminfo 2>/dev/null | head -3", "r");
    if (!pipe)
      return "Memory: N/A";

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
      result += buffer;
    }
    pclose(pipe);

    return result.empty() ? "Memory: N/A" : result;
#endif
  }

  std::chrono::milliseconds Interval() const override {
    return std::chrono::milliseconds(2000);
  }
};

class ProcessListSource : public ui::LiveSource<std::string> {
 protected:
  std::string Fetch() override {
    FILE* pipe = popen("ps aux 2>/dev/null | sort -k3 -rn | head -15", "r");
    if (!pipe)
      return "Process list unavailable";

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
      result += buffer;
    }
    pclose(pipe);

    return result.empty() ? "Process list unavailable" : result;
  }

  std::chrono::milliseconds Interval() const override {
    return std::chrono::milliseconds(2000);
  }
};

int main() {
  auto cpu_source = std::make_shared<CpuSource>();
  auto cpu_chart = ui::LiveLineChart(cpu_source, "CPU Usage %", 60);

  auto memory_source = std::make_shared<MemorySource>();
  auto memory_text = std::make_shared<std::string>();
  auto memory_mutex = std::make_shared<std::mutex>();

  memory_source->OnData([memory_text, memory_mutex](const std::string& data) {
    std::lock_guard<std::mutex> lock(*memory_mutex);
    *memory_text = data;
  });
  memory_source->Start();

  auto memory_viewer = Renderer([memory_text, memory_mutex] {
    std::lock_guard<std::mutex> lock(*memory_mutex);
    return vbox({
               text("Memory") | bold,
               separator(),
               text(*memory_text),
           }) |
           border;
  });

  auto process_source = std::make_shared<ProcessListSource>();
  auto process_text = std::make_shared<std::string>();
  auto process_mutex = std::make_shared<std::mutex>();

  process_source->OnData([process_text, process_mutex](const std::string& data) {
    std::lock_guard<std::mutex> lock(*process_mutex);
    *process_text = data;
  });
  process_source->Start();

  auto process_viewer = Renderer([process_text, process_mutex] {
    std::lock_guard<std::mutex> lock(*process_mutex);
    return vbox({
               text("Top Processes (by CPU)") | bold,
               separator(),
               text(*process_text),
           }) |
           border | flex;
  });

  auto main_container = Container::Vertical({
      cpu_chart,
      memory_viewer,
      process_viewer | flex,
  });

  auto renderer = Renderer(main_container, [main_container] {
    return vbox({
               text("System Monitor") | bold | center,
               separator(),
               main_container->Render() | flex,
           }) |
           border;
  });

  auto screen = ScreenInteractive::Fullscreen();
  screen.Loop(renderer);

  return 0;
}
