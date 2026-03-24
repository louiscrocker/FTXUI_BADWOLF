// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file virtual_table_demo.cpp
/// Demonstrates VirtualList<T> with 50 000 log lines and
/// SortableTable<T> with 200 fake server-metrics rows.
///
/// Controls:
///   Tab          Switch between VirtualList and SortableTable panes
///   ↑ / ↓        Navigate rows
///   PgUp / PgDn  Scroll by page (VirtualList) or change page (SortableTable)
///   Home / End   Jump to first / last item (VirtualList)
///   ← / →        Cycle sort column focus (SortableTable)
///   Enter        Confirm sort column (SortableTable)
///   Escape       Clear filter in either pane
///   q            Quit

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui.hpp"
#include "ftxui/ui/sortable_table.hpp"
#include "ftxui/ui/virtual_list.hpp"

using namespace ftxui;
using namespace ftxui::ui;

// ── Data generation ──────────────────────────────────────────────────────────

namespace {

constexpr int kLogLines = 50'000;
constexpr int kServerRows = 200;

const std::array<std::string, 5> kLogLevels = {
    "INFO ", "WARN ", "ERROR", "DEBUG", "TRACE"};

const std::array<std::string, 8> kServices = {
    "auth-svc", "api-gw", "db-proxy", "cache", "scheduler",
    "mailer",   "worker", "metrics"};

const std::array<std::string, 10> kMessages = {
    "Request processed successfully",
    "Connection pool exhausted, retrying",
    "Failed to parse incoming payload",
    "Cache miss for key user:session",
    "Scheduled job completed in 42 ms",
    "TLS handshake timeout after 5 s",
    "Rate limit exceeded for IP 10.0.0.1",
    "Replica lag detected: 1200 ms",
    "Health check passed",
    "Graceful shutdown initiated"};

std::vector<std::string> GenerateLogs() {
  std::vector<std::string> lines;
  lines.reserve(kLogLines);
  for (int i = 0; i < kLogLines; ++i) {
    const std::string& lvl = kLogLevels[static_cast<size_t>(i) % kLogLevels.size()];
    const std::string& svc = kServices[static_cast<size_t>(i) % kServices.size()];
    const std::string& msg = kMessages[static_cast<size_t>(i) % kMessages.size()];
    // Pseudo-timestamp based on index.
    const int hh = (i / 3600) % 24;
    const int mm = (i / 60) % 60;
    const int ss = i % 60;
    char ts[16];
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::snprintf(ts, sizeof(ts), "%02d:%02d:%02d", hh, mm, ss);
    lines.push_back(std::string(ts) + " [" + lvl + "] " + svc + " — " + msg +
                    " (#" + std::to_string(i + 1) + ")");
  }
  return lines;
}

struct Server {
  std::string name;
  float cpu;   // 0-100 %
  int mem_mb;  // MiB
  std::string region;
  std::string status;
};

std::shared_ptr<std::vector<Server>> GenerateServers() {
  const std::array<std::string, 4> regions = {"us-east", "eu-west", "ap-south",
                                               "us-west"};
  const std::array<std::string, 3> statuses = {"healthy", "degraded", "down"};

  auto servers = std::make_shared<std::vector<Server>>();
  servers->reserve(kServerRows);
  for (int i = 0; i < kServerRows; ++i) {
    // Deterministic pseudo-random values.
    const float cpu = static_cast<float>((i * 37 + 13) % 101);
    const int mem = 512 + (i * 53 + 7) % 15872;  // 512 MiB – 16 GiB
    servers->push_back({
        "srv-" + std::to_string(1000 + i),
        cpu,
        mem,
        regions[static_cast<size_t>(i) % regions.size()],
        statuses[static_cast<size_t>(i) % statuses.size()],
    });
  }
  return servers;
}

}  // namespace

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
  SetTheme(Theme::Nord());
  const Theme& theme = GetTheme();

  // ── VirtualList pane ───────────────────────────────────────────────────────
  auto log_data = std::make_shared<std::vector<std::string>>(GenerateLogs());
  std::string list_status;

  auto log_list =
      VirtualList<std::string>()
          .Items(log_data)
          .RowHeight(1)
          .Render([](const std::string& line, bool sel) -> Element {
            // Colorise by log level.
            Color lvl_color = Color::Default;
            if (line.find("[WARN ]") != std::string::npos) {
              lvl_color = Color::Yellow;
            } else if (line.find("[ERROR]") != std::string::npos) {
              lvl_color = Color::Red;
            } else if (line.find("[DEBUG]") != std::string::npos) {
              lvl_color = Color::Blue;
            } else if (line.find("[TRACE]") != std::string::npos) {
              lvl_color = Color::GrayLight;
            }
            Element e = text(" " + line);
            if (lvl_color != Color::Default) {
              e = e | color(lvl_color);
            }
            if (sel) {
              e = e | inverted | bold;
            }
            return e;
          })
          .Filter([](const std::string& line, const std::string& q) -> bool {
            // Case-insensitive substring search.
            auto it = std::search(
                line.begin(), line.end(), q.begin(), q.end(),
                [](unsigned char a, unsigned char b) {
                  return std::tolower(a) == std::tolower(b);
                });
            return it != line.end();
          })
          .OnSelect([&](size_t idx, const std::string& item) {
            list_status = "Selected #" + std::to_string(idx + 1) + ": " +
                          item.substr(0, 60);
          })
          .ShowSearch(true)
          .Build();

  // ── SortableTable pane ─────────────────────────────────────────────────────
  auto servers = GenerateServers();
  std::string table_status;

  using Col = SortableTable<Server>::Column;
  auto server_table =
      SortableTable<Server>()
          .Columns({
              Col{"Name", [](const Server& s) { return s.name; },
                  [](const Server& a, const Server& b) {
                    return a.name < b.name;
                  },
                  12, true},
              Col{"CPU %",
                  [](const Server& s) {
                    char buf[8];
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
                    std::snprintf(buf, sizeof(buf), "%.1f", s.cpu);
                    return std::string(buf);
                  },
                  [](const Server& a, const Server& b) {
                    return a.cpu < b.cpu;
                  },
                  8, true},
              Col{"Mem (MiB)", [](const Server& s) { return std::to_string(s.mem_mb); },
                  [](const Server& a, const Server& b) {
                    return a.mem_mb < b.mem_mb;
                  },
                  10, true},
              Col{"Region", [](const Server& s) { return s.region; },
                  [](const Server& a, const Server& b) {
                    return a.region < b.region;
                  },
                  10, true},
              Col{"Status", [](const Server& s) { return s.status; },
                  [](const Server& a, const Server& b) {
                    return a.status < b.status;
                  },
                  10, true},
          })
          .Rows(servers)
          .PageSize(20)
          .ShowFilter(true)
          .OnSelect([&](size_t idx, const Server& s) {
            table_status = "Selected srv-" + s.name + " | CPU: " +
                           std::to_string(static_cast<int>(s.cpu)) + "% | " +
                           s.status;
            (void)idx;
          })
          .Build();

  // ── Tab container ──────────────────────────────────────────────────────────
  int active_tab = 0;

  auto tab_labels = Renderer([&]() -> Element {
    auto tab0 = text(" VirtualList (50k logs) ") | bold;
    auto tab1 = text(" SortableTable (200 servers) ") | bold;
    if (active_tab == 0) {
      tab0 = tab0 | bgcolor(theme.secondary) | color(Color::White);
    } else {
      tab1 = tab1 | bgcolor(theme.secondary) | color(Color::White);
    }
    return hbox({tab0, text("  "), tab1,
                 filler(),
                 text(" Tab=switch  q=quit ") | dim});
  });

  auto tab_content = Container::Tab({log_list, server_table}, &active_tab);

  // Status bar at bottom.
  auto status_bar = Renderer([&]() -> Element {
    const std::string& msg = (active_tab == 0) ? list_status : table_status;
    if (msg.empty()) {
      return text(" ↑↓ navigate  Type to filter  Esc clear  (SortableTable: ←→ col, Enter sort, PgDn/PgUp page) ") |
             dim | hcenter;
    }
    return text(" " + msg + " ") | color(theme.primary);
  });

  auto layout = Container::Vertical({
      Renderer(tab_labels, [&]() { return tab_labels->Render(); }),
      tab_content,
      Renderer(status_bar, [&]() { return status_bar->Render(); }),
  });

  auto root = Renderer(layout, [&]() -> Element {
    return vbox({
               hbox({
                   text("  ⚡ Virtual List & Sortable Table Demo  ") | bold |
                       color(theme.primary),
                   filler(),
               }),
               separatorLight(),
               tab_labels->Render(),
               separatorLight(),
               tab_content->Render() | flex,
               separator(),
               status_bar->Render(),
           }) |
           borderStyled(theme.border_style, theme.border_color) | flex;
  });

  // Tab key switches pane.
  root = CatchEvent(root, [&](Event event) -> bool {
    if (event == Event::Tab) {
      active_tab = (active_tab + 1) % 2;
      return true;
    }
    return false;
  });

  root |= Keymap()
              .Bind("q", [] {
                if (App* a = App::Active()) {
                  a->Exit();
                }
              }, "Quit")
              .AsDecorator();

  App::Fullscreen().Loop(root);
  return 0;
}
