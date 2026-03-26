// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/plugin.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#define FTXUI_PLUGIN_SUPPORTED 1
#else
#define FTXUI_PLUGIN_SUPPORTED 0
#endif

using namespace ftxui;

namespace ftxui::ui {

// ── PluginHandle
// ──────────────────────────────────────────────────────────────

PluginHandle::~PluginHandle() {
#if FTXUI_PLUGIN_SUPPORTED
  if (handle_) {
    dlclose(handle_);
    handle_ = nullptr;
  }
#endif
}

bool PluginHandle::IsLoaded() const {
  return handle_ != nullptr || static_cast<bool>(in_process_factory_);
}

const PluginInfo& PluginHandle::Info() const {
  return info_;
}

std::string PluginHandle::path() const {
  return path_;
}

ftxui::Component PluginHandle::Create(const std::string& component_name,
                                      const std::string& params_json) {
  // In-process factory takes priority (no dlopen required).
  if (in_process_factory_) {
    return in_process_factory_(component_name.c_str(), params_json.c_str());
  }

#if FTXUI_PLUGIN_SUPPORTED
  if (!create_fn_) {
    return nullptr;
  }
  ftxui::Component* raw =
      create_fn_(component_name.c_str(), params_json.c_str());
  if (!raw) {
    return nullptr;
  }
  // Copy the shared_ptr out before deleting the heap-allocated wrapper.
  ftxui::Component result = *raw;
  if (destroy_fn_) {
    destroy_fn_(raw);
  }
  return result;
#else
  return nullptr;
#endif
}

void PluginHandle::Unload() {
#if FTXUI_PLUGIN_SUPPORTED
  if (handle_) {
    dlclose(handle_);
    handle_ = nullptr;
  }
#endif
  create_fn_ = nullptr;
  destroy_fn_ = nullptr;
  in_process_factory_ = nullptr;
}

// ── PluginRegistry
// ────────────────────────────────────────────────────────────

PluginRegistry& PluginRegistry::Instance() {
  static PluginRegistry instance;
  return instance;
}

std::shared_ptr<PluginHandle> PluginRegistry::Load(const std::string& path) {
#if FTXUI_PLUGIN_SUPPORTED
  dlerror();  // clear any prior error

  void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    const char* err = dlerror();
    last_error_ = err ? err : "dlopen failed (unknown error)";
    return nullptr;
  }

  dlerror();  // clear before dlsym calls
  auto* info_fn = reinterpret_cast<PluginHandle::InfoFn>(
      dlsym(handle, "badwolf_plugin_info"));
  if (!info_fn) {
    const char* err = dlerror();
    last_error_ = std::string("dlsym(badwolf_plugin_info): ") +
                  (err ? err : "unknown error");
    dlclose(handle);
    return nullptr;
  }

  auto* create_fn = reinterpret_cast<PluginHandle::CreateFn>(
      dlsym(handle, "badwolf_create_component"));
  if (!create_fn) {
    const char* err = dlerror();
    last_error_ = std::string("dlsym(badwolf_create_component): ") +
                  (err ? err : "unknown error");
    dlclose(handle);
    return nullptr;
  }

  auto* destroy_fn = reinterpret_cast<PluginHandle::DestroyFn>(
      dlsym(handle, "badwolf_destroy_component"));
  if (!destroy_fn) {
    const char* err = dlerror();
    last_error_ = std::string("dlsym(badwolf_destroy_component): ") +
                  (err ? err : "unknown error");
    dlclose(handle);
    return nullptr;
  }

  PluginInfo* info = info_fn();
  if (!info) {
    last_error_ = "badwolf_plugin_info() returned nullptr";
    dlclose(handle);
    return nullptr;
  }

