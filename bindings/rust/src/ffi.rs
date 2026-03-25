// src/ffi.rs — Raw `extern "C"` declarations matching include/ftxui-c/ftxui.h.
//
// These are `unsafe` and are not intended to be used directly by application
// code.  Use the safe wrappers in the crate root instead.

#![allow(non_camel_case_types, dead_code)]

use std::os::raw::{c_char, c_int, c_float};

// ── Opaque handle types ───────────────────────────────────────────────────────

#[repr(C)]
pub struct ftxui_element_s {
    _opaque: [u8; 0],
}

#[repr(C)]
pub struct ftxui_component_s {
    _opaque: [u8; 0],
}

#[repr(C)]
pub struct ftxui_log_panel_s {
    _opaque: [u8; 0],
}

#[repr(C)]
pub struct ftxui_reactive_int_s {
    _opaque: [u8; 0],
}

#[repr(C)]
pub struct ftxui_reactive_float_s {
    _opaque: [u8; 0],
}

// ── Color ─────────────────────────────────────────────────────────────────────

#[repr(C)]
#[derive(Clone, Copy)]
pub struct FtxuiColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

// ── Theme enum ────────────────────────────────────────────────────────────────

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum FtxuiTheme {
    Default = 0,
    Dark = 1,
    Light = 2,
    Nord = 3,
    Dracula = 4,
    Monokai = 5,
}

// ── Extern declarations ───────────────────────────────────────────────────────

extern "C" {
    // Lifecycle
    pub fn ftxui_element_free(el: *mut ftxui_element_s);
    pub fn ftxui_component_free(comp: *mut ftxui_component_s);

    // DOM elements
    pub fn ftxui_text(str: *const c_char) -> *mut ftxui_element_s;
    pub fn ftxui_text_bold(str: *const c_char) -> *mut ftxui_element_s;
    pub fn ftxui_text_colored(str: *const c_char, color: FtxuiColor)
        -> *mut ftxui_element_s;
    pub fn ftxui_separator() -> *mut ftxui_element_s;
    pub fn ftxui_hbox(
        elements: *mut *mut ftxui_element_s,
        count: usize,
    ) -> *mut ftxui_element_s;
    pub fn ftxui_vbox(
        elements: *mut *mut ftxui_element_s,
        count: usize,
    ) -> *mut ftxui_element_s;
    pub fn ftxui_border(inner: *mut ftxui_element_s) -> *mut ftxui_element_s;
    pub fn ftxui_flex(inner: *mut ftxui_element_s) -> *mut ftxui_element_s;
    pub fn ftxui_hcenter(inner: *mut ftxui_element_s) -> *mut ftxui_element_s;
    pub fn ftxui_vcenter(inner: *mut ftxui_element_s) -> *mut ftxui_element_s;
    pub fn ftxui_center(inner: *mut ftxui_element_s) -> *mut ftxui_element_s;
    pub fn ftxui_spinner(frame_index: c_int) -> *mut ftxui_element_s;
    pub fn ftxui_markdown(markdown_text: *const c_char) -> *mut ftxui_element_s;

    // Components
    pub fn ftxui_button(
        label: *const c_char,
        on_click: Option<unsafe extern "C" fn(*mut std::ffi::c_void)>,
        userdata: *mut std::ffi::c_void,
    ) -> *mut ftxui_component_s;
    pub fn ftxui_renderer(
        render_fn: Option<
            unsafe extern "C" fn(*mut std::ffi::c_void) -> *mut ftxui_element_s,
        >,
        userdata: *mut std::ffi::c_void,
    ) -> *mut ftxui_component_s;
    pub fn ftxui_catch_event(
        inner: *mut ftxui_component_s,
        handler: Option<
            unsafe extern "C" fn(*const c_char, *mut std::ffi::c_void) -> c_int,
        >,
        userdata: *mut std::ffi::c_void,
    ) -> *mut ftxui_component_s;
    pub fn ftxui_container_vertical(
        children: *mut *mut ftxui_component_s,
        count: usize,
    ) -> *mut ftxui_component_s;
    pub fn ftxui_container_horizontal(
        children: *mut *mut ftxui_component_s,
        count: usize,
    ) -> *mut ftxui_component_s;

    // App
    pub fn ftxui_run_fullscreen(root: *mut ftxui_component_s);
    pub fn ftxui_exit();
    pub fn ftxui_post_refresh();

    // Theme
    pub fn ftxui_set_theme(theme: FtxuiTheme);

    // LogPanel
    pub fn ftxui_log_panel_create(max_lines: c_int) -> *mut ftxui_log_panel_s;
    pub fn ftxui_log_panel_info(panel: *mut ftxui_log_panel_s, msg: *const c_char);
    pub fn ftxui_log_panel_warn(panel: *mut ftxui_log_panel_s, msg: *const c_char);
    pub fn ftxui_log_panel_error(panel: *mut ftxui_log_panel_s, msg: *const c_char);
    pub fn ftxui_log_panel_debug(panel: *mut ftxui_log_panel_s, msg: *const c_char);
    pub fn ftxui_log_panel_build(
        panel: *mut ftxui_log_panel_s,
        title: *const c_char,
    ) -> *mut ftxui_component_s;
    pub fn ftxui_log_panel_free(panel: *mut ftxui_log_panel_s);

    // Reactive<int>
    pub fn ftxui_reactive_int_create(initial: c_int) -> *mut ftxui_reactive_int_s;
    pub fn ftxui_reactive_int_set(r: *mut ftxui_reactive_int_s, value: c_int);
    pub fn ftxui_reactive_int_get(r: *mut ftxui_reactive_int_s) -> c_int;
    pub fn ftxui_reactive_int_on_change(
        r: *mut ftxui_reactive_int_s,
        fn_: Option<unsafe extern "C" fn(c_int, *mut std::ffi::c_void)>,
        userdata: *mut std::ffi::c_void,
    );
    pub fn ftxui_reactive_int_free(r: *mut ftxui_reactive_int_s);

    // Reactive<float>
    pub fn ftxui_reactive_float_create(initial: c_float) -> *mut ftxui_reactive_float_s;
    pub fn ftxui_reactive_float_set(r: *mut ftxui_reactive_float_s, value: c_float);
    pub fn ftxui_reactive_float_get(r: *mut ftxui_reactive_float_s) -> c_float;
    pub fn ftxui_reactive_float_on_change(
        r: *mut ftxui_reactive_float_s,
        fn_: Option<unsafe extern "C" fn(c_float, *mut std::ffi::c_void)>,
        userdata: *mut std::ffi::c_void,
    );
    pub fn ftxui_reactive_float_free(r: *mut ftxui_reactive_float_s);

    // World map / Globe
    pub fn ftxui_world_map() -> *mut ftxui_component_s;
    pub fn ftxui_globe() -> *mut ftxui_component_s;
}
