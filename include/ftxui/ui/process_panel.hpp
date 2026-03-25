// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_PROCESS_PANEL_HPP
#define FTXUI_UI_PROCESS_PANEL_HPP

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Runs a subprocess and streams its stdout/stderr into a scrollable
/// panel.
///
/// @code
/// auto proc = ProcessPanel::Create();
/// proc->SetCommand("cmake --build build --parallel 8");
/// auto ui = proc->Build("Build Output");
///
/// // Or use the builder pattern:
/// auto ui = ProcessPanel::Builder()
///     .Command("git log --oneline -20")
///     .WorkDir("/path/to/repo")
///     .AutoStart(true)
///     .OnComplete([](int exit_code) { ... })
///     .Build("Git Log");
/// @endcode
///
/// @ingroup ui
class ProcessPanel : public std::enable_shared_from_this<ProcessPanel> {
 public:
  // Factory
  static std::shared_ptr<ProcessPanel> Create();

  /// @brief Fluent builder for ProcessPanel.
  class Builder {
   public:
    Builder();
    Builder& Command(const std::string& cmd);
    Builder& WorkDir(const std::string& dir);
    Builder& AutoStart(bool auto_start);
    Builder& OnComplete(std::function<void(int)> fn);
    Builder& OnOutput(std::function<void(const std::string&)> fn);
    ftxui::Component Build(const std::string& title = "Process");

   private:
    std::shared_ptr<ProcessPanel> panel_;
    bool auto_start_{false};
  };

  // Configuration (call before Start())
  void SetCommand(const std::string& cmd);
  void SetWorkDir(const std::string& dir);
  void SetEnv(const std::vector<std::pair<std::string, std::string>>& env);
  void OnComplete(std::function<void(int exit_code)> fn);
  void OnOutput(std::function<void(const std::string& line)> fn);

  // Control
  void Start();
  void Stop();   ///< Sends SIGTERM to the child process.
  void Clear();  ///< Clears the output buffer.

  bool IsRunning() const;
  int ExitCode() const;  ///< Valid only when !IsRunning().

  /// @brief Build the FTXUI component.
  /// Shows: title bar (command, status), scrollable output, control buttons.
  ftxui::Component Build(const std::string& title = "Process");

 private:
  ProcessPanel() = default;

  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_PROCESS_PANEL_HPP
