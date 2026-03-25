// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file world_ops_demo.cpp
///
/// Global Operations Center — a live world-monitoring dashboard that combines:
///   • Interactive GeoJSON world map  (braille, pan/zoom/select)
///   • Region node table              (latency, packet-loss, status)
///   • Per-region latency sparklines  (inline trend indicators)
///   • Selected-region LineChart      (30-second latency history)
///   • Live event log                 (LogPanel, auto-scroll)
///   • Overall health sparklines in the status bar
///   • Background task refreshing all metrics every 750 ms
///   • Notification when a node degrades or recovers
///
/// Controls:
///   Map pane: ←→↑↓ pan   +/- zoom   R reset   Enter select feature
///   [T] cycle theme   [A] acknowledge all alerts   [Q] quit

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/background_task.hpp"
#include "ftxui/ui/charts.hpp"
#include "ftxui/ui/geojson.hpp"
#include "ftxui/ui/geomap.hpp"
#include "ftxui/ui/log_panel.hpp"
#include "ftxui/ui/notification.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/ui/world_map_data.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── City / Target data
// ──────────────────────────────────────────────────────── Derived from WOPR
// WorldMapView — normalized coords (x,y) where:
//   lon = x*360 - 180,  lat = 90 - y*180

struct CityTarget {
  double lon, lat;
  const char* name;
  int type;  // 0=US, 1=USSR, 2=Allied, 3=Base
};

static const CityTarget kCities[] = {
    // US
    {-127.8, 36.9, "LOS ANGELES", 0},
    {-122.4, 44.1, "SAN FRANCISCO", 0},
    {-115.2, 49.5, "SEATTLE", 0},
    {-99.0, 45.0, "CHICAGO", 0},
    {-84.6, 38.7, "WASHINGTON DC", 0},
    {-81.0, 42.3, "NEW YORK", 0},
    {-90.0, 36.0, "HOUSTON", 0},
    // USSR
    {34.2, 58.5, "MOSCOW", 1},
    {27.0, 63.0, "LENINGRAD", 1},
    {77.4, 57.6, "NOVOSIBIRSK", 1},
    {102.6, 55.8, "IRKUTSK", 1},
    {133.2, 54.9, "VLADIVOSTOK", 1},
    // Allied
    {0.0, 54.0, "LONDON", 2},
    {2.2, 51.3, "PARIS", 2},
    {13.3, 54.9, "BERLIN", 2},
    {116.3, 41.4, "BEIJING", 2},
    {139.7, 36.9, "TOKYO", 2},
    // Bases
    {-108.0, 43.2, "NORAD", 3},
    {-79.2, 64.8, "THULE AB", 3},
    {14.4, 69.3, "RAF FYLINGDALES", 3},
};
static constexpr int kCityCount = 20;

// Build a GeoCollection with city Points for map overlay.
// Types: 0=US (cyan), 1=USSR (red), 2=Allied (yellow), 3=Base (magenta)
static GeoCollection MakeCityCollection() {
  GeoCollection col;
  for (int i = 0; i < kCityCount; ++i) {
    const auto& c = kCities[i];
    GeoFeature feat;
    feat.geometry.type = "Point";
    feat.geometry.points.push_back({c.lon, c.lat});
    feat.properties["name"] = c.name;
    feat.properties["city_type"] = std::to_string(c.type);
    col.features.push_back(std::move(feat));
  }
  col.min_lon = -180;
  col.max_lon = 180;
  col.min_lat = -90;
  col.max_lat = 90;
  return col;
}

// ── Node / Region data model
// ──────────────────────────────────────────────────

enum class NodeStatus { Online, Degraded, Offline };

struct RegionStats {
  std::string name;
  std::string flag;
  float latency_ms = 0.0f;
  float packet_loss = 0.0f;
  int active_nodes = 0;
  int total_nodes = 0;
  NodeStatus status = NodeStatus::Online;
  std::vector<float> latency_history;  // last 30 samples
  std::vector<float> loss_history;     // last 30 samples
  float base_latency = 40.0f;
  float noise_amp = 8.0f;
};

static std::mutex g_mutex;
static std::vector<RegionStats> g_regions;
static int g_selected_region = 0;
static std::string g_selected_country;
static int g_tick = 0;
static std::vector<float> g_global_latency;
static std::vector<float> g_global_loss;

