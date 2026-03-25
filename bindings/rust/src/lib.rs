// src/lib.rs — Safe Rust wrapper around the FTXUI C API.
//
// # Architecture
//
// The crate has two layers:
//
// 1. `ffi` (private) — raw `extern "C"` declarations matching ftxui.h.
// 2. Public safe wrappers — RAII structs that call the corresponding
//    `_free()` function on drop, preventing resource leaks.
//
// # Example
//
// ```no_run
// use ftxui::{App, Theme, text_bold, separator, vbox, border};
//
// fn main() {
//     App::new()
//         .theme(Theme::Nord)
//         .run(|| {
//             let items = [text_bold("Hello from Rust!"), separator(), text("FTXUI C bindings")];
//             border(vbox(&items))
//         });
// }
// ```

pub mod ffi;
pub mod builder;

pub use builder::{App, Theme};

use ffi::*;

// ── Re-exported element constructors ─────────────────────────────────────────

/// Plain text element.
pub fn text(s: &str) -> Element {
    let c = std::ffi::CString::new(s).unwrap_or_default();
    Element(unsafe { ftxui_text(c.as_ptr()) })
}

/// Bold text element.
pub fn text_bold(s: &str) -> Element {
    let c = std::ffi::CString::new(s).unwrap_or_default();
    Element(unsafe { ftxui_text_bold(c.as_ptr()) })
}

/// Colored text element (RGB 0-255).
pub fn text_colored(s: &str, r: u8, g: u8, b: u8) -> Element {
    let c = std::ffi::CString::new(s).unwrap_or_default();
    let color = FtxuiColor { r, g, b };
    Element(unsafe { ftxui_text_colored(c.as_ptr(), color) })
}

/// Horizontal separator line.
pub fn separator() -> Element {
    Element(unsafe { ftxui_separator() })
}

/// Horizontal box containing `children`.
pub fn hbox(children: &[Element]) -> Element {
    let mut ptrs: Vec<*mut ftxui_element_s> = children.iter().map(|e| e.0).collect();
    Element(unsafe { ftxui_hbox(ptrs.as_mut_ptr(), ptrs.len()) })
}

/// Vertical box containing `children`.
pub fn vbox(children: &[Element]) -> Element {
    let mut ptrs: Vec<*mut ftxui_element_s> = children.iter().map(|e| e.0).collect();
    Element(unsafe { ftxui_vbox(ptrs.as_mut_ptr(), ptrs.len()) })
}

/// Wrap an element in a rounded border.
pub fn border(inner: Element) -> Element {
    let ptr = inner.0;
    std::mem::forget(inner); // ownership transferred — border consumes
    Element(unsafe { ftxui_border(ptr) })
}

/// Make an element flexible (expand to fill space).
pub fn flex(inner: Element) -> Element {
    let ptr = inner.0;
    std::mem::forget(inner);
    Element(unsafe { ftxui_flex(ptr) })
}

/// Centre an element horizontally.
pub fn hcenter(inner: Element) -> Element {
    let ptr = inner.0;
    std::mem::forget(inner);
    Element(unsafe { ftxui_hcenter(ptr) })
}

/// Centre an element vertically.
pub fn vcenter(inner: Element) -> Element {
    let ptr = inner.0;
    std::mem::forget(inner);
    Element(unsafe { ftxui_vcenter(ptr) })
}

/// Centre an element both horizontally and vertically.
pub fn center(inner: Element) -> Element {
    let ptr = inner.0;
    std::mem::forget(inner);
    Element(unsafe { ftxui_center(ptr) })
}

/// Render Markdown as a stacked element tree.
pub fn markdown(md: &str) -> Element {
    let c = std::ffi::CString::new(md).unwrap_or_default();
    Element(unsafe { ftxui_markdown(c.as_ptr()) })
}

// ── Element RAII wrapper ──────────────────────────────────────────────────────

/// An FTXUI DOM element.  Freed automatically when dropped.
pub struct Element(pub(crate) *mut ftxui_element_s);

impl Drop for Element {
    fn drop(&mut self) {
        if !self.0.is_null() {
            unsafe { ftxui_element_free(self.0) };
            self.0 = std::ptr::null_mut();
        }
    }
}

// Elements are not Send/Sync because ftxui elements are used only on the UI
// thread.

// ── Component RAII wrapper ────────────────────────────────────────────────────

/// An FTXUI interactive component.  Freed automatically when dropped.
pub struct Component(pub(crate) *mut ftxui_component_s);

impl Drop for Component {
    fn drop(&mut self) {
        if !self.0.is_null() {
            unsafe { ftxui_component_free(self.0) };
            self.0 = std::ptr::null_mut();
        }
    }
}
