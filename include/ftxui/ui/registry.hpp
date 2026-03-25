// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_REGISTRY_HPP
#define FTXUI_UI_REGISTRY_HPP

#include <functional>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

/// @brief Metadata describing a registered UI component.
/// @ingroup ui
struct ComponentMeta {
  std::string name;         ///< Unique component identifier (e.g. "my-button")
  std::string version;      ///< Semantic version string (e.g. "1.0.0")
  std::string description;  ///< Short human-readable description
  std::string author;       ///< Author / publisher name
  std::vector<std::string> tags;  ///< Searchable tags
};

/// @brief Singleton registry that maps component names to factory functions.
///
/// Components register themselves at static-init time via `ComponentRegistrar`.
/// Callers can enumerate, search, and instantiate registered components.
///
/// @ingroup ui
class Registry {
 public:
  /// @brief Returns the process-wide singleton instance.
  static Registry& Get();

  /// @brief Registers a component with the given metadata and factory.
  void Register(ComponentMeta meta,
                std::function<ftxui::Component()> factory);

  /// @brief Returns metadata for all registered components.
  std::vector<ComponentMeta> List() const;

  /// @brief Returns components whose name, description, or tags contain
  ///        @p query (case-insensitive substring match).
  std::vector<ComponentMeta> Search(const std::string& query) const;

  /// @brief Instantiates a component by name.
  /// @returns nullptr if the name is not registered.
  ftxui::Component Create(const std::string& name) const;

 private:
  Registry() = default;

  struct Entry {
    ComponentMeta meta;
    std::function<ftxui::Component()> factory;
  };

  std::vector<Entry> entries_;
};

/// @brief RAII helper that registers a component at static-initialisation time.
///
/// Declare one static instance per component translation unit.
///
/// @ingroup ui
struct ComponentRegistrar {
  ComponentRegistrar(ComponentMeta meta,
                     std::function<ftxui::Component()> factory);
};

}  // namespace ftxui::ui

/// @brief Convenience macro for self-registering components.
///
/// Place this at file scope in a .cpp (or header-only) file:
/// @code
/// BADWOLF_REGISTER_COMPONENT(my_button, "1.0.0",
///     "A stylish button", []{ return ftxui::Button("Click me", []{}); });
/// @endcode
#define BADWOLF_REGISTER_COMPONENT(name, version, desc, factory)     \
  static ::ftxui::ui::ComponentRegistrar _badwolf_reg_##name(        \
      ::ftxui::ui::ComponentMeta{#name, version, desc, "local", {}}, \
      factory)

#endif  // FTXUI_UI_REGISTRY_HPP
