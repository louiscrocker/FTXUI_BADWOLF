// src/builder.rs — High-level builder API layered on top of the safe wrappers.
//
// This module provides the `App` and `Theme` types that give Rust callers an
// ergonomic experience similar to the C++ `ftxui::ui` layer.

use crate::ffi::{ftxui_run_fullscreen, ftxui_set_theme, FtxuiTheme};
use crate::{Component, Element};

// ── Theme ─────────────────────────────────────────────────────────────────────

/// Built-in visual themes.
#[derive(Clone, Copy, PartialEq, Eq, Default)]
pub enum Theme {
    #[default]
    Default,
    Dark,
    Light,
    Nord,
    Dracula,
    Monokai,
}

impl From<Theme> for FtxuiTheme {
    fn from(t: Theme) -> Self {
        match t {
            Theme::Default => FtxuiTheme::Default,
            Theme::Dark => FtxuiTheme::Dark,
            Theme::Light => FtxuiTheme::Light,
            Theme::Nord => FtxuiTheme::Nord,
            Theme::Dracula => FtxuiTheme::Dracula,
            Theme::Monokai => FtxuiTheme::Monokai,
        }
    }
}

// ── App builder ───────────────────────────────────────────────────────────────

/// Entry point for running an FTXUI application.
///
/// # Example
///
/// ```no_run
/// use ftxui::{App, Theme, text_bold, separator, vbox, border};
///
/// App::new()
///     .theme(Theme::Nord)
///     .run(|| border(vbox(&[text_bold("Hello!"), separator()])));
/// ```
pub struct App {
    theme: Theme,
}

impl App {
    /// Create a new app builder with default settings.
    pub fn new() -> Self {
        Self {
            theme: Theme::Default,
        }
    }

    /// Set the visual theme applied before building components.
    pub fn theme(mut self, theme: Theme) -> Self {
        self.theme = theme;
        self
    }

    /// Run the application fullscreen.
    ///
    /// `build_fn` is called once to produce the root component.  The function
    /// blocks until the user exits the app.
    pub fn run<F>(self, build_fn: F)
    where
        F: FnOnce() -> Component,
    {
        unsafe { ftxui_set_theme(self.theme.into()) };
        let root = build_fn();
        unsafe { ftxui_run_fullscreen(root.0) };
    }

    /// Run the application fullscreen with a renderer closure.
    ///
    /// `render_fn` is called every frame to produce the root element.
    pub fn run_renderer<F>(self, render_fn: F)
    where
        F: Fn() -> Element + 'static,
    {
        unsafe { ftxui_set_theme(self.theme.into()) };

        // Wrap the closure behind a raw pointer so it can cross the C boundary.
        let boxed: Box<dyn Fn() -> Element> = Box::new(render_fn);
        let raw = Box::into_raw(Box::new(boxed));

        unsafe extern "C" fn trampoline(
            ud: *mut std::ffi::c_void,
        ) -> *mut crate::ffi::ftxui_element_s {
            let f = &*(ud as *const Box<dyn Fn() -> Element>);
            let el = f();
            let ptr = el.0;
            std::mem::forget(el); // ownership transferred to C
            ptr
        }

        let comp_ptr = unsafe {
            crate::ffi::ftxui_renderer(Some(trampoline), raw as *mut std::ffi::c_void)
        };
        let comp = Component(comp_ptr);
        unsafe { ftxui_run_fullscreen(comp.0) };

        // Reclaim the closure after the app exits.
        unsafe {
            drop(Box::from_raw(raw));
        }
    }
}

impl Default for App {
    fn default() -> Self {
        Self::new()
    }
}
