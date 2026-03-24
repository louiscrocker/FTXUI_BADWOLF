// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_FILEPICKER_HPP
#define FTXUI_UI_FILEPICKER_HPP

#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief File browser component using OS filesystem (std::filesystem).
///
/// @code
/// std::string selected_path;
/// auto picker = ui::FilePicker()
///     .StartDir(".")
///     .Filter([](const std::filesystem::path& p){
///         return p.extension() == ".cpp" || std::filesystem::is_directory(p);
///     })
///     .ShowHidden(false)
///     .OnSelect([&](const std::filesystem::path& p){ selected_path = p; })
///     .Build();
/// @endcode
///
/// @ingroup ui
class FilePicker {
 public:
  FilePicker();

  /// @brief Set the starting directory. Defaults to ".". Falls back to "." if
  /// the path doesn't exist.
  FilePicker& StartDir(std::string_view path = ".");

  /// @brief Optional filter applied to files (not directories).
  /// If not set all files are shown.
  FilePicker& Filter(std::function<bool(const std::filesystem::path&)> fn);

  /// @brief Whether to show hidden entries (those starting with '.').
  FilePicker& ShowHidden(bool v = false);

  /// @brief Called when the user presses Enter on a file (selection highlight
  /// changes immediately; confirm is the final "open" action).
  FilePicker& OnSelect(std::function<void(const std::filesystem::path&)> fn);

  /// @brief Called when the user presses Enter on a file (same event as
  /// OnSelect — both are called).
  FilePicker& OnConfirm(std::function<void(const std::filesystem::path&)> fn);

  /// @brief Build the FTXUI component.
  ftxui::Component Build();

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_FILEPICKER_HPP
