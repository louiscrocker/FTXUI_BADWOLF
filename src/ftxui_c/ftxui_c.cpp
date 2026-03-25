// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

// C language bindings for FTXUI.
// Each handle is a heap-allocated wrapper struct holding a shared_ptr to the
// underlying C++ object.  Callers are responsible for freeing every handle
// they receive via the corresponding _free() function.

#include "ftxui-c/ftxui.h"

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/globe.hpp"
#include "ftxui/ui/geomap.hpp"
#include "ftxui/ui/log_panel.hpp"
#include "ftxui/ui/markdown.hpp"
#include "ftxui/ui/reactive.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/ui/world_map_data.hpp"

/* ── Opaque handle definitions ───────────────────────────────────────────── */

struct ftxui_element_s {
  ftxui::Element element;
};

struct ftxui_component_s {
  ftxui::Component component;
};

struct ftxui_log_panel_s {
  std::shared_ptr<ftxui::ui::LogPanel> panel;
};

struct ftxui_reactive_int_s {
  std::shared_ptr<ftxui::ui::Reactive<int>> reactive;
};

struct ftxui_reactive_float_s {
  std::shared_ptr<ftxui::ui::Reactive<float>> reactive;
};

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static ftxui::Color to_color(ftxui_color_t c) {
  return ftxui::Color::RGB(c.r, c.g, c.b);
}

extern "C" {

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

void ftxui_element_free(ftxui_element_t el) {
  delete el;
}

void ftxui_component_free(ftxui_component_t comp) {
  delete comp;
}

/* ── DOM Elements ────────────────────────────────────────────────────────── */

ftxui_element_t ftxui_text(const char* str) {
  return new ftxui_element_s{ftxui::text(str ? str : "")};
}

ftxui_element_t ftxui_text_bold(const char* str) {
  return new ftxui_element_s{ftxui::text(str ? str : "") | ftxui::bold};
}

ftxui_element_t ftxui_text_colored(const char* str, ftxui_color_t color) {
  return new ftxui_element_s{ftxui::text(str ? str : "") |
                              ftxui::color(to_color(color))};
}

ftxui_element_t ftxui_separator(void) {
  return new ftxui_element_s{ftxui::separator()};
}

ftxui_element_t ftxui_hbox(ftxui_element_t* elements, size_t count) {
  ftxui::Elements elems;
  elems.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    if (elements[i]) {
      elems.push_back(elements[i]->element);
    }
  }
  return new ftxui_element_s{ftxui::hbox(std::move(elems))};
}

ftxui_element_t ftxui_vbox(ftxui_element_t* elements, size_t count) {
  ftxui::Elements elems;
  elems.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    if (elements[i]) {
      elems.push_back(elements[i]->element);
    }
  }
  return new ftxui_element_s{ftxui::vbox(std::move(elems))};
}

ftxui_element_t ftxui_border(ftxui_element_t inner) {
  if (!inner) {
    return nullptr;
  }
  return new ftxui_element_s{ftxui::border(inner->element)};
}

ftxui_element_t ftxui_flex(ftxui_element_t inner) {
  if (!inner) {
    return nullptr;
  }
  return new ftxui_element_s{ftxui::flex(inner->element)};
}

ftxui_element_t ftxui_hcenter(ftxui_element_t inner) {
  if (!inner) {
    return nullptr;
  }
  return new ftxui_element_s{ftxui::hcenter(inner->element)};
}

ftxui_element_t ftxui_vcenter(ftxui_element_t inner) {
  if (!inner) {
    return nullptr;
  }
  return new ftxui_element_s{ftxui::vcenter(inner->element)};
}

ftxui_element_t ftxui_center(ftxui_element_t inner) {
  if (!inner) {
    return nullptr;
  }
  return new ftxui_element_s{ftxui::center(inner->element)};
}

ftxui_element_t ftxui_spinner(int frame_index) {
  return new ftxui_element_s{ftxui::spinner(0, static_cast<size_t>(frame_index))};
}

