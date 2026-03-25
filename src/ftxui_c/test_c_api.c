// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

// Smoke test for the FTXUI C API.
// Verifies that all main entry points compile, link, and execute without
// crashing.  Does NOT launch a full-screen interactive app.

#include "ftxui-c/ftxui.h"

#include <stdio.h>

static void on_quit(void* ud) {
  (void)ud;
  ftxui_exit();
}

static ftxui_element_t render_fn(void* ud) {
  (void)ud;
  ftxui_element_t els[3];
  els[0] = ftxui_text_bold("Hello from C!");
  els[1] = ftxui_separator();
  els[2] = ftxui_text("Press Q to quit");
  ftxui_element_t box = ftxui_vbox(els, 3);
  ftxui_element_free(els[0]);
  ftxui_element_free(els[1]);
  ftxui_element_free(els[2]);
  ftxui_element_t result = ftxui_border(box);
  ftxui_element_free(box);
  return result;
}

int main(void) {
  /* Theme */
  ftxui_set_theme(FTXUI_THEME_NORD);

  /* DOM elements */
  ftxui_element_t el1 = ftxui_text("plain");
  ftxui_element_t el2 = ftxui_text_bold("bold");
  ftxui_color_t cyan = FTXUI_COLOR_CYAN;
  ftxui_element_t el3 = ftxui_text_colored("colored", cyan);
  ftxui_element_t el4 = ftxui_separator();
  ftxui_element_t el5 = ftxui_spinner(3);
  ftxui_element_t el6 = ftxui_markdown("# Hello\n**bold**");

  ftxui_element_t row_els[3];
  row_els[0] = el1;
  row_els[1] = el2;
  row_els[2] = el3;
  ftxui_element_t row = ftxui_hbox(row_els, 3);
  ftxui_element_free(el1);
  ftxui_element_free(el2);
  ftxui_element_free(el3);

  ftxui_element_t col_els[3];
  col_els[0] = row;
  col_els[1] = el4;
  col_els[2] = el5;
  ftxui_element_t col = ftxui_vbox(col_els, 3);
  ftxui_element_free(row);
  ftxui_element_free(el4);
  ftxui_element_free(el5);
  ftxui_element_free(el6);

  ftxui_element_t bordered = ftxui_border(col);
  ftxui_element_free(col);
  ftxui_element_t flexed = ftxui_flex(bordered);
  ftxui_element_free(bordered);
  ftxui_element_t centered = ftxui_center(flexed);
  ftxui_element_free(flexed);
  ftxui_element_free(centered);

  /* Components */
  ftxui_component_t btn = ftxui_button("Quit", on_quit, NULL);
  ftxui_component_t ui = ftxui_renderer(render_fn, NULL);
  ftxui_component_t kids[2];
  kids[0] = ui;
  kids[1] = btn;
  ftxui_component_t vcomp = ftxui_container_vertical(kids, 2);
  ftxui_component_t hcomp = ftxui_container_horizontal(kids, 2);
  ftxui_component_free(vcomp);
  ftxui_component_free(hcomp);
  ftxui_component_free(btn);
  ftxui_component_free(ui);

  /* Log panel */
  ftxui_log_panel_t log = ftxui_log_panel_create(100);
  ftxui_log_panel_info(log, "info message");
  ftxui_log_panel_warn(log, "warning");
  ftxui_log_panel_error(log, "error");
  ftxui_log_panel_debug(log, "debug");
  ftxui_component_t log_comp = ftxui_log_panel_build(log, "Log");
  ftxui_component_free(log_comp);
  ftxui_log_panel_free(log);

  /* Reactive int */
  ftxui_reactive_int_t ri = ftxui_reactive_int_create(42);
  ftxui_reactive_int_set(ri, 100);
  int val = ftxui_reactive_int_get(ri);
  ftxui_reactive_int_free(ri);

  /* Reactive float */
  ftxui_reactive_float_t rf = ftxui_reactive_float_create(1.5f);
  ftxui_reactive_float_set(rf, 3.14f);
  float fval = ftxui_reactive_float_get(rf);
  ftxui_reactive_float_free(rf);

  printf("C API: OK (reactive_int=%d, reactive_float=%.2f)\n", val, fval);
  return 0;
}
