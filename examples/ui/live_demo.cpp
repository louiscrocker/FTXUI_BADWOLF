// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/live_source.hpp"

using namespace ftxui;

class MockPrometheusSource : public ui::PrometheusSource {
 public:
  MockPrometheusSource() : PrometheusSource("localhost", 9090, "/metrics") {}

 protected:
  std::vector<ui::PrometheusMetric> Fetch() override {
    std::vector<ui::PrometheusMetric> metrics;

    ui::PrometheusMetric cpu;
    cpu.name = "cpu_usage_percent";
    cpu.help = "CPU usage percentage";
    cpu.type = "gauge";
    cpu.samples.push_back({"", 45.2});
    metrics.push_back(cpu);

    ui::PrometheusMetric memory;
    memory.name = "memory_bytes";
    memory.help = "Memory usage in bytes";
    memory.type = "gauge";
    memory.samples.push_back({"type=\"used\"", 8589934592.0});
    memory.samples.push_back({"type=\"free\"", 4294967296.0});
    metrics.push_back(memory);

    ui::PrometheusMetric requests;
    requests.name = "http_requests_total";
    requests.help = "Total HTTP requests";
    requests.type = "counter";
    requests.samples.push_back({"method=\"GET\"", 12345.0});
    requests.samples.push_back({"method=\"POST\"", 6789.0});
    metrics.push_back(requests);

    return metrics;
  }
};

int main() {
  // Tab 1: File Tail
  auto file_source =
      std::make_shared<ui::FileTailSource>("badwolf_demo.log", 1000);
  auto log_panel = ui::LiveLogPanel(file_source, 100);

  auto generate_button = Button("Generate Log", [&] {
    std::ofstream file("badwolf_demo.log", std::ios::app);
    file << "Generated log entry at " << std::time(nullptr) << "\n";
  });

  auto tab1 = Container::Vertical({
      generate_button,
      log_panel | flex,
  });

  // Tab 2: HTTP JSON
  auto http_source = std::make_shared<ui::HttpJsonSource>(
      "httpbin.org", 80, "/json", std::chrono::seconds(10));
  auto json_viewer = ui::LiveJsonViewer(http_source);

  auto tab2 = Container::Vertical({
      Renderer([] { return text("Polling httpbin.org/json every 10s") | bold; }),
      json_viewer | flex,
  });

  // Tab 3: Prometheus Metrics
  auto prom_source = std::make_shared<MockPrometheusSource>();
  auto metrics_table = ui::LiveMetricsTable(prom_source);

  auto tab3 = Container::Vertical({
      Renderer(
          [] { return text("Mock Prometheus Metrics (no network)") | bold; }),
      metrics_table | flex,
  });

  // Tab 4: Stdin
  auto stdin_source = std::make_shared<ui::StdinSource>();
  auto stdin_lines = std::make_shared<std::vector<std::string>>();
  auto stdin_mutex = std::make_shared<std::mutex>();

  stdin_source->OnData([stdin_lines, stdin_mutex](const std::string& data) {
    if (!data.empty()) {
      std::lock_guard<std::mutex> lock(*stdin_mutex);
      stdin_lines->push_back(data);
      if (stdin_lines->size() > 50) {
        stdin_lines->erase(stdin_lines->begin());
      }
    }
  });

  auto stdin_viewer = Renderer([stdin_lines, stdin_mutex] {
    std::lock_guard<std::mutex> lock(*stdin_mutex);

    std::vector<Element> lines;
    lines.push_back(text("Stdin Input (pipe data to this program):") | bold);
    lines.push_back(separator());

    if (stdin_lines->empty()) {
      lines.push_back(text("No input yet...") | dim);
    } else {
      for (const auto& line : *stdin_lines) {
        lines.push_back(text(line));
      }
    }

    return vbox(std::move(lines)) | border | flex;
  });

  stdin_source->Start();

  auto tab4 = stdin_viewer;

  // Tab selection
  int tab_index = 0;
  std::vector<std::string> tab_names = {"File Tail", "HTTP", "Prometheus",
                                        "Stdin"};
  auto tab_menu = Menu(&tab_names, &tab_index, MenuOption::HorizontalAnimated());

  auto tab_container = Container::Tab(
      {
          tab1,
          tab2,
          tab3,
          tab4,
      },
      &tab_index);

  auto main_container = Container::Vertical({
      tab_menu,
      tab_container | flex,
  });

  auto screen = ScreenInteractive::Fullscreen();
  screen.Loop(main_container);

  return 0;
}