  // Replace any existing plugin with the same name.
  auto existing = std::find_if(plugins_.begin(), plugins_.end(),
                               [&](const std::shared_ptr<PluginHandle>& h) {
                                 return h->info_.name == info->name;
                               });
  if (existing != plugins_.end()) {
    (*existing)->Unload();
    plugins_.erase(existing);
  }

  auto plugin = std::make_shared<PluginHandle>();
  plugin->handle_ = handle;
  plugin->info_ = *info;
  plugin->path_ = path;
  plugin->create_fn_ = create_fn;
  plugin->destroy_fn_ = destroy_fn;

  plugins_.push_back(plugin);
  return plugin;
#else
  last_error_ = "plugins not supported on this platform";
  return nullptr;
#endif
}

int PluginRegistry::LoadDirectory(const std::string& dir) {
  namespace fs = std::filesystem;

  std::error_code ec;
  if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
    return 0;
  }

  int count = 0;
  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (ec) {
      break;
    }
    const auto& p = entry.path();
    const std::string ext = p.extension().string();

#if defined(__APPLE__)
    const bool is_plugin = (ext == ".dylib");
#elif defined(__unix__)
    const bool is_plugin = (ext == ".so");
#else
    const bool is_plugin = false;
#endif

    if (is_plugin) {
      if (Load(p.string())) {
        ++count;
      }
    }
  }
  return count;
}

bool PluginRegistry::Unload(const std::string& plugin_name) {
  auto it = std::find_if(plugins_.begin(), plugins_.end(),
                         [&](const std::shared_ptr<PluginHandle>& h) {
                           return h->info_.name == plugin_name;
                         });
  if (it == plugins_.end()) {
    return false;
  }
  (*it)->Unload();
  plugins_.erase(it);
  return true;
}

std::vector<std::shared_ptr<PluginHandle>> PluginRegistry::Plugins() const {
  return plugins_;
}

std::shared_ptr<PluginHandle> PluginRegistry::FindProvider(
    const std::string& component_name) const {
  for (const auto& handle : plugins_) {
    for (const auto& provides : handle->info_.provides) {
      if (provides == component_name) {
        return handle;
      }
    }
  }
  return nullptr;
}

ftxui::Component PluginRegistry::Create(const std::string& component_name,
                                        const std::string& params_json) {
  auto handle = FindProvider(component_name);
  if (!handle) {
    last_error_ = "No plugin provides component: " + component_name;
    return nullptr;
  }
  return handle->Create(component_name, params_json);
}

std::string PluginRegistry::last_error() const {
  return last_error_;
}

void PluginRegistry::AddSearchPath(const std::string& path) {
  search_paths_.push_back(path);
}

std::vector<std::string> PluginRegistry::SearchPaths() const {
  return search_paths_;
}

bool PluginRegistry::Reload(const std::string& plugin_name) {
  auto it = std::find_if(plugins_.begin(), plugins_.end(),
                         [&](const std::shared_ptr<PluginHandle>& h) {
                           return h->info_.name == plugin_name;
                         });
  if (it == plugins_.end()) {
    return false;
  }
  std::string saved_path = (*it)->path_;
  if (saved_path.empty() || saved_path == "[in-process]") {
    last_error_ = "Cannot reload in-process plugin: " + plugin_name;
    return false;
  }
  Unload(plugin_name);
  return Load(saved_path) != nullptr;
}

// ── PluginManager
// ─────────────────────────────────────────────────────────────