static void InitRegions() {
  g_regions = {
      {"Americas",
       "🌎",
       0,
       0,
       18,
       18,
       NodeStatus::Online,
       {},
       {},
       38.0f,
       10.0f},
      {"Europe", "🌍", 0, 0, 24, 24, NodeStatus::Online, {}, {}, 22.0f, 6.0f},
      {"Asia-Pacific",
       "🌏",
       0,
       0,
       31,
       32,
       NodeStatus::Online,
       {},
       {},
       65.0f,
       20.0f},
      {"EMEA", "🌐", 0, 0, 12, 12, NodeStatus::Online, {}, {}, 48.0f, 14.0f},
  };
  for (auto& r : g_regions) {
    r.latency_history.assign(30, r.base_latency);
    r.loss_history.assign(30, 0.5f);
  }
  g_global_latency.assign(30, 40.0f);
  g_global_loss.assign(30, 0.5f);
}

static std::string TimeStamp() {
  auto now = std::chrono::system_clock::now();
  auto tt = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &tt);
#else
  localtime_r(&tt, &tm);
#endif
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min,
                tm.tm_sec);
  return buf;
}

static std::string StatusIcon(NodeStatus s) {
  switch (s) {
    case NodeStatus::Online:
      return "● ";
    case NodeStatus::Degraded:
      return "⚠ ";
    case NodeStatus::Offline:
      return "✗ ";
  }
  return "? ";
}
static Color StatusColor(NodeStatus s) {
  switch (s) {
    case NodeStatus::Online:
      return Color::Green;
    case NodeStatus::Degraded:
      return Color::Yellow;
    case NodeStatus::Offline:
      return Color::Red;
  }
  return Color::Default;
}

// ── Shared log panel
// ──────────────────────────────────────────────────────────

static std::shared_ptr<LogPanel> g_log;

// ── Metrics updater
// ───────────────────────────────────────────────────────────

static void UpdateMetrics() {
  static std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
  std::uniform_real_distribution<float> spike(0.0f, 1.0f);
  std::uniform_int_distribution<int> pick(0, 3);

  std::lock_guard<std::mutex> lock(g_mutex);
  ++g_tick;

  float total_lat = 0.0f;
  float total_loss = 0.0f;

  for (auto& r : g_regions) {
    // Random walk latency
    float delta = noise(rng) * r.noise_amp;
    // Occasional spike
    if (spike(rng) < 0.04f) {
      delta += 60.0f + noise(rng) * 20.0f;
    }
    r.latency_ms = std::max(5.0f, r.base_latency + delta);
    r.packet_loss =
        std::max(0.0f, std::min(15.0f, r.packet_loss + noise(rng) * 0.5f +
                                           (spike(rng) < 0.03f ? 5.0f : 0.0f)));

    // Update history (rolling window)
    r.latency_history.erase(r.latency_history.begin());
    r.latency_history.push_back(r.latency_ms);
    r.loss_history.erase(r.loss_history.begin());
    r.loss_history.push_back(r.packet_loss);

    // Status transitions
    NodeStatus prev = r.status;
    if (r.latency_ms > 150.0f || r.packet_loss > 8.0f) {
      r.status = NodeStatus::Degraded;
      r.active_nodes = r.total_nodes - 1;
    } else if (r.latency_ms > 250.0f || r.packet_loss > 12.0f) {
      r.status = NodeStatus::Offline;
      r.active_nodes = r.total_nodes - pick(rng) - 2;
    } else {
      r.status = NodeStatus::Online;
      r.active_nodes = r.total_nodes;
    }

    if (prev != r.status && g_log) {
      std::string msg = r.name + ": ";
      if (r.status == NodeStatus::Degraded) {
        msg += "degraded  (lat " +
               std::to_string(static_cast<int>(r.latency_ms)) + "ms)";
      } else if (r.status == NodeStatus::Offline) {
        msg += "NODE DOWN";
      } else {
        msg += "recovered";
      }
      switch (r.status) {
        case NodeStatus::Online:
          g_log->Info(msg);
          break;
        case NodeStatus::Degraded:
          g_log->Warn(msg);
          break;
        case NodeStatus::Offline:
          g_log->Error(msg);
          break;
      }
    }

    total_lat += r.latency_ms;
    total_loss += r.packet_loss;
  }

  float avg_lat = total_lat / static_cast<float>(g_regions.size());
  float avg_loss = total_loss / static_cast<float>(g_regions.size());

  g_global_latency.erase(g_global_latency.begin());
  g_global_latency.push_back(avg_lat);
  g_global_loss.erase(g_global_loss.begin());
  g_global_loss.push_back(avg_loss);

  // Periodic info log every ~10 ticks
  if (g_tick % 10 == 0 && g_log) {
    int online = 0;
    for (const auto& r : g_regions) {
      if (r.status == NodeStatus::Online) {
        ++online;
      }
    }
    g_log->Info("Health check: " + std::to_string(online) + "/" +
                std::to_string(static_cast<int>(g_regions.size())) +
                " regions nominal  avg_lat=" +
                std::to_string(static_cast<int>(avg_lat)) + "ms");
  }
}

