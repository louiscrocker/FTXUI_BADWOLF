// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
//
// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  FTXUI BadWolf — High-Level Terminal UI API                              ║
// ║  Single-include header. Include this file to access all ftxui::ui        ║
// ║  components. See https://louiscrocker.github.io/FTXUI_BADWOLF/          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#ifndef FTXUI_UI_HPP
#define FTXUI_UI_HPP

// ── Core Infrastructure ──────────────────────────────────────────────────────
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/ui/state.hpp"
#include "ftxui/ui/mvu.hpp"

// ── Reactive State & Data Binding ────────────────────────────────────────────
#include "ftxui/ui/reactive.hpp"
#include "ftxui/ui/reactive_list.hpp"
#include "ftxui/ui/reactive_decorators.hpp"
#include "ftxui/ui/bind.hpp"
#include "ftxui/ui/model_binding.hpp"
#include "ftxui/ui/data_context.hpp"

// ── Data Types ───────────────────────────────────────────────────────────────
#include "ftxui/ui/json.hpp"

// ── Layout & Structure ───────────────────────────────────────────────────────
#include "ftxui/ui/layout.hpp"
#include "ftxui/ui/grid.hpp"
#include "ftxui/ui/router.hpp"
#include "ftxui/ui/transitions.hpp"

// ── Text & Richness ───────────────────────────────────────────────────────────
#include "ftxui/ui/rich_text.hpp"
#include "ftxui/ui/typewriter.hpp"
#include "ftxui/ui/markdown.hpp"

// ── Input Widgets ────────────────────────────────────────────────────────────
#include "ftxui/ui/textinput.hpp"
#include "ftxui/ui/form.hpp"
#include "ftxui/ui/keymap.hpp"
#include "ftxui/ui/filepicker.hpp"
#include "ftxui/ui/config_editor.hpp"

// ── Data Display ─────────────────────────────────────────────────────────────
#include "ftxui/ui/datatable.hpp"
#include "ftxui/ui/simple_table.hpp"
#include "ftxui/ui/sortable_table.hpp"
#include "ftxui/ui/virtual_list.hpp"
#include "ftxui/ui/list.hpp"
#include "ftxui/ui/tree.hpp"

// ── Charts & Visualization ────────────────────────────────────────────────────
#include "ftxui/ui/charts.hpp"
#include "ftxui/ui/canvas.hpp"

// ── Geospatial & Maps ─────────────────────────────────────────────────────────
#include "ftxui/ui/geojson.hpp"
#include "ftxui/ui/geomap.hpp"
#include "ftxui/ui/globe.hpp"
#include "ftxui/ui/galaxy_map.hpp"
#include "ftxui/ui/world_map_data.hpp"

// ── Animation & Effects ───────────────────────────────────────────────────────
#include "ftxui/ui/animation.hpp"
#include "ftxui/ui/particles.hpp"

// ── Overlays & Dialogs ────────────────────────────────────────────────────────
#include "ftxui/ui/dialog.hpp"
#include "ftxui/ui/notification.hpp"
#include "ftxui/ui/progress.hpp"
#include "ftxui/ui/widgets.hpp"
#include "ftxui/ui/wizard.hpp"
#include "ftxui/ui/alert.hpp"

// ── Navigation & Commands ─────────────────────────────────────────────────────
#include "ftxui/ui/command_palette.hpp"
#include "ftxui/ui/log_panel.hpp"

// ── Theming & Branding ────────────────────────────────────────────────────────
#include "ftxui/ui/lcars.hpp"

// ── Networking & Collaboration ────────────────────────────────────────────────
#include "ftxui/ui/network_reactive.hpp"
#include "ftxui/ui/collab.hpp"
#include "ftxui/ui/live_source.hpp"

// ── AI & Intelligence ────────────────────────────────────────────────────────
#include "ftxui/ui/llm_bridge.hpp"
#include "ftxui/ui/registry.hpp"

// ── System & Process ─────────────────────────────────────────────────────────
#include "ftxui/ui/process_panel.hpp"
#include "ftxui/ui/background_task.hpp"

// ── Developer Tools ───────────────────────────────────────────────────────────
#include "ftxui/ui/debug_overlay.hpp"
#include "ftxui/ui/inspector.hpp"
#include "ftxui/ui/event_recorder.hpp"
#include "ftxui/ui/ui_builder.hpp"

// ── WebAssembly & Platform ────────────────────────────────────────────────────
#include "ftxui/ui/wasm_bridge.hpp"
#include "ftxui/ui/webgl_renderer.hpp"

#endif  // FTXUI_UI_HPP