ftxui::Component PluginManager() {
  struct ManagerState {
    std::string load_path_input;
    std::string status_msg;
    int selected_row{0};
  };

  auto state = std::make_shared<ManagerState>();
  state->status_msg = "Plugin Manager — no action taken";

  auto load_input =
      ftxui::Input(&state->load_path_input, "Path to .so/.dylib...");

  auto load_btn = ftxui::Button("Load", [state] {
    if (state->load_path_input.empty()) {
      state->status_msg = "Error: path is empty";
      return;
    }
    auto handle = PluginRegistry::Instance().Load(state->load_path_input);
    if (handle) {
      state->status_msg = "Loaded: " + handle->Info().name;
      state->load_path_input.clear();
    } else {
      state->status_msg =
          "Load failed: " + PluginRegistry::Instance().last_error();
    }
  });

  auto controls = ftxui::Container::Horizontal({load_input, load_btn});

  auto body = ftxui::Renderer(controls, [state, controls] {
    auto& reg = PluginRegistry::Instance();
    auto plugins = reg.Plugins();

    // Table header
    std::vector<ftxui::Element> rows;
    rows.push_back(ftxui::hbox({
        ftxui::text(" Name") | ftxui::bold |
            ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 14),
        ftxui::separator(),
        ftxui::text(" Ver") | ftxui::bold |
            ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8),
        ftxui::separator(),
        ftxui::text(" Provides") | ftxui::bold |
            ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 22),
        ftxui::separator(),
        ftxui::text(" Path") | ftxui::bold | ftxui::flex,
    }));
    rows.push_back(ftxui::separator());

    if (plugins.empty()) {
      rows.push_back(ftxui::text(" (no plugins loaded)") |
                     ftxui::color(ftxui::Color::GrayDark));
    } else {
      for (const auto& p : plugins) {
        std::string provides_str;
        for (size_t i = 0; i < p->Info().provides.size(); ++i) {
          if (i > 0) {
            provides_str += ", ";
          }
          provides_str += p->Info().provides[i];
        }
        rows.push_back(ftxui::hbox({
            ftxui::text(" " + p->Info().name) |
                ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 14),
            ftxui::separator(),
            ftxui::text(" " + p->Info().version) |
                ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8),
            ftxui::separator(),
            ftxui::text(" " + provides_str) |
                ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 22),
            ftxui::separator(),
            ftxui::text(" " + p->path()) | ftxui::flex,
        }));
      }
    }
    rows.push_back(ftxui::separator());

    int n = static_cast<int>(plugins.size());
    std::string count_str =
        " " + std::to_string(n) + " plugin" + (n != 1 ? "s" : "") + " loaded";
    rows.push_back(ftxui::text(count_str) |
                   ftxui::color(ftxui::Color::GreenLight));

    return ftxui::vbox({
        ftxui::vbox(rows) | ftxui::border,
        ftxui::hbox({
            ftxui::text("Load: "),
            controls->Render() | ftxui::flex,
        }) | ftxui::border,
        ftxui::text(" " + state->status_msg) |
            ftxui::color(ftxui::Color::YellowLight),
    });
  });

  return body;
}

// ── InProcessPlugin
// ───────────────────────────────────────────────────────────

InProcessPlugin::InProcessPlugin(
    PluginInfo info,
    std::function<ftxui::Component(const std::string&, const std::string&)>
        factory)
    : info_(std::move(info)), factory_(std::move(factory)) {}

void InProcessPlugin::Register() {
  auto handle = std::make_shared<PluginHandle>();
  handle->info_ = info_;
  handle->path_ = "[in-process]";

  // Capture factory_ by copy so the lambda outlives *this if needed.
  auto factory_copy = factory_;
  handle->in_process_factory_ =
      [factory_copy](const char* name, const char* params) -> ftxui::Component {
    return factory_copy(std::string(name), std::string(params));
  };

  // Replace any existing entry with this name.
  auto& reg = PluginRegistry::Instance();
  auto it = std::find_if(reg.plugins_.begin(), reg.plugins_.end(),
                         [&](const std::shared_ptr<PluginHandle>& h) {
                           return h->info_.name == info_.name;
                         });
  if (it != reg.plugins_.end()) {
    (*it)->Unload();
    reg.plugins_.erase(it);
  }

  reg.plugins_.push_back(handle);
}

void InProcessPlugin::Unregister() {
  PluginRegistry::Instance().Unload(info_.name);
}

}  // namespace ftxui::ui