// ── Theme cycling
// ─────────────────────────────────────────────────────────────

static const std::vector<std::string> kThemeNames = {
    "Default", "Dark", "Light", "Nord", "Dracula", "Monokai"};
static int g_theme_idx = 3;  // start with Nord

static void ApplyTheme() {
  switch (g_theme_idx % static_cast<int>(kThemeNames.size())) {
    case 0:
      SetTheme(Theme::Default());
      break;
    case 1:
      SetTheme(Theme::Dark());
      break;
    case 2:
      SetTheme(Theme::Light());
      break;
    case 3:
      SetTheme(Theme::Nord());
      break;
    case 4:
      SetTheme(Theme::Dracula());
      break;
    case 5:
      SetTheme(Theme::Monokai());
      break;
  }
}

// ── Region detail panel
// ───────────────────────────────────────────────────────

Component MakeDetailPanel() {
  return Renderer([] {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_selected_region < 0 ||
        g_selected_region >= static_cast<int>(g_regions.size())) {
      return text("No region selected") | dim | center;
    }
    const Theme& t = GetTheme();
    const auto& r = g_regions[static_cast<size_t>(g_selected_region)];

    // Header
    Elements rows;
    rows.push_back(
        hbox({text(" " + r.flag + " " + r.name + " ") | bold | color(t.primary),
              filler(),
              text(StatusIcon(r.status)) | color(StatusColor(r.status)),
              text(r.status == NodeStatus::Online     ? "Online"
                   : r.status == NodeStatus::Degraded ? "Degraded"
                                                      : "Offline") |
                  color(StatusColor(r.status))}) |
        color(t.primary));
    rows.push_back(separator());

    // Key metrics
    rows.push_back(hbox({
        text("  Latency:  ") | dim,
        text(std::to_string(static_cast<int>(r.latency_ms)) + " ms") |
            color(r.latency_ms > 150 ? t.error_color : t.success_color),
        filler(),
        text("  Loss: ") | dim,
        text([&]() -> std::string {
          std::ostringstream ss;
          ss.precision(1);
          ss << std::fixed << r.packet_loss << "%";
          return ss.str();
        }()) |
            color(r.packet_loss > 5.0f ? t.error_color : t.success_color),
    }));
    rows.push_back(hbox({
        text("  Nodes:    ") | dim,
        text(std::to_string(r.active_nodes) + "/" +
             std::to_string(r.total_nodes)) |
            color(r.active_nodes < r.total_nodes ? t.warning_color
                                                 : t.success_color),
        filler(),
    }));
    rows.push_back(separator());

    // Sparklines
    rows.push_back(hbox({text("  Latency  ") | dim,
                         Sparkline(r.latency_history, 20, t.primary)}));
    rows.push_back(hbox({text("  Pkt Loss ") | dim,
                         Sparkline(r.loss_history, 20, t.warning_color)}));

    return vbox(std::move(rows));
  });
}

// ── Region table
// ──────────────────────────────────────────────────────────────

