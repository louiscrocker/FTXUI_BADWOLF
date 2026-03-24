// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/process_panel.hpp"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/log_panel.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Impl ──────────────────────────────────────────────────────────────────────

struct ProcessPanel::Impl {
  std::string command;
  std::string work_dir;
  std::vector<std::pair<std::string, std::string>> env;
  std::function<void(int)> on_complete;
  std::function<void(const std::string&)> on_output;

  std::atomic<bool> running{false};
  std::atomic<int> exit_code{-1};

  std::mutex proc_mutex;
  pid_t child_pid{-1};
  int read_fd{-1};

  std::shared_ptr<LogPanel> log{LogPanel::Create(1000)};
};

// ── Factory ───────────────────────────────────────────────────────────────────

std::shared_ptr<ProcessPanel> ProcessPanel::Create() {
  auto panel = std::shared_ptr<ProcessPanel>(new ProcessPanel());
  panel->impl_ = std::make_shared<Impl>();
  return panel;
}

// ── Builder ───────────────────────────────────────────────────────────────────

ProcessPanel::Builder::Builder() : panel_(ProcessPanel::Create()) {}

ProcessPanel::Builder& ProcessPanel::Builder::Command(const std::string& cmd) {
  panel_->SetCommand(cmd);
  return *this;
}

ProcessPanel::Builder& ProcessPanel::Builder::WorkDir(const std::string& dir) {
  panel_->SetWorkDir(dir);
  return *this;
}

ProcessPanel::Builder& ProcessPanel::Builder::AutoStart(bool auto_start) {
  auto_start_ = auto_start;
  return *this;
}

ProcessPanel::Builder& ProcessPanel::Builder::OnComplete(
    std::function<void(int)> fn) {
  panel_->OnComplete(std::move(fn));
  return *this;
}

ProcessPanel::Builder& ProcessPanel::Builder::OnOutput(
    std::function<void(const std::string&)> fn) {
  panel_->OnOutput(std::move(fn));
  return *this;
}

ftxui::Component ProcessPanel::Builder::Build(const std::string& title) {
  if (auto_start_) panel_->Start();
  return panel_->Build(title);
}

// ── Configuration ─────────────────────────────────────────────────────────────

void ProcessPanel::SetCommand(const std::string& cmd) {
  impl_->command = cmd;
}

void ProcessPanel::SetWorkDir(const std::string& dir) {
  impl_->work_dir = dir;
}

void ProcessPanel::SetEnv(
    const std::vector<std::pair<std::string, std::string>>& env) {
  impl_->env = env;
}

void ProcessPanel::OnComplete(std::function<void(int exit_code)> fn) {
  impl_->on_complete = std::move(fn);
}

void ProcessPanel::OnOutput(std::function<void(const std::string& line)> fn) {
  impl_->on_output = std::move(fn);
}

// ── Control ───────────────────────────────────────────────────────────────────

