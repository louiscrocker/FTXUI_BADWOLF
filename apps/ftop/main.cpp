// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file ftop/main.cpp
/// A real-time system monitor — htop replacement.
///
/// Shows CPU usage per core, memory, top processes, load averages, and uptime.
/// Updates every second via a background thread.
///
/// Controls:  q=quit  s=sort(cpu/mem)  k=kill  ↑↓=navigate

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/charts.hpp"
#include "ftxui/ui/reactive.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;
using namespace ftxui::ui;
using namespace std::chrono_literals;

// ── Data types ────────────────────────────────────────────────────────────────

struct ProcessRow {
  int pid = 0;
  std::string name;
  float cpu_pct = 0.f;
  float mem_pct = 0.f;
  std::string state;
};

struct SystemMetrics {
  std::vector<float> cpu_cores;   // per-core usage 0–100
  float mem_used_gb = 0.f;
  float mem_total_gb = 0.f;
  double load_1 = 0.0;
  double load_5 = 0.0;
  double load_15 = 0.0;
  std::vector<ProcessRow> procs;
  std::string uptime_str;
};

// ── Platform: macOS ──────────────────────────────────────────────────────────
#ifdef __APPLE__

static std::vector<integer_t> g_prev_cpu_ticks;

static std::vector<float> ReadCPUUsage() {
  natural_t num_cpus = 0;
  processor_info_array_t cpu_info = nullptr;
  mach_msg_type_number_t num_cpu_info = 0;

  if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &num_cpus,
                          &cpu_info, &num_cpu_info) != KERN_SUCCESS) {
    return {};
  }

  std::vector<integer_t> curr(num_cpus * CPU_STATE_MAX);
  for (natural_t i = 0; i < num_cpus; ++i) {
    for (int j = 0; j < CPU_STATE_MAX; ++j) {
      curr[i * CPU_STATE_MAX + j] = cpu_info[i * CPU_STATE_MAX + j];
    }
  }
  vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(cpu_info),
                num_cpu_info * sizeof(*cpu_info));

  std::vector<float> usage(num_cpus, 0.f);
  if (g_prev_cpu_ticks.size() == curr.size()) {
    for (natural_t i = 0; i < num_cpus; ++i) {
      const integer_t* c = &curr[i * CPU_STATE_MAX];
      const integer_t* p = &g_prev_cpu_ticks[i * CPU_STATE_MAX];
      uint64_t user = static_cast<uint64_t>(c[CPU_STATE_USER] - p[CPU_STATE_USER]);
      uint64_t sys  = static_cast<uint64_t>(c[CPU_STATE_SYSTEM] - p[CPU_STATE_SYSTEM]);
      uint64_t idle = static_cast<uint64_t>(c[CPU_STATE_IDLE] - p[CPU_STATE_IDLE]);
      uint64_t nice = static_cast<uint64_t>(c[CPU_STATE_NICE] - p[CPU_STATE_NICE]);
      uint64_t total = user + sys + idle + nice;
      usage[i] = (total > 0) ? 100.f * static_cast<float>(user + sys + nice) / static_cast<float>(total) : 0.f;
    }
  }
  g_prev_cpu_ticks = curr;
  return usage;
}

static void ReadMemory(float& used_gb, float& total_gb) {
  // Total physical memory
  int mib[2] = {CTL_HW, HW_MEMSIZE};
  uint64_t total_bytes = 0;
  size_t len = sizeof(total_bytes);
  sysctl(mib, 2, &total_bytes, &len, nullptr, 0);
  total_gb = static_cast<float>(total_bytes) / (1024.f * 1024.f * 1024.f);

  // VM statistics
  vm_statistics64_data_t vm_stat;
  mach_msg_type_number_t info_count = HOST_VM_INFO64_COUNT;
  vm_size_t page_size_val = 0;
  host_page_size(mach_host_self(), &page_size_val);

  if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                        reinterpret_cast<host_info64_t>(&vm_stat),
                        &info_count) == KERN_SUCCESS) {
    uint64_t used_pages = vm_stat.active_count + vm_stat.inactive_count +
                          vm_stat.wire_count + vm_stat.compressor_page_count;
    used_gb = static_cast<float>(used_pages * page_size_val) / (1024.f * 1024.f * 1024.f);
  }
}

