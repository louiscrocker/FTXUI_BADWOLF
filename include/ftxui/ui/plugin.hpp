// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_PLUGIN_HPP
#define FTXUI_UI_PLUGIN_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"

namespace ftxui::ui {

// ── Plugin metadata
// ───────────────────────────────────────────────────────────
struct PluginInfo {
  std::string name;
  std::string version;
  std::string description;
  std::string author;
  std::vector<std::string> provides;  ///< component names this plugin exports
};

// ── Plugin API — what a plugin .so/.dylib must export
// ───────────────────────── Each plugin shared library must export these C
// functions:
//
//   extern "C" PluginInfo* badwolf_plugin_info();
//   extern "C" ftxui::Component* badwolf_create_component(const char* name,
//                                                          const char*
//                                                          params_json);
//   extern "C" void badwolf_destroy_component(ftxui::Component* c);
//   extern "C" const char* badwolf_plugin_version();  // returns semver string

// ── BADWOLF_PLUGIN_DEFINE macro
// ─────────────────────────────────────────────── Use in plugin implementation
// files to declare plugin metadata and version.
//   BADWOLF_PLUGIN_DEFINE("myplugin", "1.0.0", "My desc", "Author",
//                          (std::vector<std::string>{"WidgetA", "WidgetB"}))
//   BADWOLF_PLUGIN_FACTORY { ... }
//   BADWOLF_PLUGIN_DESTROY { delete c; }
#define BADWOLF_PLUGIN_DEFINE(pname, pver, pdesc, pauthor, pcomponents)     \
  static ftxui::ui::PluginInfo _badwolf_plugin_info_{pname, pver, pdesc,    \
                                                     pauthor, pcomponents}; \
  extern "C" ftxui::ui::PluginInfo* badwolf_plugin_info() {                 \
    return &_badwolf_plugin_info_;                                          \
  }                                                                         \
  extern "C" const char* badwolf_plugin_version() {                         \
    return pver;                                                            \
  }

#define BADWOLF_PLUGIN_FACTORY                           \
  extern "C" ftxui::Component* badwolf_create_component( \
      const char* name, const char* params_json)

#define BADWOLF_PLUGIN_DESTROY \
  extern "C" void badwolf_destroy_component(ftxui::Component* c)

// ── PluginHandle
// ──────────────────────────────────────────────────────────────
/// @brief Handle to a loaded plugin. Obtained from PluginRegistry::Load().
/// @ingroup ui
class PluginHandle {
 public:
  ~PluginHandle();

  /// @brief Returns true if the plugin is currently loaded.
  bool IsLoaded() const;

  /// @brief Returns plugin metadata.
  const PluginInfo& Info() const;

  /// @brief Returns the filesystem path this plugin was loaded from.
  ///        Returns "[in-process]" for InProcessPlugin instances.
  std::string path() const;

  /// @brief Creates a component exported by this plugin.
  /// @param component_name  Name of the component (must be in Info().provides).
  /// @param params_json     JSON configuration string passed to the factory.
  /// @returns nullptr if the plugin does not recognise component_name.
  ftxui::Component Create(const std::string& component_name,
                          const std::string& params_json = "{}");

  /// @brief Unloads the plugin (all created components must be released first).
  void Unload();

 private:
  friend class PluginRegistry;
  friend class InProcessPlugin;

  void* handle_{nullptr};
  PluginInfo info_;
  std::string path_;

  using CreateFn = ftxui::Component* (*)(const char*, const char*);
  using DestroyFn = void (*)(ftxui::Component*);
  using InfoFn = PluginInfo* (*)();

  CreateFn create_fn_{nullptr};
  DestroyFn destroy_fn_{nullptr};

  // Non-null for InProcessPlugin (no dlopen required).
  std::function<ftxui::Component(const char*, const char*)> in_process_factory_;
};

// ── PluginRegistry
// ────────────────────────────────────────────────────────────
/// @brief Singleton registry of dynamically-loaded FTXUI plugins.
/// @ingroup ui
class PluginRegistry {
 public:
  /// @brief Returns the process-wide singleton instance.
  static PluginRegistry& Instance();

  /// @brief Loads a plugin from a shared library at @p path.
  /// @returns A shared_ptr to the PluginHandle, or nullptr on failure.
  ///          Inspect last_error() for details on failure.
  std::shared_ptr<PluginHandle> Load(const std::string& path);

  /// @brief Scans @p dir for *.so (Linux) / *.dylib (macOS) and loads each.
  /// @returns Count of successfully loaded plugins.
  int LoadDirectory(const std::string& dir);

  /// @brief Unloads the plugin with the given name.
  /// @returns true if the plugin was found and unloaded.
  bool Unload(const std::string& plugin_name);

  /// @brief Lists all currently loaded plugins.
  std::vector<std::shared_ptr<PluginHandle>> Plugins() const;

  /// @brief Finds the first plugin that lists @p component_name in its
  /// provides.
  /// @returns nullptr if no loaded plugin provides the component.
  std::shared_ptr<PluginHandle> FindProvider(
      const std::string& component_name) const;

  /// @brief Creates a component by searching all loaded plugins.
  /// @returns nullptr if no loaded plugin provides component_name.
  ftxui::Component Create(const std::string& component_name,
                          const std::string& params_json = "{}");

  /// @brief Returns the last error message from Load() / Create().
  std::string last_error() const;

  /// @brief Adds a directory to the plugin search path list.
  void AddSearchPath(const std::string& path);

  /// @brief Returns all registered search paths.
  std::vector<std::string> SearchPaths() const;

  /// @brief Hot-reloads a plugin: unloads it then reloads from its original
  /// path.
  /// @returns true on success.  Returns false for in-process plugins.
  bool Reload(const std::string& plugin_name);

 private:
  friend class InProcessPlugin;

  PluginRegistry() = default;

  std::vector<std::shared_ptr<PluginHandle>> plugins_;
  std::vector<std::string> search_paths_;
  mutable std::string last_error_;
};

// ── PluginManager Component
// ───────────────────────────────────────────────────
/// @brief Returns an interactive component that lists loaded plugins and
///        provides Unload / Reload buttons plus a "Load Plugin" entry.
/// @ingroup ui
ftxui::Component PluginManager();

// ── InProcessPlugin
// ───────────────────────────────────────────────────────────
/// @brief Registers a component factory directly without loading a .so/.dylib.
///
/// Useful for testing and prototyping — no shared library compilation needed.
///
/// @code
/// ftxui::ui::InProcessPlugin my_plugin(
///   {"demo", "1.0.0", "Demo plugin", "Dev", {"MyWidget"}},
///   [](const std::string& name, const std::string& /*params*/) {
///     return ftxui::Renderer([]{ return ftxui::text("hello"); });
///   });
/// my_plugin.Register();
/// // ...
/// my_plugin.Unregister();
/// @endcode
///
/// @ingroup ui
class InProcessPlugin {
 public:
  InProcessPlugin(
      PluginInfo info,
      std::function<ftxui::Component(const std::string& name,
                                     const std::string& params)> factory);

  /// @brief Adds this plugin to the PluginRegistry singleton.
  void Register();

  /// @brief Removes this plugin from the PluginRegistry singleton.
  void Unregister();

 private:
  PluginInfo info_;
  std::function<ftxui::Component(const std::string&, const std::string&)>
      factory_;
};

}  // namespace ftxui::ui

#endif  // FTXUI_UI_PLUGIN_HPP
