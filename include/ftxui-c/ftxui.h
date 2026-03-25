// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* ── Opaque handle types ─────────────────────────────────────────────────── */
typedef struct ftxui_element_s* ftxui_element_t;
typedef struct ftxui_component_s* ftxui_component_t;
typedef struct ftxui_log_panel_s* ftxui_log_panel_t;
typedef struct ftxui_reactive_int_s* ftxui_reactive_int_t;
typedef struct ftxui_reactive_float_s* ftxui_reactive_float_t;

/* ── Colors ──────────────────────────────────────────────────────────────── */
typedef struct {
  uint8_t r, g, b;
} ftxui_color_t;
#define FTXUI_COLOR_DEFAULT ((ftxui_color_t){0, 0, 0})
#define FTXUI_COLOR_RED ((ftxui_color_t){255, 0, 0})
#define FTXUI_COLOR_GREEN ((ftxui_color_t){0, 255, 0})
#define FTXUI_COLOR_BLUE ((ftxui_color_t){0, 0, 255})
#define FTXUI_COLOR_WHITE ((ftxui_color_t){255, 255, 255})
#define FTXUI_COLOR_YELLOW ((ftxui_color_t){255, 255, 0})
#define FTXUI_COLOR_CYAN ((ftxui_color_t){0, 255, 255})

/* ── Lifecycle ───────────────────────────────────────────────────────────── */
/* All handles must be freed by the caller. Handles are heap-allocated C++   */
/* wrapper objects that hold a shared_ptr to the underlying FTXUI object.    */
void ftxui_element_free(ftxui_element_t el);
void ftxui_component_free(ftxui_component_t comp);

/* ── DOM Elements ────────────────────────────────────────────────────────── */
/* All DOM-element factory functions return a new handle the caller owns.    */
ftxui_element_t ftxui_text(const char* str);
ftxui_element_t ftxui_text_bold(const char* str);
ftxui_element_t ftxui_text_colored(const char* str, ftxui_color_t color);
ftxui_element_t ftxui_separator(void);

/* Container functions copy (ref-count) each child element; the caller        */
/* still owns the original handles and must free them separately.             */
ftxui_element_t ftxui_hbox(ftxui_element_t* elements, size_t count);
ftxui_element_t ftxui_vbox(ftxui_element_t* elements, size_t count);

/* Decorator functions copy the inner element; the caller must free `inner`. */
ftxui_element_t ftxui_border(ftxui_element_t inner);
ftxui_element_t ftxui_flex(ftxui_element_t inner);
ftxui_element_t ftxui_hcenter(ftxui_element_t inner);
ftxui_element_t ftxui_vcenter(ftxui_element_t inner);
ftxui_element_t ftxui_center(ftxui_element_t inner);

/* spinner: charset_index selects the animation set; frame_index is the frame */
ftxui_element_t ftxui_spinner(int frame_index);

/* Render markdown text as a stacked FTXUI element tree. */
ftxui_element_t ftxui_markdown(const char* markdown_text);

/* ── Components ──────────────────────────────────────────────────────────── */
/* on_click / render_fn / handler are called on the UI thread.               */
ftxui_component_t ftxui_button(const char* label,
                                void (*on_click)(void* userdata),
                                void* userdata);

/* render_fn is called every frame; it must return a freshly-created element. */
/* The returned element is owned by the component and freed automatically.    */
ftxui_component_t ftxui_renderer(ftxui_element_t (*render_fn)(void* userdata),
                                  void* userdata);

/* handler returns non-zero if the event was consumed.                        */
/* event_str is a NUL-terminated UTF-8 string representing the event input;  */
/* for mouse events it is the literal string "mouse".                         */
ftxui_component_t ftxui_catch_event(ftxui_component_t inner,
                                     int (*handler)(const char* event_str,
                                                    void* userdata),
                                     void* userdata);

/* Container functions copy (ref-count) each child; the caller frees originals. */
ftxui_component_t ftxui_container_vertical(ftxui_component_t* children,
                                            size_t count);
ftxui_component_t ftxui_container_horizontal(ftxui_component_t* children,
                                              size_t count);

/* ── App ─────────────────────────────────────────────────────────────────── */
/* Run a component fullscreen (alternate screen buffer) until the app exits.  */
/* Blocks the calling thread until done. Not thread-safe.                     */
void ftxui_run_fullscreen(ftxui_component_t root);

/* Exit the currently running app.  Safe to call from a UI callback.         */
void ftxui_exit(void);

/* Post a UI refresh event.  Thread-safe; safe to call from any thread.      */
void ftxui_post_refresh(void);

/* ── Themes ──────────────────────────────────────────────────────────────── */
typedef enum {
  FTXUI_THEME_DEFAULT = 0,
  FTXUI_THEME_DARK,
  FTXUI_THEME_LIGHT,
  FTXUI_THEME_NORD,
  FTXUI_THEME_DRACULA,
  FTXUI_THEME_MONOKAI,
} ftxui_theme_t;

/* Apply a built-in theme globally.  Must be called before building           */
/* components.  Not thread-safe.                                              */
void ftxui_set_theme(ftxui_theme_t theme);

/* ── LogPanel ────────────────────────────────────────────────────────────── */
/* LogPanel is thread-safe: all log functions may be called from any thread.  */
ftxui_log_panel_t ftxui_log_panel_create(int max_lines);
void ftxui_log_panel_info(ftxui_log_panel_t panel, const char* msg);
void ftxui_log_panel_warn(ftxui_log_panel_t panel, const char* msg);
void ftxui_log_panel_error(ftxui_log_panel_t panel, const char* msg);
void ftxui_log_panel_debug(ftxui_log_panel_t panel, const char* msg);

/* Build a renderable component from the panel.  Returns a new handle.        */
/* `title` may be NULL or empty for a panel without a border title.           */
ftxui_component_t ftxui_log_panel_build(ftxui_log_panel_t panel,
                                         const char* title);
void ftxui_log_panel_free(ftxui_log_panel_t panel);

/* ── Reactive int ────────────────────────────────────────────────────────── */
/* Reactive<int> is thread-safe for Set/Get/on_change callbacks.              */
ftxui_reactive_int_t ftxui_reactive_int_create(int initial);
void ftxui_reactive_int_set(ftxui_reactive_int_t r, int value);
int ftxui_reactive_int_get(ftxui_reactive_int_t r);

/* fn is called on any thread that calls Set/Update.  userdata is forwarded.  */
void ftxui_reactive_int_on_change(ftxui_reactive_int_t r,
                                   void (*fn)(int value, void* userdata),
                                   void* userdata);
void ftxui_reactive_int_free(ftxui_reactive_int_t r);

/* ── Reactive float ──────────────────────────────────────────────────────── */
ftxui_reactive_float_t ftxui_reactive_float_create(float initial);
void ftxui_reactive_float_set(ftxui_reactive_float_t r, float value);
float ftxui_reactive_float_get(ftxui_reactive_float_t r);
void ftxui_reactive_float_on_change(ftxui_reactive_float_t r,
                                     void (*fn)(float value, void* userdata),
                                     void* userdata);
void ftxui_reactive_float_free(ftxui_reactive_float_t r);

/* ── World map / Globe ───────────────────────────────────────────────────── */
/* Returns a component rendering the Natural Earth 110m embedded world map.   */
ftxui_component_t ftxui_world_map(void);

/* Returns a component rendering an animated spinning 3-D globe.              */
ftxui_component_t ftxui_globe(void);

#ifdef __cplusplus
}
#endif