Component MakeRegionTable() {
  static int focused_row = 0;

  auto table_renderer = Renderer([&] {
    std::lock_guard<std::mutex> lock(g_mutex);
    const Theme& t = GetTheme();

    Elements header_cells = {
        text(" Region      ") | bold | color(t.primary),
        text(" Lat(ms) ") | bold | color(t.primary),
        text(" Loss%  ") | bold | color(t.primary),
        text(" Nodes  ") | bold | color(t.primary),
        text(" Status ") | bold | color(t.primary),
        text(" Trend          ") | bold | color(t.primary),
    };

    Elements rows_el;
    rows_el.push_back(hbox(header_cells));
    rows_el.push_back(separator());

    for (int i = 0; i < static_cast<int>(g_regions.size()); ++i) {
      const auto& r = g_regions[static_cast<size_t>(i)];
      bool selected = (i == g_selected_region);

      std::string lat_str = std::to_string(static_cast<int>(r.latency_ms));
      std::ostringstream loss_ss;
      loss_ss.precision(1);
      loss_ss << std::fixed << r.packet_loss;

      Element row = hbox({
          text(" " + r.flag + " " + r.name + "  ") |
              ftxui::size(WIDTH, EQUAL, 14),
          text(" " + lat_str + "  ") | ftxui::size(WIDTH, EQUAL, 9),
          text(" " + loss_ss.str() + "  ") | ftxui::size(WIDTH, EQUAL, 8),
          text(" " + std::to_string(r.active_nodes) + "/" +
               std::to_string(r.total_nodes) + "  ") |
              ftxui::size(WIDTH, EQUAL, 8),
          hbox({text(" " + StatusIcon(r.status)) | color(StatusColor(r.status)),
                text(r.status == NodeStatus::Online     ? "OK      "
                     : r.status == NodeStatus::Degraded ? "Warn    "
                                                        : "Down    ") |
                    color(StatusColor(r.status))}) |
              ftxui::size(WIDTH, EQUAL, 9),
          hbox({text(" "),
                Sparkline(r.latency_history, 16,
                          r.status == NodeStatus::Online ? t.primary
                                                         : t.warning_color)}),
      });

      if (selected) {
        row = row | inverted;
      }
      rows_el.push_back(row);
    }
    return vbox(std::move(rows_el));
  });

  // Wrap to capture keyboard ↑↓ for region selection
  return CatchEvent(table_renderer, [](Event e) {
    if (e == Event::ArrowDown) {
      std::lock_guard<std::mutex> lock(g_mutex);
      g_selected_region = std::min(g_selected_region + 1,
                                   static_cast<int>(g_regions.size()) - 1);
      return true;
    }
    if (e == Event::ArrowUp) {
      std::lock_guard<std::mutex> lock(g_mutex);
      g_selected_region = std::max(g_selected_region - 1, 0);
      return true;
    }
    return false;
  });
}

// ── Status bar
// ────────────────────────────────────────────────────────────────

Element MakeStatusBar() {
  std::lock_guard<std::mutex> lock(g_mutex);
  const Theme& t = GetTheme();

  int online = 0, degraded = 0, offline_cnt = 0;
  for (const auto& r : g_regions) {
    switch (r.status) {
      case NodeStatus::Online:
        ++online;
        break;
      case NodeStatus::Degraded:
        ++degraded;
        break;
      case NodeStatus::Offline:
        ++offline_cnt;
        break;
    }
  }

  int total_nodes = 0, active_nodes = 0;
  for (const auto& r : g_regions) {
    total_nodes += r.total_nodes;
    active_nodes += r.active_nodes;
  }

  // Theme name display
  std::string theme_name = kThemeNames[static_cast<size_t>(
      g_theme_idx % static_cast<int>(kThemeNames.size()))];

  return hbox({
      text(" ● ") | color(Color::Green),
      text(std::to_string(online) + " nominal  ") | color(t.success_color),
      text("⚠ ") | color(Color::Yellow),
      text(std::to_string(degraded) + " degraded  ") | color(t.warning_color),
      text("✗ ") | color(Color::Red),
      text(std::to_string(offline_cnt) + " offline  ") | color(t.error_color),
      text("│") | dim,
      text("  Nodes: " + std::to_string(active_nodes) + "/" +
           std::to_string(total_nodes) + "  ") |
          dim,
      text("│") | dim,
      text("  Lat "),
      Sparkline(g_global_latency, 12, t.primary),
      text("  Loss "),
      Sparkline(g_global_loss, 12, t.warning_color),
      text("  ") | flex,
      text("Theme: " + theme_name + "  [T] cycle  [Q] quit") | dim,
  });
}

// ── Main
// ──────────────────────────────────────────────────────────────────────

