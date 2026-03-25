// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_NOTIFICATION_HPP
#define FTXUI_UI_NOTIFICATION_HPP

#include <chrono>
#include <string_view>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Notification severity level for toast messages.
/// @ingroup ui
enum class Severity { Info, Success, Warning, Error, Debug };

/// @brief Post a toast notification. Thread-safe.
///
/// The notification auto-dismisses after @p duration.
/// Pass `duration = 0` for a persistent notification.
void Notify(
    std::string_view message,
    Severity severity = Severity::Info,
    std::chrono::milliseconds duration = std::chrono::milliseconds(3000));

/// @brief Clear all active notifications immediately.
void ClearNotifications();

/// @brief Wrap a component with a toast notification overlay.
///
/// The overlay renders notifications as a stack in the top-right corner.
ftxui::ComponentDecorator WithNotifications();
/// @brief Wrap @p parent with a toast notification overlay.
ftxui::Component WithNotifications(ftxui::Component parent);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_NOTIFICATION_HPP