static std::string ReadUptime() {
  struct timeval boot_time;
  size_t size = sizeof(boot_time);
  int mib[2] = {CTL_KERN, KERN_BOOTTIME};
  if (sysctl(mib, 2, &boot_time, &size, nullptr, 0) == 0) {
    time_t now = time(nullptr);
    long uptime_secs = now - boot_time.tv_sec;
    int days = static_cast<int>(uptime_secs / 86400);
    int hours = static_cast<int>((uptime_secs % 86400) / 3600);
    int mins  = static_cast<int>((uptime_secs % 3600) / 60);
    char buf[64];
    if (days > 0) {
      snprintf(buf, sizeof(buf), "%dd %dh %dm", days, hours, mins);
    } else if (hours > 0) {
      snprintf(buf, sizeof(buf), "%dh %dm", hours, mins);
    } else {
      snprintf(buf, sizeof(buf), "%dm %ds", mins, static_cast<int>(uptime_secs % 60));
    }
    return buf;
  }
  return "unknown";
}

static std::vector<ProcessRow> ReadProcesses(int max_count) {
  std::vector<ProcessRow> rows;
  FILE* fp = popen("ps -axo pid,pcpu,pmem,state,comm 2>/dev/null", "r");
  if (!fp) return rows;

  char line[512];
  bool first = true;
  while (fgets(line, sizeof(line), fp)) {
    if (first) { first = false; continue; }  // skip header
    int pid;
    float cpu, mem;
    char state[8], comm[256];
    if (sscanf(line, "%d %f %f %7s %255s", &pid, &cpu, &mem, state, comm) == 5) {
      ProcessRow r;
      r.pid = pid;
      r.cpu_pct = cpu;
      r.mem_pct = mem;
      r.state = state;
      // Extract just the basename of comm
      std::string full(comm);
      size_t slash = full.rfind('/');
      r.name = (slash != std::string::npos) ? full.substr(slash + 1) : full;
      if (r.name.size() > 20) r.name.resize(20);
      rows.push_back(r);
    }
  }
  pclose(fp);

  std::sort(rows.begin(), rows.end(),
            [](const ProcessRow& a, const ProcessRow& b) { return a.cpu_pct > b.cpu_pct; });
  if (static_cast<int>(rows.size()) > max_count) rows.resize(static_cast<size_t>(max_count));
  return rows;
}

// ── Platform: Linux ──────────────────────────────────────────────────────────
#else

struct LinuxCPUStat {
  uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
  uint64_t total_idle() const { return idle + iowait; }
  uint64_t total_active() const { return user + nice + system + irq + softirq + steal; }
  uint64_t total() const { return total_idle() + total_active(); }
};

static std::vector<LinuxCPUStat> g_prev_cpu_stats;

static std::vector<float> ReadCPUUsage() {
  std::ifstream f("/proc/stat");
  if (!f) return {};

  std::vector<LinuxCPUStat> curr;
  std::string line;
  while (std::getline(f, line)) {
    if (line.substr(0, 3) != "cpu") break;
    if (line.size() < 4 || !std::isdigit(line[3])) continue;  // skip overall "cpu " line
    LinuxCPUStat s{};
    sscanf(line.c_str() + 4, "%lu %lu %lu %lu %lu %lu %lu %lu",
           &s.user, &s.nice, &s.system, &s.idle,
           &s.iowait, &s.irq, &s.softirq, &s.steal);
    curr.push_back(s);
  }

  std::vector<float> usage(curr.size(), 0.f);
  if (g_prev_cpu_stats.size() == curr.size()) {
    for (size_t i = 0; i < curr.size(); ++i) {
      uint64_t d_active = curr[i].total_active() - g_prev_cpu_stats[i].total_active();
      uint64_t d_total  = curr[i].total() - g_prev_cpu_stats[i].total();
      usage[i] = (d_total > 0) ? 100.f * static_cast<float>(d_active) / static_cast<float>(d_total) : 0.f;
    }
  }
  g_prev_cpu_stats = curr;
  return usage;
}

static void ReadMemory(float& used_gb, float& total_gb) {
  std::ifstream f("/proc/meminfo");
  if (!f) return;
  uint64_t mem_total = 0, mem_available = 0;
  std::string line;
  while (std::getline(f, line)) {
    if (sscanf(line.c_str(), "MemTotal: %lu kB", &mem_total) == 1) continue;
    if (sscanf(line.c_str(), "MemAvailable: %lu kB", &mem_available) == 1) continue;
  }
  total_gb = static_cast<float>(mem_total) / (1024.f * 1024.f);
  used_gb  = static_cast<float>(mem_total - mem_available) / (1024.f * 1024.f);
}

