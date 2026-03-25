// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_CONFIG_EDITOR_HPP
#define FTXUI_UI_CONFIG_EDITOR_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief A persistent key-value configuration editor.
///
/// Renders a Form-like UI for editing string/bool/int/float config values,
/// with Load/Save buttons that persist to a file.
///
/// @code
/// auto cfg = ui::ConfigEditor()
///     .File("/tmp/myapp.cfg")
///     .Title("Application Settings")
///     .String("server_host",  "localhost",  "Hostname or IP")
///     .Int("server_port",     8080,         "Port number (1-65535)")
///     .Bool("enable_tls",     false,         "Use TLS encryption")
///     .Bool("debug_mode",     false,         "Enable debug logging")
///     .Float("timeout_sec",   30.0f,         "Request timeout in seconds")
///     .Section("Appearance")
///     .String("theme",        "Nord",        "Theme name")
///     .OnChange([&]{ /* called when any value changes */ })
///     .Build();
/// @endcode
///
/// Values are stored in a simple `key=value\n` text file.
/// The editor auto-loads on Build() if the file exists.
///
/// @ingroup ui
class ConfigEditor {
 public:
  ConfigEditor();

  ConfigEditor& File(std::string_view path);
  ConfigEditor& Title(std::string_view title);

  /// @brief Add a string field.
  ConfigEditor& String(std::string_view key,
                       std::string default_value,
                       std::string_view description = "");

  /// @brief Add an integer field (rendered as TextInput with validation).
  ConfigEditor& Int(std::string_view key,
                    int default_value,
                    std::string_view description = "");

  /// @brief Add a float field.
  ConfigEditor& Float(std::string_view key,
                      float default_value,
                      std::string_view description = "");

  /// @brief Add a boolean field (rendered as Checkbox).
  ConfigEditor& Bool(std::string_view key,
                     bool default_value,
                     std::string_view description = "");

  /// @brief Add a separator / section label.
  ConfigEditor& Section(std::string_view label);

  /// @brief Callback invoked when any value changes.
  ConfigEditor& OnChange(std::function<void()> fn);

  // ── Value accessors ────────────────────────────────────────────────────────

  std::string GetString(std::string_view key) const;
  int GetInt(std::string_view key) const;
  float GetFloat(std::string_view key) const;
  bool GetBool(std::string_view key) const;

  // ── Persistence ────────────────────────────────────────────────────────────

  /// @brief Write all values to the configured file. Returns true on success.
  bool Save() const;

  /// @brief Load values from the configured file. Returns true on success.
  bool Load();

  // ── Build ──────────────────────────────────────────────────────────────────

  /// @brief Build the FTXUI component. Auto-loads if the file exists.
  ftxui::Component Build();

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_CONFIG_EDITOR_HPP
