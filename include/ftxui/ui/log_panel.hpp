// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_LOG_PANEL_HPP
#define FTXUI_UI_LOG_PANEL_HPP

#include <memory>
#include <string>
#include <string_view>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

/// @brief Thread-safe scrolling log panel component.
///
/// Example:
/// @code
/// auto log = ftxui::ui::LogPanel::Create();
/// log->SetMinLevel(ftxui::ui::LogLevel::Debug);
///
/// // From any thread:
/// log->Info("Server started");
/// log->Warn("Low memory");
/// log->Error("Connection refused");
///
/// ftxui::ui::Run(log->Build("Application Log"));
/// @endcode
///
/// @ingroup ui
class LogPanel {
 public:
  static std::shared_ptr<LogPanel> Create(size_t max_lines = 1000);

  void Log(std::string_view message, LogLevel level = LogLevel::Info);
  void Trace(std::string_view message);
  void Debug(std::string_view message);
  void Info(std::string_view message);
  void Warn(std::string_view message);
  void Error(std::string_view message);

  void Clear();
  void SetMaxLines(size_t n);
  void SetMinLevel(LogLevel level);

  // Returns a Component that renders the log with auto-scroll and level colors.
  // title: optional panel title (empty = no border)
  ftxui::Component Build(std::string_view title = "") const;

 private:
  LogPanel() = default;
  struct Entry {
    std::string message;
    LogLevel level;
    std::string timestamp;
  };
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_LOG_PANEL_HPP