ftxui_element_t ftxui_markdown(const char* markdown_text) {
  return new ftxui_element_s{
      ftxui::ui::Markdown(markdown_text ? markdown_text : "")};
}

/* ── Components ──────────────────────────────────────────────────────────── */

ftxui_component_t ftxui_button(const char* label,
                                void (*on_click)(void* userdata),
                                void* userdata) {
  std::string lbl = label ? label : "";
  auto cb = [on_click, userdata]() {
    if (on_click) {
      on_click(userdata);
    }
  };
  return new ftxui_component_s{ftxui::Button(lbl, cb)};
}

ftxui_component_t ftxui_renderer(ftxui_element_t (*render_fn)(void* userdata),
                                  void* userdata) {
  auto fn = [render_fn, userdata]() -> ftxui::Element {
    if (!render_fn) {
      return ftxui::text("");
    }
    ftxui_element_t handle = render_fn(userdata);
    if (!handle) {
      return ftxui::text("");
    }
    ftxui::Element el = handle->element;
    delete handle;
    return el;
  };
  return new ftxui_component_s{ftxui::Renderer(fn)};
}

ftxui_component_t ftxui_catch_event(ftxui_component_t inner,
                                     int (*handler)(const char* event_str,
                                                    void* userdata),
                                     void* userdata) {
  if (!inner) {
    return nullptr;
  }
  auto fn = [handler, userdata](ftxui::Event event) -> bool {
    if (!handler) {
      return false;
    }
    // Serialize the event to a C string.
    // For mouse events we use "mouse"; for all others we expose the raw input.
    if (event.is_mouse()) {
      return handler("mouse", userdata) != 0;
    }
    const std::string& raw = event.input();
    return handler(raw.c_str(), userdata) != 0;
  };
  return new ftxui_component_s{ftxui::CatchEvent(inner->component, fn)};
}

ftxui_component_t ftxui_container_vertical(ftxui_component_t* children,
                                            size_t count) {
  ftxui::Components comps;
  comps.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    if (children[i]) {
      comps.push_back(children[i]->component);
    }
  }
  return new ftxui_component_s{ftxui::Container::Vertical(std::move(comps))};
}

ftxui_component_t ftxui_container_horizontal(ftxui_component_t* children,
                                              size_t count) {
  ftxui::Components comps;
  comps.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    if (children[i]) {
      comps.push_back(children[i]->component);
    }
  }
  return new ftxui_component_s{ftxui::Container::Horizontal(std::move(comps))};
}

/* ── App ─────────────────────────────────────────────────────────────────── */

void ftxui_run_fullscreen(ftxui_component_t root) {
  if (!root) {
    return;
  }
  ftxui::ui::RunFullscreen(root->component);
}

void ftxui_exit(void) {
  if (ftxui::App* app = ftxui::App::Active()) {
    app->Exit();
  }
}

void ftxui_post_refresh(void) {
  if (ftxui::App* app = ftxui::App::Active()) {
    app->PostEvent(ftxui::Event::Custom);
  }
}

/* ── Themes ──────────────────────────────────────────────────────────────── */

void ftxui_set_theme(ftxui_theme_t theme) {
  switch (theme) {
    case FTXUI_THEME_DEFAULT:
      ftxui::ui::SetTheme(ftxui::ui::Theme::Default());
      break;
    case FTXUI_THEME_DARK:
      ftxui::ui::SetTheme(ftxui::ui::Theme::Dark());
      break;
    case FTXUI_THEME_LIGHT:
      ftxui::ui::SetTheme(ftxui::ui::Theme::Light());
      break;
    case FTXUI_THEME_NORD:
      ftxui::ui::SetTheme(ftxui::ui::Theme::Nord());
      break;
    case FTXUI_THEME_DRACULA:
      ftxui::ui::SetTheme(ftxui::ui::Theme::Dracula());
      break;
    case FTXUI_THEME_MONOKAI:
      ftxui::ui::SetTheme(ftxui::ui::Theme::Monokai());
      break;
    default:
      ftxui::ui::SetTheme(ftxui::ui::Theme::Default());
      break;
  }
}

