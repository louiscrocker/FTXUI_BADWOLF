// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file collab_dashboard.cpp
///
/// Shared ops dashboard: all peers see the same live metrics view.
/// Clicking a metric on one client highlights it for ALL clients.
///
/// Server:  ./collab_dashboard --server [PORT]
/// Client:  ./collab_dashboard --client HOST[:PORT] --name NAME

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/collab.hpp"

using namespace ftxui;
using namespace ftxui::ui;

static constexpr int kDefaultPort = 7778;

// ── Fake metrics ─────────────────────────────────────────────────────────────

struct Metric {
  std::string name;
  std::string unit;
  double value = 0.0;
  double max_value = 100.0;
  Color color;
};

std::vector<Metric> GenerateMetrics(double t) {
  return {
      {"CPU",    "%",  40.0 + 30.0 * std::sin(t * 0.7),           100.0, Color::GreenLight},
      {"Memory", "%",  60.0 + 10.0 * std::sin(t * 0.3 + 1.0),    100.0, Color::CyanLight},
      {"Disk I/O","MB/s", 120.0 + 80.0 * std::abs(std::sin(t)),   400.0, Color::YellowLight},
      {"Network","Mb/s", 25.0 + 20.0 * std::sin(t * 1.1 + 0.5),  100.0, Color::BlueLight},
      {"Requests","/s", 820.0 + 180.0 * std::sin(t * 0.9),       1500.0, Color::MagentaLight},
      {"Errors",  "/s", 2.0 + 1.5 * std::abs(std::sin(t * 2.0)),   20.0, Color::RedLight},
  };
}

std::string FormatValue(double v, const std::string& unit) {
  std::ostringstream oss;
  oss.precision(1);
  oss << std::fixed << v << " " << unit;
  return oss.str();
}

// ── Dashboard renderer ────────────────────────────────────────────────────────

Element RenderDashboard(const std::vector<Metric>& metrics,
                         int selected,
                         int viewer_count,
                         const std::string& highlight_name) {
  Elements rows;
  rows.push_back(
      hbox({
          text(" ★  SHARED OPS DASHBOARD  ★") | bold | color(Color::CyanLight) | flex,
          text(" Viewers: ") | color(Color::GrayLight),
          text(std::to_string(viewer_count)) | bold | color(Color::GreenLight),
          text("  "),
      }) |
      border);

  for (int i = 0; i < static_cast<int>(metrics.size()); ++i) {
    const auto& m = metrics[i];
    bool is_selected = (i == selected);
    bool is_highlighted = (m.name == highlight_name);
    double pct = m.value / m.max_value;
    if (pct > 1.0) pct = 1.0;

    auto bar = hbox({
        text(" " + m.name) | bold |
            size(WIDTH, EQUAL, 12) |
            color(is_selected ? Color::White : m.color),
        gauge(static_cast<float>(pct)) | flex | color(m.color),
        text(" " + FormatValue(m.value, m.unit)) |
            size(WIDTH, EQUAL, 14) | color(Color::GrayLight),
    });

    if (is_selected) {
      bar = bar | inverted;
    }
    if (is_highlighted && !is_selected) {
      bar = bar | bold;
    }
    rows.push_back(bar | border);
  }

  if (!highlight_name.empty()) {
    rows.push_back(
        hbox({
            text(" Highlighted by peer: "),
            text(highlight_name) | bold | color(Color::YellowLight),
        }) |
        color(Color::GrayDark));
  }

  rows.push_back(
      text(" ↑/↓ = select | Enter = highlight for all | q = quit") |
      color(Color::GrayDark) | hcenter);

  return vbox(rows);
}

// ── Args ─────────────────────────────────────────────────────────────────────

struct Args {
  bool is_server = false;
  std::string host = "127.0.0.1";
  int port = kDefaultPort;
  std::string name = "Viewer";
};

Args ParseArgs(int argc, char** argv) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--server") {
      args.is_server = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        args.port = std::stoi(argv[++i]);
      }
    } else if (a == "--client" && i + 1 < argc) {
      std::string hp = argv[++i];
      auto colon = hp.rfind(':');
      if (colon != std::string::npos) {
        args.host = hp.substr(0, colon);
        args.port = std::stoi(hp.substr(colon + 1));
      } else {
        args.host = hp;
      }
    } else if (a == "--name" && i + 1 < argc) {
      args.name = argv[++i];
    }
  }
  return args;
}

// ── Shared dashboard logic (used by both server and client) ───────────────────