int main() {
  InitRegions();
  ApplyTheme();

  g_log = LogPanel::Create(200);
  g_log->Info("Global Operations Center started");
  g_log->Info("Monitoring " + std::to_string(g_regions.size()) + " regions");
  g_log->Debug("Background metrics refresh: 750ms");

  // ── Load GeoJSON (embedded — always available, no file dependency)
  // ──────────
  GeoMap map_builder;
  map_builder.Data(WorldMapGeoJSON());
  g_log->Info("Map: Natural Earth 110m (embedded, 461 features)");

  // ── City overlay: US=cyan, USSR=red, Allied=yellow, Bases=magenta ────────
  auto cities_col = MakeCityCollection();
  map_builder.Overlay(cities_col, Color::Cyan);

  // ── Missile arc trajectories (great-circle, DEFCON-style) ────────────────
  // Washington DC → Moscow
  map_builder.AddArc(-84.6, 38.7, 34.2, 58.5, Color::Red, 48);
  // NORAD → Leningrad
  map_builder.AddArc(-108.0, 43.2, 27.0, 63.0, Color{220, 80, 80}, 48);
  // Vladivostok → Seattle
  map_builder.AddArc(133.2, 54.9, -115.2, 49.5, Color{220, 80, 80}, 48);
  // London → Moscow (retaliation arc)
  map_builder.AddArc(0.0, 54.0, 34.2, 58.5, Color{255, 140, 0}, 32);
  // Beijing → Tokyo (regional)
  map_builder.AddArc(116.3, 41.4, 139.7, 36.9, Color{200, 100, 50}, 24);

  g_log->Warn("DEFCON 3 — 5 missile trajectories plotted");
  g_log->Debug("Great-circle arcs: DC→Moscow, NORAD→Leningrad, Vlad→Seattle");

  // ── GeoMap ────────────────────────────────────────────────────────────────
  auto map =
      map_builder.LineColor(Color::GrayLight)
          .ShowGraticule(true)
          .GraticuleStep(30.0f)
          .OnSelect([](const GeoFeature& f) {
            auto it = f.properties.find("region");
            std::string region_name =
                (it != f.properties.end()) ? it->second : "";
            auto name_it = f.properties.find("name");
            std::string country =
                (name_it != f.properties.end()) ? name_it->second : "?";

            std::lock_guard<std::mutex> lock(g_mutex);
            g_selected_country = country;

            for (int i = 0; i < static_cast<int>(g_regions.size()); ++i) {
              if (g_regions[static_cast<size_t>(i)].name == region_name) {
                g_selected_region = i;
                break;
              }
            }
            if (g_log) {
              g_log->Info("Selected: " + country);
            }
          })
          .Build();

  // ── Right panel (table + detail + log) ───────────────────────────────────
  auto region_table = MakeRegionTable();
  auto detail_panel = MakeDetailPanel();
  auto log_component = g_log->Build("Event Log");

  // Wrap table + detail in a vertical split
  auto right_top = Container::Vertical({region_table, detail_panel});

  // right_top uses fixed rows; log gets remaining space
  auto right_panel = Container::Vertical({right_top, log_component});
  auto right_renderer = Renderer(right_panel, [&] {
    const Theme& t = GetTheme();
    return vbox({
        // Region table in a bordered box
        vbox({region_table->Render()}) | border |
            ftxui::size(HEIGHT, EQUAL, 8) | color(t.border_color),

        // Detail for selected region
        vbox({detail_panel->Render()}) | border |
            ftxui::size(HEIGHT, EQUAL, 10) | color(t.border_color),

        // Log fills remainder
        log_component->Render() | border | flex | color(t.border_color),
    });
  });

  // ── Split layout: map (left, 60%) | right panel (40%) ────────────────────
  auto split = Container::Horizontal({map, right_renderer});
  auto layout = Renderer(split, [&] {
    const Theme& t = GetTheme();
    return hbox({
        // Left: world map
        vbox({
            text(" ★  Global Operations Center") | bold | color(t.primary) |
                hcenter,
            separator() | color(t.border_color),
            map->Render() | flex,
            separator() | color(t.border_color),
            // Mini hint
            hbox({text("  ← → ↑ ↓  pan   +/-  zoom   R  reset   "
                       "Enter  select   Tab  switch pane") |
                  dim | flex}),
        }) | flex_grow |
            border | color(t.border_color),

        // Right: table + detail + log
        right_renderer->Render() | ftxui::size(WIDTH, EQUAL, 58),
    });
  });

  // ── Status bar wrapper ────────────────────────────────────────────────────
  auto root = Renderer(layout, [&] {
    const Theme& t = GetTheme();
    return vbox({
        layout->Render() | flex,
        separator() | color(t.border_color),
        MakeStatusBar(),
    });
  });

  // ── Global key handler ────────────────────────────────────────────────────
  auto app_component = CatchEvent(root, [&](Event e) {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      if (App* a = App::Active()) {
        a->Exit();
      }
      return true;
    }
    if (e == Event::Character('t') || e == Event::Character('T')) {
      ++g_theme_idx;
      ApplyTheme();
      std::lock_guard<std::mutex> lock(g_mutex);
      g_log->Info("Theme changed to " +
                  kThemeNames[static_cast<size_t>(
                      g_theme_idx % static_cast<int>(kThemeNames.size()))]);
      return true;
    }
    return false;
  });

  // ── Background metrics refresh ────────────────────────────────────────────
  std::atomic<bool> running{true};
  std::thread bg_thread([&running] {
    while (running.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(750));
      if (!running.load()) {
        break;
      }
      UpdateMetrics();
      if (App* a = App::Active()) {
        a->Post([] {});
      }
    }
  });

  // ── Run ───────────────────────────────────────────────────────────────────
  RunFullscreen(app_component);

  running.store(false);
  bg_thread.join();
  return 0;
}
