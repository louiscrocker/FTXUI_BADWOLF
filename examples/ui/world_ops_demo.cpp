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

using namespace ftxui;
using namespace ftxui::ui;

// ── World GeoJSON ─────────────────────────────────────────────────────────────

static const char kWorldGeoJSON[] = R"GEO({
"type":"FeatureCollection",
"features":[
  {"type":"Feature","properties":{"name":"USA","region":"Americas"},"geometry":{"type":"Polygon","coordinates":[[
    [-125,49],[-95,49],[-75,45],[-67,47],[-70,43],[-77,35],[-81,25],[-97,26],
    [-105,22],[-117,32],[-124,36],[-125,49]]]}},
  {"type":"Feature","properties":{"name":"Canada","region":"Americas"},"geometry":{"type":"Polygon","coordinates":[[
    [-141,60],[-125,49],[-95,49],[-75,45],[-67,47],[-60,47],[-52,47],
    [-55,60],[-79,63],[-95,63],[-120,65],[-141,60]]]}},
  {"type":"Feature","properties":{"name":"Mexico","region":"Americas"},"geometry":{"type":"Polygon","coordinates":[[
    [-117,32],[-97,26],[-94,18],[-90,18],[-88,16],[-83,10],[-85,10],
    [-91,15],[-105,22],[-117,32]]]}},
  {"type":"Feature","properties":{"name":"Brazil","region":"Americas"},"geometry":{"type":"Polygon","coordinates":[[
    [-73,-4],[-60,5],[-50,5],[-35,-5],[-35,-22],[-50,-33],[-55,-34],
    [-65,-55],[-70,-55],[-75,-45],[-80,-2],[-73,-4]]]}},
  {"type":"Feature","properties":{"name":"Argentina","region":"Americas"},"geometry":{"type":"Polygon","coordinates":[[
    [-73,-40],[-55,-35],[-53,-34],[-65,-55],[-70,-55],[-75,-50],[-73,-40]]]}},
  {"type":"Feature","properties":{"name":"UK","region":"Europe"},"geometry":{"type":"Polygon","coordinates":[[
    [-5,50],[1,51],[1,53],[-3,58],[-5,55],[-6,53],[-5,50]]]}},
  {"type":"Feature","properties":{"name":"France","region":"Europe"},"geometry":{"type":"Polygon","coordinates":[[
    [-4,43],[8,43],[8,47],[6,50],[2,51],[-2,49],[-4,48],[-4,43]]]}},
  {"type":"Feature","properties":{"name":"Germany","region":"Europe"},"geometry":{"type":"Polygon","coordinates":[[
    [6,47],[15,47],[15,55],[14,54],[9,55],[6,54],[5,50],[6,47]]]}},
  {"type":"Feature","properties":{"name":"Spain","region":"Europe"},"geometry":{"type":"Polygon","coordinates":[[
    [-9,36],[3,36],[3,44],[-2,44],[-9,44],[-9,36]]]}},
  {"type":"Feature","properties":{"name":"Italy","region":"Europe"},"geometry":{"type":"Polygon","coordinates":[[
    [7,44],[18,40],[16,38],[15,38],[12,38],[9,40],[7,44]]]}},
  {"type":"Feature","properties":{"name":"Russia","region":"Europe"},"geometry":{"type":"Polygon","coordinates":[[
    [28,55],[40,50],[60,52],[90,65],[140,65],[145,55],[130,42],
    [90,45],[60,42],[38,42],[28,45],[28,55]]]}},
  {"type":"Feature","properties":{"name":"China","region":"Asia-Pacific"},"geometry":{"type":"Polygon","coordinates":[[
    [73,20],[90,20],[100,8],[115,8],[120,20],[135,35],[135,48],
    [120,52],[90,52],[75,40],[73,38],[73,20]]]}},
  {"type":"Feature","properties":{"name":"India","region":"Asia-Pacific"},"geometry":{"type":"Polygon","coordinates":[[
    [68,8],[80,8],[88,20],[92,27],[80,35],[72,28],[68,22],[68,8]]]}},
  {"type":"Feature","properties":{"name":"Japan","region":"Asia-Pacific"},"geometry":{"type":"Polygon","coordinates":[[
    [130,31],[131,33],[133,34],[136,36],[141,41],[145,44],[141,45],
    [135,34],[130,31]]]}},
  {"type":"Feature","properties":{"name":"Australia","region":"Asia-Pacific"},"geometry":{"type":"Polygon","coordinates":[[
    [114,-22],[114,-35],[120,-38],[148,-38],[154,-28],[150,-18],
    [138,-12],[120,-14],[114,-22]]]}},
  {"type":"Feature","properties":{"name":"South Korea","region":"Asia-Pacific"},"geometry":{"type":"Polygon","coordinates":[[
    [126,34],[129,34],[129,38],[126,38],[126,34]]]}},
  {"type":"Feature","properties":{"name":"Egypt","region":"EMEA"},"geometry":{"type":"Polygon","coordinates":[[
    [25,22],[35,22],[35,31],[33,31],[32,29],[25,29],[25,22]]]}},
  {"type":"Feature","properties":{"name":"Nigeria","region":"EMEA"},"geometry":{"type":"Polygon","coordinates":[[
    [3,4],[14,4],[14,14],[3,14],[3,4]]]}},
  {"type":"Feature","properties":{"name":"South Africa","region":"EMEA"},"geometry":{"type":"Polygon","coordinates":[[
    [17,-35],[30,-35],[33,-28],[38,-20],[26,-18],[18,-18],[17,-25],[17,-35]]]}},
  {"type":"Feature","properties":{"name":"Saudi Arabia","region":"EMEA"},"geometry":{"type":"Polygon","coordinates":[[
    [37,28],[37,16],[45,12],[55,22],[58,28],[55,32],[45,32],[37,28]]]}}
]})GEO";

