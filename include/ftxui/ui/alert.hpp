// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_ALERT_HPP
#define FTXUI_UI_ALERT_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Tiered alert severity levels.
/// @ingroup ui
enum class AlertLevel {
  AllClear,  ///< Normal operations.
  Blue,      ///< Informational.
  Yellow,    ///< Caution.
  Red,       ///< Emergency.
};

/// @brief Singleton alert system that drives a reactive alert overlay.
///
/// Use the free functions RedAlert(), YellowAlert(), and AllClear() for
/// convenience, or call WithAlertOverlay() to wrap your root component.
///
/// @ingroup ui
class AlertSystem {
 public:
  /// @brief Return the process-wide AlertSystem instance.
  static AlertSystem& Instance();

  /// @brief Trigger a Red Alert.
  void Red(std::string message = "RED ALERT");

  /// @brief Trigger a Yellow Alert.
  void Yellow(std::string message = "YELLOW ALERT");

  /// @brief Trigger a Blue informational alert.
  void Blue(std::string message);

  /// @brief Return to normal operations.
  void AllClear();

  /// @brief Current alert level.
  AlertLevel Level() const;

  /// @brief Current alert message.
  std::string Message() const;

  /// @brief Subscribe to alert level changes.
  void OnChange(std::function<void(AlertLevel, std::string)> fn);

  /// @brief Wrap a component with alert-reactive chrome.
  ///
  /// - Red Alert:    border flashes red, message shown in header.
  /// - Yellow Alert: border turns amber.
  /// - AllClear:     normal rendering.
  ftxui::Component WithAlertOverlay(ftxui::Component inner);

 private:
  AlertSystem();

  mutable std::mutex mutex_;
  AlertLevel level_ = AlertLevel::AllClear;
  std::string message_;
  std::vector<std::function<void(AlertLevel, std::string)>> listeners_;
};

// ── Convenience free functions
// ────────────────────────────────────────────────

inline void RedAlert(std::string msg = "RED ALERT") {
  AlertSystem::Instance().Red(std::move(msg));
}

inline void YellowAlert(std::string msg = "YELLOW ALERT") {
  AlertSystem::Instance().Yellow(std::move(msg));
}

inline void BlueAlert(std::string msg) {
  AlertSystem::Instance().Blue(std::move(msg));
}

inline void AllClear() {
  AlertSystem::Instance().AllClear();
}

/// @brief Global convenience: wrap your root component with the alert overlay.
/// @ingroup ui
ftxui::Component WithAlertOverlay(ftxui::Component inner);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_ALERT_HPP
