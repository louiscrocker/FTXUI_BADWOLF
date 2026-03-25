// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file ftgit/main.cpp
/// Beautiful git TUI — browse history, diffs, and status.
///
/// Layout:
///   Left:         git log graph in VirtualList
///   Right top:    commit detail/diff in LogPanel
///   Right bottom: git status
///   Status bar:   branch, ahead/behind
///
/// Usage: ftgit [repo-path]
/// Controls: up/down navigate, Enter=diff, r=refresh, q=quit

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/log_panel.hpp"
#include "ftxui/ui/process_panel.hpp"
#include "ftxui/ui/reactive.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/ui/virtual_list.hpp"

using namespace ftxui;
using namespace ftxui::ui;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

struct CommitEntry {
  std::string sha;       // short SHA (7 hex chars), empty for graph-only lines
  std::string graph;     // graph decoration prefix
  std::string message;   // commit subject
  std::string raw_line;  // original log line
};

struct StatusEntry {
  std::string code;  // 2-char status code
  std::string file;
};

// ---------------------------------------------------------------------------
// Git helpers
// ---------------------------------------------------------------------------

static std::vector<std::string> GitRun(const std::string& repo,
                                        const std::string& args) {
  std::string cmd = "git -C \"" + repo + "\" " + args + " 2>/dev/null";
  FILE* fp = popen(cmd.c_str(), "r");
  std::vector<std::string> out;
  if (!fp) return out;
  char buf[2048];
  while (fgets(buf, sizeof(buf), fp)) {
    std::string line(buf);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
      line.pop_back();
    out.push_back(line);
  }
  pclose(fp);
  return out;
}

static CommitEntry ParseLogLine(const std::string& line) {
  CommitEntry e;
  e.raw_line = line;

  // Walk past graph chars: * | / \ - space
  size_t i = 0;
  while (i < line.size()) {
    char c = line[i];
    if (c == '*' || c == '|' || c == '/' || c == '\\' || c == ' ' || c == '-')
      ++i;
    else
      break;
  }
  e.graph = line.substr(0, i);

  // Next 7 chars should be a hex SHA
  if (i + 7 <= line.size()) {
    bool is_hex = true;
    for (size_t k = i; k < i + 7; ++k)
      if (!std::isxdigit(static_cast<unsigned char>(line[k]))) { is_hex = false; break; }
    if (is_hex) {
      e.sha = line.substr(i, 7);
      e.message = (i + 8 < line.size()) ? line.substr(i + 8) : "";
    } else {
      e.message = line.substr(i);
    }
  }
  return e;
}

static std::vector<StatusEntry> ParseStatus(const std::vector<std::string>& lines) {
  std::vector<StatusEntry> result;
  for (const auto& l : lines) {
    if (l.size() < 3) continue;
    result.push_back({l.substr(0, 2), l.substr(3)});
  }
  return result;
}