// ── Node / Region data model ──────────────────────────────────────────────────

enum class NodeStatus { Online, Degraded, Offline };

struct RegionStats {
  std::string name;
  std::string flag;
  float latency_ms   = 0.0f;
  float packet_loss  = 0.0f;
  int   active_nodes = 0;
  int   total_nodes  = 0;
  NodeStatus status  = NodeStatus::Online;
  std::vector<float> latency_history;   // last 30 samples
  std::vector<float> loss_history;      // last 30 samples
  float base_latency = 40.0f;
  float noise_amp    = 8.0f;
};

static std::mutex              g_mutex;
static std::vector<RegionStats> g_regions;
static int                     g_selected_region = 0;
static std::string             g_selected_country;
static int                     g_tick = 0;
static std::vector<float>      g_global_latency;
static std::vector<float>      g_global_loss;

static void InitRegions() {
  g_regions = {
    {"Americas",    "🌎",  0, 0, 18, 18, NodeStatus::Online, {}, {}, 38.0f, 10.0f},
    {"Europe",      "🌍",  0, 0, 24, 24, NodeStatus::Online, {}, {}, 22.0f,  6.0f},
    {"Asia-Pacific","🌏",  0, 0, 31, 32, NodeStatus::Online, {}, {}, 65.0f, 20.0f},
    {"EMEA",        "🌐",  0, 0, 12, 12, NodeStatus::Online, {}, {}, 48.0f, 14.0f},
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
  auto tt  = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &tt);
#else
  localtime_r(&tt, &tm);
#endif
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                tm.tm_hour, tm.tm_min, tm.tm_sec);
  return buf;
}

static std::string StatusIcon(NodeStatus s) {
  switch (s) {
    case NodeStatus::Online:   return "● ";
    case NodeStatus::Degraded: return "⚠ ";
    case NodeStatus::Offline:  return "✗ ";
  }
  return "? ";
}
static Color StatusColor(NodeStatus s) {
  switch (s) {
    case NodeStatus::Online:   return Color::Green;
    case NodeStatus::Degraded: return Color::Yellow;
    case NodeStatus::Offline:  return Color::Red;
  }
  return Color::Default;
}

// ── Shared log panel ──────────────────────────────────────────────────────────

static std::shared_ptr<LogPanel> g_log;

// ── Metrics updater ───────────────────────────────────────────────────────────