/* ── LogPanel ────────────────────────────────────────────────────────────── */

ftxui_log_panel_t ftxui_log_panel_create(int max_lines) {
  size_t n = (max_lines > 0) ? static_cast<size_t>(max_lines) : 1000;
  return new ftxui_log_panel_s{ftxui::ui::LogPanel::Create(n)};
}

void ftxui_log_panel_info(ftxui_log_panel_t panel, const char* msg) {
  if (panel && panel->panel) {
    panel->panel->Info(msg ? msg : "");
  }
}

void ftxui_log_panel_warn(ftxui_log_panel_t panel, const char* msg) {
  if (panel && panel->panel) {
    panel->panel->Warn(msg ? msg : "");
  }
}

void ftxui_log_panel_error(ftxui_log_panel_t panel, const char* msg) {
  if (panel && panel->panel) {
    panel->panel->Error(msg ? msg : "");
  }
}

void ftxui_log_panel_debug(ftxui_log_panel_t panel, const char* msg) {
  if (panel && panel->panel) {
    panel->panel->Debug(msg ? msg : "");
  }
}

ftxui_component_t ftxui_log_panel_build(ftxui_log_panel_t panel,
                                         const char* title) {
  if (!panel || !panel->panel) {
    return nullptr;
  }
  std::string t = (title && title[0]) ? title : "";
  return new ftxui_component_s{panel->panel->Build(t)};
}

void ftxui_log_panel_free(ftxui_log_panel_t panel) {
  delete panel;
}

/* ── Reactive int ────────────────────────────────────────────────────────── */

ftxui_reactive_int_t ftxui_reactive_int_create(int initial) {
  return new ftxui_reactive_int_s{
      std::make_shared<ftxui::ui::Reactive<int>>(initial)};
}

void ftxui_reactive_int_set(ftxui_reactive_int_t r, int value) {
  if (r && r->reactive) {
    r->reactive->Set(value);
  }
}

int ftxui_reactive_int_get(ftxui_reactive_int_t r) {
  if (r && r->reactive) {
    return r->reactive->Get();
  }
  return 0;
}

void ftxui_reactive_int_on_change(ftxui_reactive_int_t r,
                                   void (*fn)(int value, void* userdata),
                                   void* userdata) {
  if (r && r->reactive && fn) {
    r->reactive->OnChange([fn, userdata](const int& val) {
      fn(val, userdata);
    });
  }
}

void ftxui_reactive_int_free(ftxui_reactive_int_t r) {
  delete r;
}

/* ── Reactive float ──────────────────────────────────────────────────────── */

ftxui_reactive_float_t ftxui_reactive_float_create(float initial) {
  return new ftxui_reactive_float_s{
      std::make_shared<ftxui::ui::Reactive<float>>(initial)};
}

void ftxui_reactive_float_set(ftxui_reactive_float_t r, float value) {
  if (r && r->reactive) {
    r->reactive->Set(value);
  }
}

float ftxui_reactive_float_get(ftxui_reactive_float_t r) {
  if (r && r->reactive) {
    return r->reactive->Get();
  }
  return 0.0f;
}

void ftxui_reactive_float_on_change(ftxui_reactive_float_t r,
                                     void (*fn)(float value, void* userdata),
                                     void* userdata) {
  if (r && r->reactive && fn) {
    r->reactive->OnChange([fn, userdata](const float& val) {
      fn(val, userdata);
    });
  }
}

void ftxui_reactive_float_free(ftxui_reactive_float_t r) {
  delete r;
}

/* ── World map / Globe ───────────────────────────────────────────────────── */

ftxui_component_t ftxui_world_map(void) {
  ftxui::Component comp =
      ftxui::ui::GeoMap().Data(ftxui::ui::WorldMapGeoJSON()).Build();
  return new ftxui_component_s{comp};
}

ftxui_component_t ftxui_globe(void) {
  return new ftxui_component_s{ftxui::ui::GlobeMap().Build()};
}

}  // extern "C"
