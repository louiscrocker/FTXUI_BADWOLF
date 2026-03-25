// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/live_source.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/charts.hpp"
#include "ftxui/ui/log_panel.hpp"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace ftxui::ui {

// ── HTTP JSON Source ───────────────────────────────────────────────────────

HttpJsonSource::HttpJsonSource(std::string host,
                               int port,
                               std::string path,
                               std::chrono::milliseconds interval)
    : host_(std::move(host)),
      port_(port),
      path_(std::move(path)),
      interval_(interval) {}

std::string HttpJsonSource::Fetch() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return "";
  }

  struct hostent* server = gethostbyname(host_.c_str());
  if (!server) {
    close(sock);
    return "";
  }

  struct sockaddr_in serv_addr{};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_);
  std::memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    close(sock);
    return "";
  }

  std::string request = "GET " + path_ + " HTTP/1.0\r\n" + "Host: " + host_ +
                        "\r\n" + "Connection: close\r\n\r\n";

  if (send(sock, request.c_str(), request.size(), MSG_NOSIGNAL) < 0) {
    close(sock);
    return "";
  }

  std::string response;
  char buffer[4096];
  ssize_t bytes;
  while ((bytes = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
    response.append(buffer, bytes);
  }

  close(sock);

  size_t body_pos = response.find("\r\n\r\n");
  if (body_pos != std::string::npos) {
    return response.substr(body_pos + 4);
  }

  return "";
}

// ── Prometheus Source ──────────────────────────────────────────────────────

PrometheusSource::PrometheusSource(std::string host, int port, std::string path)
    : host_(std::move(host)), path_(std::move(path)), port_(port) {}

std::vector<PrometheusMetric> PrometheusSource::Fetch() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return {};
  }

  struct hostent* server = gethostbyname(host_.c_str());
  if (!server) {
    close(sock);
    return {};
  }

  struct sockaddr_in serv_addr{};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_);
  std::memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    close(sock);
    return {};
  }

  std::string request = "GET " + path_ + " HTTP/1.0\r\n" + "Host: " + host_ +
                        "\r\n" + "Connection: close\r\n\r\n";

  if (send(sock, request.c_str(), request.size(), MSG_NOSIGNAL) < 0) {
    close(sock);
    return {};
  }

  std::string response;
  char buffer[4096];
  ssize_t bytes;
  while ((bytes = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
    response.append(buffer, bytes);
  }

  close(sock);

  size_t body_pos = response.find("\r\n\r\n");
  if (body_pos != std::string::npos) {
    return ParsePrometheusText(response.substr(body_pos + 4));
  }

  return {};
}

std::vector<PrometheusMetric> PrometheusSource::ParsePrometheusText(
    const std::string& text) {
  std::vector<PrometheusMetric> metrics;
  std::map<std::string, PrometheusMetric> metric_map;

  std::istringstream stream(text);
  std::string line;

  while (std::getline(stream, line)) {
    if (line.empty() || line[0] == '#') {
      if (line.find("# HELP") == 0) {
        std::istringstream iss(line);
        std::string hash, help_kw, name;
        iss >> hash >> help_kw >> name;
        std::string help_text;
        std::getline(iss, help_text);
        if (!help_text.empty() && help_text[0] == ' ') {
          help_text = help_text.substr(1);
        }
        metric_map[name].name = name;
        metric_map[name].help = help_text;
      } else if (line.find("# TYPE") == 0) {
        std::istringstream iss(line);
        std::string hash, type_kw, name, type;
        iss >> hash >> type_kw >> name >> type;
        metric_map[name].name = name;
        metric_map[name].type = type;
      }
      continue;
    }

    size_t brace_pos = line.find('{');
    size_t space_pos = line.find(' ');

    if (brace_pos != std::string::npos && brace_pos < space_pos) {
      std::string name = line.substr(0, brace_pos);
      size_t close_brace = line.find('}', brace_pos);
      if (close_brace == std::string::npos) {
        continue;
      }

      std::string labels =
          line.substr(brace_pos + 1, close_brace - brace_pos - 1);
      std::string rest = line.substr(close_brace + 1);

      std::istringstream iss(rest);
      double value;
      iss >> value;

      metric_map[name].name = name;
      metric_map[name].samples.push_back({labels, value});
    } else if (space_pos != std::string::npos) {
      std::string name = line.substr(0, space_pos);
      std::string rest = line.substr(space_pos + 1);

      std::istringstream iss(rest);
      double value;
      iss >> value;

      metric_map[name].name = name;
      metric_map[name].samples.push_back({"", value});
    }
  }

  for (auto& [name, metric] : metric_map) {
    metrics.push_back(std::move(metric));
  }

  return metrics;
}

// ── File Tail Source ───────────────────────────────────────────────────────

FileTailSource::FileTailSource(std::string filepath, int max_lines)
    : filepath_(std::move(filepath)), max_lines_(max_lines) {}

