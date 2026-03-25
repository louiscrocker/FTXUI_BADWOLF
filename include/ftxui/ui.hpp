// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_HPP
#define FTXUI_UI_HPP

/// @defgroup ui FTXUI Higher-Level UI Utilities
///
/// The `ftxui::ui` namespace provides a higher-level, more ergonomic API built
/// on top of the core `ftxui` library.  It is inspired by Go terminal UI
/// libraries (Bubbletea, tview) and aims to drastically reduce the amount of
/// boilerplate needed for common patterns.
///
/// ## Quick start
/// @code
/// #include "ftxui/ui.hpp"
/// using namespace ftxui;
/// using namespace ftxui::ui;
///
/// int main() {
///   std::string name, email;
///
///   auto form = Form()
///       .Title("Welcome")
///       .Field("Name",  &name)
///       .Field("Email", &email)
///       .Submit("OK", [&]{ App::Active()->Exit(); });
///
///   Run(form.Build());
/// }
/// @endcode
///
/// ## Sub-modules
///  - **app.hpp**          – one-liner `Run / RunFullscreen / RunFitComponent`
///  - **datatable.hpp**    – `DataTable<T>` interactive sortable/filterable
///  table
///  - **dialog.hpp**       – `WithConfirm / WithAlert / WithHelp` overlays
///  - **form.hpp**         – fluent `Form` builder
///  - **keymap.hpp**       – `Keymap` keybinding registry + help renderer
///  - **layout.hpp**       – `Panel / Row / Column / StatusBar / HSplit /
///  TabView`
///  - **list.hpp**         – `List<T>` interactive item list with optional
///  filter
///  - **log_panel.hpp**    – `LogPanel` thread-safe scrolling log panel
///  - **mvu.hpp**          – `MVU<TModel,TMsg>` Elm/Bubbletea-style
///  architecture
///  - **notification.hpp** – `Notify / WithNotifications` toast overlay system
///  - **theme.hpp**        – `Theme` system with `SetTheme / GetTheme`
///  - **wizard.hpp**         – `Wizard` multi-step form builder
///  - **command_palette.hpp** – `CommandPalette` searchable command overlay
///  - **router.hpp**          – `Router` history-based multi-screen navigation
///  - **tree.hpp**            – `Tree` collapsible tree-view component

#include "ftxui/ui/alert.hpp"
#include "ftxui/ui/animation.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/background_task.hpp"
#include "ftxui/ui/bind.hpp"
#include "ftxui/ui/canvas.hpp"
#include "ftxui/ui/charts.hpp"
#include "ftxui/ui/collab.hpp"
#include "ftxui/ui/command_palette.hpp"
#include "ftxui/ui/config_editor.hpp"
#include "ftxui/ui/data_context.hpp"
#include "ftxui/ui/datatable.hpp"
#include "ftxui/ui/debug_overlay.hpp"
#include "ftxui/ui/dialog.hpp"
#include "ftxui/ui/event_recorder.hpp"
#include "ftxui/ui/filepicker.hpp"
#include "ftxui/ui/form.hpp"
#include "ftxui/ui/galaxy_map.hpp"
#include "ftxui/ui/geojson.hpp"
#include "ftxui/ui/geomap.hpp"
#include "ftxui/ui/grid.hpp"
#include "ftxui/ui/inspector.hpp"
#include "ftxui/ui/keymap.hpp"
#include "ftxui/ui/layout.hpp"
#include "ftxui/ui/lcars.hpp"
#include "ftxui/ui/list.hpp"
#include "ftxui/ui/live_source.hpp"
#include "ftxui/ui/log_panel.hpp"
#include "ftxui/ui/markdown.hpp"
#include "ftxui/ui/model_binding.hpp"
#include "ftxui/ui/mvu.hpp"
#include "ftxui/ui/network_reactive.hpp"
#include "ftxui/ui/notification.hpp"
#include "ftxui/ui/particles.hpp"
#include "ftxui/ui/progress.hpp"
#include "ftxui/ui/reactive_decorators.hpp"
#include "ftxui/ui/reactive_list.hpp"
#include "ftxui/ui/router.hpp"
#include "ftxui/ui/simple_table.hpp"
#include "ftxui/ui/sortable_table.hpp"
#include "ftxui/ui/state.hpp"
#include "ftxui/ui/textinput.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/ui/transitions.hpp"
#include "ftxui/ui/tree.hpp"
#include "ftxui/ui/ui_builder.hpp"
#include "ftxui/ui/virtual_list.hpp"
#include "ftxui/ui/widgets.hpp"
#include "ftxui/ui/wizard.hpp"
#include "ftxui/ui/world_map_data.hpp"
#include "ftxui/ui/llm_bridge.hpp"

#endif  // FTXUI_UI_HPP