static void UpdateMetrics() {
  static std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
  std::uniform_real_distribution<float> spike(0.0f, 1.0f);
  std::uniform_int_distribution<int>    pick(0, 3);

  std::lock_guard<std::mutex> lock(g_mutex);
  ++g_tick;

  float total_lat = 0.0f;
  float total_loss = 0.0f;

  for (auto& r : g_regions) {
    // Random walk latency
    float delta = noise(rng) * r.noise_amp;
    // Occasional spike
    if (spike(rng) < 0.04f) delta += 60.0f + noise(rng) * 20.0f;
    r.latency_ms  = std::max(5.0f, r.base_latency + delta);
    r.packet_loss = std::max(0.0f, std::min(15.0f,
        r.packet_loss + noise(rng) * 0.5f +
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
      if (r.status == NodeStatus::Degraded)
        msg += "degraded  (lat " + std::to_string(static_cast<int>(r.latency_ms)) + "ms)";
      else if (r.status == NodeStatus::Offline)
        msg += "NODE DOWN";
      else
        msg += "recovered";
      switch (r.status) {
        case NodeStatus::Online:   g_log->Info(msg); break;
        case NodeStatus::Degraded: g_log->Warn(msg); break;
        case NodeStatus::Offline:  g_log->Error(msg); break;
      }
    }

    total_lat  += r.latency_ms;
    total_loss += r.packet_loss;
  }

  float avg_lat  = total_lat  / static_cast<float>(g_regions.size());
  float avg_loss = total_loss / static_cast<float>(g_regions.size());

  g_global_latency.erase(g_global_latency.begin());
  g_global_latency.push_back(avg_lat);
  g_global_loss.erase(g_global_loss.begin());
  g_global_loss.push_back(avg_loss);

  // Periodic info log every ~10 ticks
  if (g_tick % 10 == 0 && g_log) {
    int online = 0;
    for (const auto& r : g_regions)
      if (r.status == NodeStatus::Online) ++online;
    g_log->Info("Health check: " + std::to_string(online) + "/" +
                std::to_string(static_cast<int>(g_regions.size())) +
                " regions nominal  avg_lat=" +
                std::to_string(static_cast<int>(avg_lat)) + "ms");
  }
}

// ── Theme cycling ─────────────────────────────────────────────────────────────

static const std::vector<std::string> kThemeNames =
    {"Default", "Dark", "Light", "Nord", "Dracula", "Monokai"};
static int g_theme_idx = 3;  // start with Nord

static void ApplyTheme() {
  switch (g_theme_idx % static_cast<int>(kThemeNames.size())) {
    case 0: SetTheme(Theme::Default()); break;
    case 1: SetTheme(Theme::Dark());    break;
    case 2: SetTheme(Theme::Light());   break;
    case 3: SetTheme(Theme::Nord());    break;
    case 4: SetTheme(Theme::Dracula()); break;
    case 5: SetTheme(Theme::Monokai()); break;
  }
}

// ── Region detail panel ───────────────────────────────────────────────────────

Component MakeDetailPanel() {
  return Renderer([] {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_selected_region < 0 ||
        g_selected_region >= static_cast<int>(g_regions.size())) {
      return text("No region selected") | dim | center;
    }
    const Theme& t = GetTheme();
    const auto& r  = g_regions[static_cast<size_t>(g_selected_region)];

    // Header
    Elements rows;
    rows.push_back(
        hbox({text(" " + r.flag + " " + r.name + " ") | bold | color(t.primary),
              filler(),
              text(StatusIcon(r.status)) | color(StatusColor(r.status)),
              text(r.status == NodeStatus::Online   ? "Online"
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

// ── Region table ──────────────────────────────────────────────────────────────

Component MakeRegionTable() {
  static int focused_row = 0;

  auto table_renderer = Renderer([&] {
    std::lock_guard<std::mutex> lock(g_mutex);
    const Theme& t = GetTheme();

    Elements header_cells = {
        text(" Region      ") | bold | color(t.primary),
        text(" Lat(ms) ")     | bold | color(t.primary),
        text(" Loss%  ")      | bold | color(t.primary),
        text(" Nodes  ")      | bold | color(t.primary),
        text(" Status ")      | bold | color(t.primary),
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
          text(" " + r.flag + " " + r.name + "  ") | ftxui::size(WIDTH, EQUAL, 14),
          text(" " + lat_str + "  ")                | ftxui::size(WIDTH, EQUAL, 9),
          text(" " + loss_ss.str() + "  ")          | ftxui::size(WIDTH, EQUAL, 8),
          text(" " + std::to_string(r.active_nodes) + "/" +
               std::to_string(r.total_nodes) + "  ")| ftxui::size(WIDTH, EQUAL, 8),
          hbox({text(" " + StatusIcon(r.status))    | color(StatusColor(r.status)),
                text(r.status == NodeStatus::Online   ? "OK      "
                     : r.status == NodeStatus::Degraded ? "Warn    "
                                                        : "Down    ") |
                    color(StatusColor(r.status))})  | ftxui::size(WIDTH, EQUAL, 9),
          hbox({text(" "), Sparkline(r.latency_history, 16,
                                     r.status == NodeStatus::Online
                                         ? t.primary
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

// ── Status bar ────────────────────────────────────────────────────────────────

Element MakeStatusBar() {
  std::lock_guard<std::mutex> lock(g_mutex);
  const Theme& t = GetTheme();

  int online = 0, degraded = 0, offline_cnt = 0;
  for (const auto& r : g_regions) {
    switch (r.status) {
      case NodeStatus::Online:   ++online;      break;
      case NodeStatus::Degraded: ++degraded;    break;
      case NodeStatus::Offline:  ++offline_cnt; break;
    }
  }

  int total_nodes = 0, active_nodes = 0;
  for (const auto& r : g_regions) {
    total_nodes  += r.total_nodes;
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
           std::to_string(total_nodes) + "  ") | dim,
      text("│") | dim,
      text("  Lat "),
      Sparkline(g_global_latency, 12, t.primary),
      text("  Loss "),
      Sparkline(g_global_loss, 12, t.warning_color),
      text("  ") | flex,
      text("Theme: " + theme_name + "  [T] cycle  [Q] quit") | dim,
  });
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  InitRegions();
  ApplyTheme();

  g_log = LogPanel::Create(200);
  g_log->Info("Global Operations Center started");
  g_log->Info("Monitoring " + std::to_string(g_regions.size()) + " regions");
  g_log->Debug("Background metrics refresh: 750ms");

  // ── GeoMap ────────────────────────────────────────────────────────────────
  auto map = GeoMap()
                 .Data(kWorldGeoJSON)
                 .LineColor(Color::GrayLight)
                 .ShowGraticule(true)
                 .GraticuleStep(30.0f)
                 .OnSelect([](const GeoFeature& f) {
                   // Find which region this country belongs to
                   auto it = f.properties.find("region");
                   std::string region_name =
                       (it != f.properties.end()) ? it->second : "";
                   auto name_it = f.properties.find("name");
                   std::string country =
                       (name_it != f.properties.end()) ? name_it->second : "?";

                   std::lock_guard<std::mutex> lock(g_mutex);
                   g_selected_country = country;

                   // Select matching region in table
                   for (int i = 0; i < static_cast<int>(g_regions.size()); ++i) {
                     if (g_regions[static_cast<size_t>(i)].name == region_name) {
                       g_selected_region = i;
                       break;
                     }
                   }
                   if (g_log) {
                     g_log->Info("Selected: " + country + " (" + region_name + ")");
                   }
                 })
                 .Build();

  // ── Right panel (table + detail + log) ───────────────────────────────────
  auto region_table  = MakeRegionTable();
  auto detail_panel  = MakeDetailPanel();
  auto log_component = g_log->Build("Event Log");

  // Wrap table + detail in a vertical split
  auto right_top = Container::Vertical({region_table, detail_panel});

  // right_top uses fixed rows; log gets remaining space
  auto right_panel = Container::Vertical({right_top, log_component});
  auto right_renderer = Renderer(right_panel, [&] {
    const Theme& t = GetTheme();
    return vbox({
               // Region table in a bordered box
               vbox({region_table->Render()}) |
                   border | ftxui::size(HEIGHT, EQUAL, 8) | color(t.border_color),

               // Detail for selected region
               vbox({detail_panel->Render()}) |
                   border | ftxui::size(HEIGHT, EQUAL, 10) | color(t.border_color),

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
            text(" ★  Global Operations Center") | bold | color(t.primary) | hcenter,
            separator() | color(t.border_color),
            map->Render() | flex,
            separator() | color(t.border_color),
            // Mini hint
            hbox({text("  ← → ↑ ↓  pan   +/-  zoom   R  reset   "
                       "Enter  select   Tab  switch pane") |
                  dim | flex}) ,
        }) | flex_grow | border | color(t.border_color),

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
      if (App* a = App::Active()) a->Exit();
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
      if (!running.load()) break;
      UpdateMetrics();
      if (App* a = App::Active()) a->Post([] {});
    }
  });

  // ── Run ───────────────────────────────────────────────────────────────────
  RunFullscreen(app_component);

  running.store(false);
  bg_thread.join();
  return 0;
}