std::string FileTailSource::Fetch() {
  std::ifstream file(filepath_);
  if (!file.is_open()) {
    return "";
  }

  if (last_pos_ == std::streampos(0)) {
    file.seekg(0, std::ios::end);
    last_pos_ = file.tellg();
    return "";
  }

  file.seekg(last_pos_);

  std::string new_content;
  std::string line;
  int line_count = 0;

  while (std::getline(file, line) && line_count < max_lines_) {
    if (!new_content.empty()) {
      new_content += "\n";
    }
    new_content += line;
    line_count++;
  }

  // tellg() returns -1 when eofbit is set with no new data; preserve position.
  auto new_pos = file.tellg();
  if (new_pos != std::streampos(-1)) {
    last_pos_ = new_pos;
  }

  return new_content;
}

// ── Stdin Source ───────────────────────────────────────────────────────────

StdinSource::StdinSource() = default;

std::string StdinSource::Fetch() {
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  struct timeval tv{};
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  int ret = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &tv);
  if (ret <= 0) {
    return "";
  }

  std::string result;
  char buffer[1024];
  ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
  if (bytes > 0) {
    buffer[bytes] = '\0';
    result = buffer;
  }

  return result;
}

// ── Widget Factories ───────────────────────────────────────────────────────

Component LiveLogPanel(std::shared_ptr<FileTailSource> source, int max_lines) {
  auto log = LogPanel::Create(max_lines);

  source->OnData([log](const std::string& content) {
    if (content.empty()) {
      return;
    }

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
      if (!line.empty()) {
        log->Info(line);
      }
    }
  });

  if (!source->IsRunning()) {
    source->Start();
  }

  return log->Build("Live Log");
}

Component LiveMetricsTable(std::shared_ptr<PrometheusSource> source) {
  auto metrics = std::make_shared<std::vector<PrometheusMetric>>();
  auto mutex = std::make_shared<std::mutex>();

  source->OnData([metrics, mutex](const std::vector<PrometheusMetric>& data) {
    std::lock_guard<std::mutex> lock(*mutex);
    *metrics = data;
  });

  if (!source->IsRunning()) {
    source->Start();
  }

  return Renderer([metrics, mutex]() -> Element {
    std::lock_guard<std::mutex> lock(*mutex);

    std::vector<Element> rows;
    rows.push_back(hbox({
        text("Metric") | bold | size(WIDTH, EQUAL, 30),
        separator(),
        text("Type") | bold | size(WIDTH, EQUAL, 15),
        separator(),
        text("Value") | bold | size(WIDTH, EQUAL, 20),
    }));
    rows.push_back(separator());

    for (const auto& metric : *metrics) {
      for (const auto& [label, value] : metric.samples) {
        std::string label_str = label.empty() ? "" : "{" + label + "}";
        rows.push_back(hbox({
            text(metric.name + label_str) | size(WIDTH, EQUAL, 30),
            separator(),
            text(metric.type) | size(WIDTH, EQUAL, 15),
            separator(),
            text(std::to_string(value)) | size(WIDTH, EQUAL, 20),
        }));
      }
    }

    return vbox(std::move(rows)) | border;
  });
}

Component LiveLineChart(std::shared_ptr<LiveSource<double>> source,
                        const std::string& title,
                        int history) {
  auto buffer = std::make_shared<std::vector<double>>();
  auto mutex = std::make_shared<std::mutex>();
  buffer->reserve(history);

  source->OnData([buffer, mutex, history](double value) {
    std::lock_guard<std::mutex> lock(*mutex);
    buffer->push_back(value);
    if (static_cast<int>(buffer->size()) > history) {
      buffer->erase(buffer->begin());
    }
  });

  if (!source->IsRunning()) {
    source->Start();
  }

  return Renderer([buffer, mutex, title, history]() -> Element {
    std::lock_guard<std::mutex> lock(*mutex);

    if (buffer->empty()) {
      if (!title.empty()) {
        return vbox({
                   text(title) | bold,
                   separator(),
                   text("Waiting for data..."),
               }) |
               border;
      }
      return text("Waiting for data...") | border;
    }

    std::vector<float> float_buffer;
    float_buffer.reserve(buffer->size());
    for (double val : *buffer) {
      float_buffer.push_back(static_cast<float>(val));
    }

    Element chart = Sparkline(float_buffer, history, Color::Green);

    if (!title.empty()) {
      return vbox({
                 text(title) | bold,
                 separator(),
                 chart,
             }) |
             border;
    }

    return chart | border;
  });
}

Component LiveJsonViewer(std::shared_ptr<HttpJsonSource> source) {
  auto content = std::make_shared<std::string>("");
  auto mutex = std::make_shared<std::mutex>();

  source->OnData([content, mutex](const std::string& data) {
    std::lock_guard<std::mutex> lock(*mutex);
    *content = data;
  });

  if (!source->IsRunning()) {
    source->Start();
  }

  return Renderer([content, mutex]() -> Element {
    std::lock_guard<std::mutex> lock(*mutex);

    if (content->empty()) {
      return text("Loading...") | center | border;
    }

    return vbox({
               text("JSON Response:") | bold,
               separator(),
               text(*content),
           }) |
           border | vscroll_indicator | frame | flex;
  });
}

}  // namespace ftxui::ui