void ProcessPanel::Start() {
  if (impl_->running) return;
  if (impl_->command.empty()) {
    impl_->log->Warn("No command set.");
    return;
  }

  impl_->running = true;
  impl_->exit_code = -1;
  impl_->log->Clear();

  // Build shell command (with optional workdir and stderr merged into stdout).
  std::string shell_cmd = impl_->command;
  if (!impl_->work_dir.empty()) {
    shell_cmd = "cd " + impl_->work_dir + " && " + shell_cmd;
  }
  shell_cmd += " 2>&1";

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    impl_->log->Error("Failed to create pipe.");
    impl_->running = false;
    return;
  }

  pid_t pid = fork();
  if (pid < 0) {
    impl_->log->Error("fork() failed.");
    close(pipefd[0]);
    close(pipefd[1]);
    impl_->running = false;
    return;
  }

  if (pid == 0) {
    // Child: redirect stdout+stderr to pipe write end, then exec.
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    execl("/bin/sh", "sh", "-c", shell_cmd.c_str(), nullptr);
    _exit(127);
  }

  // Parent: record the child PID and the read end of the pipe.
  close(pipefd[1]);
  {
    std::lock_guard<std::mutex> lk(impl_->proc_mutex);
    impl_->child_pid = pid;
    impl_->read_fd = pipefd[0];
  }

  // Detached reader thread: read lines, feed LogPanel, and reap the child.
  auto impl = impl_;
  std::thread([impl]() {
    char buf[4096];
    std::string partial;

    ssize_t n;
    int fd;
    {
      std::lock_guard<std::mutex> lk(impl->proc_mutex);
      fd = impl->read_fd;
    }

    while ((n = ::read(fd, buf, sizeof(buf) - 1)) > 0) {
      buf[n] = '\0';
      partial += buf;
      size_t pos;
      while ((pos = partial.find('\n')) != std::string::npos) {
        std::string line = partial.substr(0, pos);
        partial = partial.substr(pos + 1);
        impl->log->Info(line);
        if (impl->on_output) impl->on_output(line);
        if (App* a = App::Active()) a->Post([] {});
      }
    }

    // Flush any trailing partial line (no trailing newline).
    if (!partial.empty()) {
      impl->log->Info(partial);
      if (impl->on_output) impl->on_output(partial);
      if (App* a = App::Active()) a->Post([] {});
    }

    {
      std::lock_guard<std::mutex> lk(impl->proc_mutex);
      close(impl->read_fd);
      impl->read_fd = -1;
    }

    // Reap the child process.
    int status = 0;
    pid_t child;
    {
      std::lock_guard<std::mutex> lk(impl->proc_mutex);
      child = impl->child_pid;
    }
    if (child > 0) {
      waitpid(child, &status, 0);
      std::lock_guard<std::mutex> lk(impl->proc_mutex);
      impl->child_pid = -1;
    }

    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    impl->exit_code = code;
    impl->running = false;

    if (App* a = App::Active()) a->Post([] {});
    if (impl->on_complete) impl->on_complete(code);
  }).detach();
}

void ProcessPanel::Stop() {
  std::lock_guard<std::mutex> lk(impl_->proc_mutex);
  if (impl_->child_pid > 0) {
    kill(impl_->child_pid, SIGTERM);
  }
}

void ProcessPanel::Clear() {
  impl_->log->Clear();
}

bool ProcessPanel::IsRunning() const {
  return impl_->running;
}

int ProcessPanel::ExitCode() const {
  return impl_->exit_code;
}

// ── Build ─────────────────────────────────────────────────────────────────────

ftxui::Component ProcessPanel::Build(const std::string& title) {
  auto self = shared_from_this();
  auto impl = impl_;

  // Create buttons ONCE here — never inside the Renderer lambda.
  auto btn_start = Button(" ▶ START ", [self]() { self->Start(); });
  auto btn_stop  = Button(" ■ STOP ",  [self]() { self->Stop(); });
  auto btn_clear = Button(" ✗ CLEAR ", [self]() { self->Clear(); });

  auto log_comp = impl->log->Build();

  auto btn_row  = Container::Horizontal({btn_start, btn_stop, btn_clear});
  auto container = Container::Vertical({btn_row, log_comp});

  return Renderer(
      container,
      [impl, title, btn_start, btn_stop, btn_clear, log_comp]() -> Element {
        const Theme& t = GetTheme();

        // Determine status colour and label.
        Color dot_color;
        std::string status_text;
        if (impl->running) {
          dot_color   = t.success_color;
          status_text = "Running";
        } else if (impl->exit_code == 0) {
          dot_color   = t.text_muted;
          status_text = "Done (0)";
        } else if (impl->exit_code > 0) {
          dot_color   = t.error_color;
          status_text = "Error (" + std::to_string(impl->exit_code.load()) + ")";
        } else {
          dot_color   = t.text_muted;
          status_text = "Idle";
        }

        // Top info bar: command + status + control buttons.
        Element top_bar = hbox({
          text(" cmd: ") | color(t.text_muted) | dim,
          text(impl->command) | color(t.text) | flex,
          text("  Status: ") | dim,
          text("● ") | color(dot_color),
          text(status_text + "  "),
          btn_start->Render(),
          text(" "),
          btn_stop->Render(),
          text(" "),
          btn_clear->Render(),
          text(" "),
        });

        return window(
            text(" " + title + " "),
            vbox({
              top_bar,
              separatorLight(),
              log_comp->Render() | flex,
            }) | flex,
            t.border_style);
      });
}

}  // namespace ftxui::ui