static std::string ReadUptime() {
  std::ifstream f("/proc/uptime");
  double up_secs = 0.0;
  f >> up_secs;
  long s = static_cast<long>(up_secs);
  int days = static_cast<int>(s / 86400);
  int hours = static_cast<int>((s % 86400) / 3600);
  int mins  = static_cast<int>((s % 3600) / 60);
  char buf[64];
  if (days > 0) snprintf(buf, sizeof(buf), "%dd %dh %dm", days, hours, mins);
  else if (hours > 0) snprintf(buf, sizeof(buf), "%dh %dm", hours, mins);
  else snprintf(buf, sizeof(buf), "%dm %ds", mins, static_cast<int>(s % 60));
  return buf;
}

static std::vector<ProcessRow> ReadProcesses(int max_count) {
  std::vector<ProcessRow> rows;
  long clk_tck = sysconf(_SC_CLK_TCK);
  if (clk_tck <= 0) clk_tck = 100;

  // Get total memory for mem_pct calculation
  uint64_t mem_total_kb = 0;
  {
    std::ifstream mf("/proc/meminfo");
    std::string line;
    while (std::getline(mf, line)) {
      if (sscanf(line.c_str(), "MemTotal: %lu kB", &mem_total_kb) == 1) break;
    }
  }

  DIR* proc_dir = opendir("/proc");
  if (!proc_dir) return rows;

  struct dirent* entry;
  while ((entry = readdir(proc_dir)) != nullptr) {
    if (!std::isdigit(entry->d_name[0])) continue;
    int pid = atoi(entry->d_name);

    // Read process name
    std::string name;
    {
      std::ifstream cf(std::string("/proc/") + entry->d_name + "/comm");
      if (cf) std::getline(cf, name);
    }
    if (name.empty()) continue;
    if (name.size() > 20) name.resize(20);

    // Read stat for cpu/state/memory
    std::ifstream sf(std::string("/proc/") + entry->d_name + "/stat");
    if (!sf) continue;

    std::string stat_line;
    std::getline(sf, stat_line);

    // Parse: pid (name) state ppid pgroup session tty_nr ...
    // Fields after the closing ')': state=3rd, rss=24th
    size_t rparen = stat_line.rfind(')');
    if (rparen == std::string::npos) continue;

    char state_c = '?';
    unsigned long utime = 0, stime = 0;
    long rss = 0;
    sscanf(stat_line.c_str() + rparen + 2,
           "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u "
           "%lu %lu %*d %*d %*d %*d %*d %*d %*u %*u %ld",
           &state_c, &utime, &stime, &rss);

    ProcessRow r;
    r.pid = pid;
    r.name = name;
    r.state = std::string(1, state_c);
    // Rough CPU% (utime+stime in ticks / total ticks) – not delta-based here
    // for simplicity; real htop does delta sampling
    r.cpu_pct = static_cast<float>(utime + stime) / static_cast<float>(clk_tck);
    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
    r.mem_pct = (mem_total_kb > 0)
                    ? 100.f * static_cast<float>(rss * page_size_kb) / static_cast<float>(mem_total_kb)
                    : 0.f;
    rows.push_back(r);
  }
  closedir(proc_dir);

  std::sort(rows.begin(), rows.end(),
            [](const ProcessRow& a, const ProcessRow& b) { return a.cpu_pct > b.cpu_pct; });
  if (static_cast<int>(rows.size()) > max_count) rows.resize(static_cast<size_t>(max_count));
  return rows;
}

#endif  // __APPLE__ / Linux

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string FormatGB(float gb) {
  char buf[32];
  if (gb >= 10.f) snprintf(buf, sizeof(buf), "%.1f GB", gb);
  else            snprintf(buf, sizeof(buf), "%.2f GB", gb);
  return buf;
}

static Color CPUColor(float pct) {
  if (pct < 50.f) return Color::Green;
  if (pct < 80.f) return Color::Yellow;
  return Color::Red;
}