int RunDashboard(const Args& args) {
  // Server setup
  std::shared_ptr<CollabServer> server;
  if (args.is_server) {
    server = std::make_shared<CollabServer>(args.port);
    server->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  // Both server and client connect as a peer
  std::string connect_name = args.is_server ? "Host" : args.name;
  auto client =
      std::make_shared<CollabClient>(args.host, args.port, connect_name);

  if (!client->Connect()) {
    fprintf(stderr, "Cannot connect to %s:%d\n", args.host.c_str(), args.port);
    if (server) server->Stop();
    return 1;
  }

  // Shared state
  std::atomic<double> sim_time{0.0};
  std::atomic<int> selected{0};
  std::string highlight_name;
  std::mutex state_mutex;
  auto metrics = std::make_shared<std::vector<Metric>>(GenerateMetrics(0.0));

  // Metrics updater thread (server drives the metrics)
  std::atomic<bool> running{true};
  std::thread updater;
  if (args.is_server) {
    updater = std::thread([&]() {
      while (running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        double t = sim_time.load() + 0.25;
        sim_time.store(t);

        auto new_metrics = GenerateMetrics(t);
        {
          std::lock_guard<std::mutex> lk(state_mutex);
          *metrics = new_metrics;
        }

        // Broadcast STATE_SYNC with serialised metrics summary
        CollabEvent ev;
        ev.peer_id = client->LocalPeer().id;
        ev.peer_name = client->LocalPeer().name;
        ev.type = CollabEvent::Type::STATE_SYNC;
        // Simple CSV: name=value,name=value,...
        std::string payload;
        for (auto& m : new_metrics) {
          if (!payload.empty()) payload += ";";
          payload += m.name + "=" + std::to_string(m.value);
        }
        ev.payload = "METRICS:" + payload;
        ev.timestamp_ms = 0;
        client->SendEvent(ev);

        if (App* a = App::Active()) {
          a->Post([] {});
        }
      }
    });
  }

  // Subscribe to remote STATE_SYNC events
  client->OnRemoteEvent([&](CollabEvent ev) {
    if (ev.type == CollabEvent::Type::STATE_SYNC) {
      if (ev.payload.rfind("METRICS:", 0) == 0) {
        // Parse CSV metrics update
        std::string csv = ev.payload.substr(8);
        std::lock_guard<std::mutex> lk(state_mutex);
        size_t pos = 0;
        int idx = 0;
        while (pos < csv.size() && idx < static_cast<int>(metrics->size())) {
          auto semi = csv.find(';', pos);
          std::string entry =
              csv.substr(pos, semi == std::string::npos ? csv.size() - pos
                                                        : semi - pos);
          auto eq = entry.find('=');
          if (eq != std::string::npos) {
            try {
              (*metrics)[idx].value = std::stod(entry.substr(eq + 1));
            } catch (...) {
            }
          }
          pos = (semi == std::string::npos ? csv.size() : semi + 1);
          ++idx;
        }
      } else if (ev.payload.rfind("HIGHLIGHT:", 0) == 0) {
        std::lock_guard<std::mutex> lk(state_mutex);
        highlight_name = ev.payload.substr(10);
      }
    }
    if (App* a = App::Active()) {
      a->Post([] {});
    }
  });

  // UI component
  auto root = Renderer([&]() -> Element {
    std::vector<Metric> snap;
    std::string hl;
    int sel = selected.load();
    {
      std::lock_guard<std::mutex> lk(state_mutex);
      snap = *metrics;
      hl = highlight_name;
    }
    int viewers =
        server ? server->PeerCount() + 1 : (client->GetRemotePeers().size() + 1);
    return vbox({
        RenderDashboard(snap, sel, static_cast<int>(viewers), hl),
        CollabStatusBar(client) | border,
    });
  });

  auto app_comp = CatchEvent(root, [&](Event e) -> bool {
    int sz = static_cast<int>(metrics->size());
    if (e == Event::ArrowUp) {
      int s = (selected.load() - 1 + sz) % sz;
      selected.store(s);
      return true;
    }
    if (e == Event::ArrowDown) {
      int s = (selected.load() + 1) % sz;
      selected.store(s);
      return true;
    }
    if (e == Event::Return) {
      std::string metric_name;
      {
        std::lock_guard<std::mutex> lk(state_mutex);
        if (selected.load() < sz) {
          metric_name = (*metrics)[static_cast<size_t>(selected.load())].name;
          highlight_name = metric_name;
        }
      }
      // Broadcast highlight to all peers
      CollabEvent ev;
      ev.peer_id = client->LocalPeer().id;
      ev.peer_name = client->LocalPeer().name;
      ev.type = CollabEvent::Type::STATE_SYNC;
      ev.payload = "HIGHLIGHT:" + metric_name;
      ev.timestamp_ms = 0;
      client->SendEvent(ev);
      return true;
    }
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    return false;
  });

  RunFullscreen(app_comp);

  running.store(false);
  if (updater.joinable()) {
    updater.join();
  }
  client->Disconnect();
  if (server) {
    server->Stop();
  }
  return 0;
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
  if (argc < 2) {
    // Show usage
    auto root = Renderer([]() -> Element {
      return vbox({
                 text(" ★  FTXUI Shared Ops Dashboard  ★") | bold |
                     color(Color::CyanLight) | hcenter,
                 separator(),
                 text("  Start server:  ./collab_dashboard --server [PORT]"),
                 text("  Connect:       ./collab_dashboard --client HOST[:PORT] "
                      "--name NAME"),
                 separator(),
                 text(" q = quit") | color(Color::GrayDark),
             }) |
             border;
    });
    auto app_comp = CatchEvent(root, [](Event e) -> bool {
      if (e == Event::Character('q') || e == Event::Character('Q')) {
        if (App* a = App::Active()) {
          a->Exit();
        }
        return true;
      }
      return false;
    });
    Run(app_comp);
    return 0;
  }
  auto args = ParseArgs(argc, argv);
  return RunDashboard(args);
}