static Color StatusColor(char c) {
  switch (c) {
    case 'M': return Color::Yellow;
    case 'A': return Color::Green;
    case 'D': return Color::Red;
    case 'R': return Color::Cyan;
    case '?': return Color::GrayLight;
    default:  return Color::Default;
  }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
  SetTheme(Theme::Dark());
  const Theme& theme = GetTheme();

  std::string repo_path = (argc > 1) ? argv[1] : ".";

  // ── Shared data (protected by commits_mutex) ──────────────────────────────
  std::mutex commits_mutex;
  auto commits = std::make_shared<std::vector<CommitEntry>>();

  Reactive<std::vector<StatusEntry>> status_entries;
  Reactive<std::string> branch_name{std::string("...")};
  Reactive<std::string> ahead_behind{std::string("")};

  // ── Load initial data ─────────────────────────────────────────────────────
  auto load_commits = [&]() {
    auto lines = GitRun(repo_path, "log --oneline --graph --color=never -60");
    std::vector<CommitEntry> fresh;
    fresh.reserve(lines.size());
    for (const auto& l : lines)
      fresh.push_back(ParseLogLine(l));
    std::lock_guard<std::mutex> lk(commits_mutex);
    *commits = std::move(fresh);
  };

  auto load_status = [&]() {
    auto sl = GitRun(repo_path, "status --short");
    status_entries.Set(ParseStatus(sl));
  };

  auto load_branch = [&]() {
    auto bl = GitRun(repo_path, "branch --show-current");
    if (!bl.empty()) branch_name.Set(bl[0]);
    // Ahead/behind
    auto abl = GitRun(repo_path,
        "rev-list --left-right --count HEAD...@{upstream}");
    if (!abl.empty()) {
      int ahead = 0, behind = 0;
      std::istringstream ss(abl[0]);
      ss >> ahead >> behind;
      if (ahead > 0 || behind > 0)
        ahead_behind.Set("↑" + std::to_string(ahead) + " ↓" + std::to_string(behind));
      else
        ahead_behind.Set("up to date");
    }
  };

  load_commits();
  load_branch();
  load_status();

  // ── Commit detail log panel ───────────────────────────────────────────────
  auto detail_log = LogPanel::Create(2000);
  detail_log->Info("Select a commit to view details.");
  detail_log->Info("Press Enter to show full diff.");

  // ── ProcessPanel that drives git-show ────────────────────────────────────
  auto show_panel = ProcessPanel::Create();
  show_panel->SetWorkDir(repo_path);
  show_panel->OnOutput([&](const std::string& line) {
    LogLevel lvl = LogLevel::Info;
    if (!line.empty()) {
      if (line[0] == '+' && (line.size() < 2 || line[1] != '+'))
        lvl = LogLevel::Debug;
      else if (line[0] == '-' && (line.size() < 2 || line[1] != '-'))
        lvl = LogLevel::Error;
      else if (line[0] == '@')
        lvl = LogLevel::Warn;
    }
    detail_log->Log(line, lvl);
  });

  std::string selected_sha;
  std::atomic<bool> running{true};

  auto run_git_show = [&](const std::string& sha, bool full_diff) {
    if (sha.empty()) return;
    selected_sha = sha;
    detail_log->Clear();
    detail_log->Info("── " + sha + " ──────────────────────────────");
    show_panel->Stop();
    show_panel->Clear();
    std::string cmd = "git -C \"" + repo_path + "\" show " +
                      (full_diff ? "" : "--stat ") + sha;
    show_panel->SetCommand(cmd);
    show_panel->Start();
  };

  // ── VirtualList for commits ───────────────────────────────────────────────
  auto commit_list =
      VirtualList<CommitEntry>()
          .Items(commits)
          .RowHeight(1)
          .Render([&](const CommitEntry& e, bool sel) -> Element {
            Element graph = text(e.graph) | color(Color::GreenLight);
            Element sha_e = e.sha.empty()
                                ? text("       ")
                                : (text(e.sha) | color(Color::Yellow) | bold);
            Element msg   = text(" " + e.message);
            Element row   = hbox({graph, sha_e, msg, filler()});
            return sel ? (row | bgcolor(theme.secondary) | color(Color::White)) : row;
          })
          .OnSelect([&](size_t /*idx*/, const CommitEntry& e) {
            if (!e.sha.empty()) run_git_show(e.sha, false);
          })
          .Build();

  // ── Status renderer ───────────────────────────────────────────────────────
  auto status_renderer = Renderer([&]() -> Element {
    const auto& entries = *status_entries;
    if (entries.empty())
      return text("  nothing to commit, working tree clean") | color(theme.accent);
    Elements rows;
    for (const auto& s : entries) {
      Color c = s.code.empty() ? Color::Default : StatusColor(s.code[0]);
      rows.push_back(hbox({
          text(" " + s.code + " ") | color(c) | bold,
          text(s.file) | color(theme.text_muted),
          filler(),
      }));
    }
    return vbox(std::move(rows));
  });

  // ── Detail panel component ────────────────────────────────────────────────
  auto detail_comp = detail_log->Build();

  // ── Status bar ────────────────────────────────────────────────────────────
  auto statusbar = Renderer([&]() -> Element {
    return hbox({
        text(" ftgit ") | bold | color(theme.primary),
        separatorLight(),
        text("  "),
        text(*branch_name) | bold | color(theme.accent),
        text("  "),
        text(*ahead_behind) | color(theme.text_muted),
        filler(),
        text(" ↑↓") | color(theme.accent), text(":nav "),
        text("Enter") | color(theme.accent), text(":diff "),
        text("r") | color(theme.accent), text(":refresh "),
        text("q") | color(theme.accent), text(":quit "),
    });
  });

  // ── Layout ────────────────────────────────────────────────────────────────
  // Left: log list
  // Right: detail (flex) + status (fixed height)
  auto left_container = Container::Vertical({commit_list});
  auto right_container = Container::Vertical({detail_comp, status_renderer});

  auto left_render = Renderer(left_container, [&]() -> Element {
    return window(text(" Log "), commit_list->Render() | flex) | flex;
  });

  auto right_render = Renderer(right_container, [&]() -> Element {
    return vbox({
        window(text(" Detail "), detail_comp->Render() | flex) | flex,
        window(text(" Status "), status_renderer->Render() |
                                     size(HEIGHT, LESS_THAN, 10)),
    }) | flex;
  });

  auto split = Container::Horizontal({left_render, right_render});
  auto body = Renderer(split, [&]() -> Element {
    return hbox({
        left_render->Render() | size(WIDTH, LESS_THAN, 52),
        right_render->Render() | flex,
    }) | flex;
  });

  auto all = Container::Vertical({body, statusbar});
  auto root = Renderer(all, [&]() -> Element {
    return vbox({
        body->Render() | flex,
        separator(),
        statusbar->Render(),
    }) | flex;
  });

  root |= CatchEvent([&](Event e) -> bool {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
      running.store(false);
      show_panel->Stop();
      if (App* a = App::Active()) a->Exit();
      return true;
    }
    if (e == Event::Return) {
      if (!selected_sha.empty()) run_git_show(selected_sha, true);
      return true;
    }
    if (e == Event::Character('r') || e == Event::Character('R')) {
      std::thread([&] {
        load_commits();
        load_branch();
        load_status();
        if (App* a = App::Active()) a->PostEvent(Event::Custom);
      }).detach();
      return true;
    }
    return false;
  });

  RunFullscreen(root);
  running.store(false);
  show_panel->Stop();
  return 0;
}