static Color MemColor(float frac) {
  if (frac < 0.6f) return Color::Cyan;
  if (frac < 0.85f) return Color::Yellow;
  return Color::Red;
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
  SetTheme(Theme::Dark());
  const Theme& theme = GetTheme();

  // ── Reactive state ──────────────────────────────────────────────────────
  Reactive<std::vector<float>> cpu_cores;
  Reactive<float> mem_used_gb{0.f};
  Reactive<float> mem_total_gb{0.f};
  Reactive<std::vector<ProcessRow>> processes;
  Reactive<std::string> uptime_str{std::string("...")};
  Reactive<std::vector<float>> load_history{std::vector<float>(60, 0.f)};

  double load_avgs[3] = {0.0, 0.0, 0.0};
  getloadavg(load_avgs, 3);

  // ── Process table state ─────────────────────────────────────────────────
  int selected_proc = 0;
  bool sort_by_mem = false;  // false=CPU, true=MEM

  // ── Stop flag ────────────────────────────────────────────────────────────
  std::atomic<bool> running{true};

  // ── Background sampling thread ───────────────────────────────────────────
  std::thread sampler([&] {
    // Prime the CPU delta sampler
    ReadCPUUsage();
    std::this_thread::sleep_for(200ms);

    while (running.load()) {
      ReactiveGroup batch;
      batch.BeginBatch();

      // CPU
      auto cores = ReadCPUUsage();
      cpu_cores.Set(cores);

      // Memory
      float used = 0.f, total = 0.f;
      ReadMemory(used, total);
      mem_used_gb.Set(used);
      mem_total_gb.Set(total);

      // Load average
      double la[3] = {0.0, 0.0, 0.0};
      getloadavg(la, 3);
      load_avgs[0] = la[0]; load_avgs[1] = la[1]; load_avgs[2] = la[2];
      load_history.Update([la](std::vector<float>& v) {
        v.erase(v.begin());
        v.push_back(static_cast<float>(la[0]));
      });

      // Uptime
      uptime_str.Set(ReadUptime());

      // Processes
      auto procs = ReadProcesses(20);
      if (sort_by_mem) {
        std::sort(procs.begin(), procs.end(),
                  [](const ProcessRow& a, const ProcessRow& b) { return a.mem_pct > b.mem_pct; });
      }
      processes.Set(procs);

      batch.EndBatch();

      std::this_thread::sleep_for(1s);
    }
  });

  // ── CPU section ───────────────────────────────────────────────────────────
  auto cpu_renderer = Renderer([&] {
    const auto& cores = *cpu_cores;
    Elements rows;
    int col_count = std::max(1, static_cast<int>(cores.size()));
    int cols = (col_count <= 4) ? col_count : (col_count <= 8) ? 2 : 4;
    int rows_n = (col_count + cols - 1) / cols;

    for (int r = 0; r < rows_n; ++r) {
      Elements row_elems;
      for (int c = 0; c < cols; ++c) {
        int idx = r * cols + c;
        if (idx >= col_count) {
          row_elems.push_back(filler());
          continue;
        }
        float pct = idx < static_cast<int>(cores.size()) ? cores[static_cast<size_t>(idx)] : 0.f;
        char label[8];
        snprintf(label, sizeof(label), "C%-2d", idx);
        char val[8];
        snprintf(val, sizeof(val), "%3.0f%%", pct);
        row_elems.push_back(
            hbox({
              text(label) | color(theme.text_muted) | size(WIDTH, EQUAL, 4),
              gauge(pct / 100.f) | color(CPUColor(pct)) | flex,
              text(val) | color(CPUColor(pct)) | size(WIDTH, EQUAL, 5),
            }) | flex);
        if (c + 1 < cols) row_elems.push_back(text("  "));
      }
      rows.push_back(hbox(std::move(row_elems)));
    }

    return window(text(" CPU "), vbox(std::move(rows)));
  });

  // ── Memory + Load section ─────────────────────────────────────────────────
  auto mem_load_renderer = Renderer([&] {
    float used = *mem_used_gb;
    float total = *mem_total_gb;
    float frac = (total > 0.f) ? used / total : 0.f;

    char mem_label[64];
    snprintf(mem_label, sizeof(mem_label), " %s / %s",
             FormatGB(used).c_str(), FormatGB(total).c_str());

    Element mem_bar = hbox({
        text(" MEM") | color(theme.text_muted) | size(WIDTH, EQUAL, 5),
        gauge(frac) | color(MemColor(frac)) | flex,
        text(mem_label) | color(MemColor(frac)) | size(WIDTH, EQUAL, 18),
    });

    char load_label[64];
    snprintf(load_label, sizeof(load_label), " %.2f  %.2f  %.2f",
             load_avgs[0], load_avgs[1], load_avgs[2]);

    const auto& hist = *load_history;
    Element load_row = hbox({
        text(" Load:") | color(theme.text_muted) | size(WIDTH, EQUAL, 7),
        Sparkline(hist, 40, Color::Cyan),
        text(load_label) | color(theme.text_muted),
        filler(),
        text("uptime: " + *uptime_str) | color(theme.text_muted),
        text(" "),
    });

    return vbox({mem_bar, load_row});
  });

  // ── Process table ─────────────────────────────────────────────────────────
  auto proc_renderer = Renderer([&] {
    const auto& procs = *processes;
    const int n = static_cast<int>(procs.size());

    // Header
    std::vector<std::vector<std::string>> table_data;
    table_data.push_back({"PID", "NAME", "CPU%", "MEM%", "STATE"});
    for (int i = 0; i < n; ++i) {
      const auto& p = procs[static_cast<size_t>(i)];
      char cpu_buf[10], mem_buf[10];
      snprintf(cpu_buf, sizeof(cpu_buf), "%.1f", p.cpu_pct);
      snprintf(mem_buf, sizeof(mem_buf), "%.1f", p.mem_pct);
      table_data.push_back({
          std::to_string(p.pid),
          p.name,
          cpu_buf,
          mem_buf,
          p.state,
      });
    }

    auto tbl = Table(table_data);
    tbl.SelectAll().Border(LIGHT);
    tbl.SelectRow(0).Decorate(bold);
    tbl.SelectRow(0).DecorateCells(color(theme.primary));

    if (selected_proc + 1 < static_cast<int>(table_data.size())) {
      tbl.SelectRow(selected_proc + 1).Decorate(inverted);
    }

    // Color CPU column
    for (int i = 1; i < static_cast<int>(table_data.size()); ++i) {
      const auto& p = procs[static_cast<size_t>(i - 1)];
      float pct = p.cpu_pct;
      if (pct > 0.1f) {
        tbl.SelectCell(i, 2).Decorate(color(CPUColor(pct)));
      }
    }

    tbl.SelectColumn(0).DecorateCells(color(theme.text_muted));
    tbl.SelectColumn(1).DecorateCells(bold);
    return window(text(" Processes (sort: " + std::string(sort_by_mem ? "MEM" : "CPU") + ") "),
                  tbl.Render() | flex_grow);
  });

  // ── Status bar ────────────────────────────────────────────────────────────
  auto status_bar = Renderer([&] {
    return hbox({
        text(" ftop ") | bold | color(theme.primary),
        separatorLight(),
        text(" q") | bold | color(theme.accent),
        text(":quit ") | color(theme.text_muted),
        text("s") | bold | color(theme.accent),
        text(":sort ") | color(theme.text_muted),
        text("k") | bold | color(theme.accent),
        text(":kill ") | color(theme.text_muted),
        text("↑↓") | bold | color(theme.accent),
        text(":navigate") | color(theme.text_muted),
        filler(),
        text(sort_by_mem ? " [MEM] " : " [CPU] ") | color(theme.warning_color),
    });
  });

  // ── Root layout ───────────────────────────────────────────────────────────
  auto layout = Container::Vertical({
      cpu_renderer,
      mem_load_renderer,
      proc_renderer,
      status_bar,
  });

  auto root = Renderer(layout, [&] {
    return vbox({
               cpu_renderer->Render(),
               mem_load_renderer->Render(),
               proc_renderer->Render() | flex,
               separator(),
               status_bar->Render(),
           }) |
           flex;
  });

  root |= CatchEvent([&](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      running.store(false);
      if (App* a = App::Active()) a->Exit();
      return true;
    }
    if (e == Event::Character('s') || e == Event::Character('S')) {
      sort_by_mem = !sort_by_mem;
      return true;
    }
    if (e == Event::ArrowUp) {
      if (selected_proc > 0) --selected_proc;
      return true;
    }
    if (e == Event::ArrowDown) {
      const auto& procs = *processes;
      if (selected_proc < static_cast<int>(procs.size()) - 1) ++selected_proc;
      return true;
    }
    if (e == Event::Character('k') || e == Event::Character('K')) {
      const auto& procs = *processes;
      if (selected_proc >= 0 && selected_proc < static_cast<int>(procs.size())) {
        int pid = procs[static_cast<size_t>(selected_proc)].pid;
        kill(pid, SIGTERM);
      }
      return true;
    }
    return false;
  });

  RunFullscreen(root);
  running.store(false);
  sampler.join();
  return 0;
}
