// examples/hello.rs — Minimal demonstration of the FTXUI Rust bindings.
//
// Build and run:
//   # 1. Build libftxui-c first:
//   #    cmake -S ../.. -B ../../build -DFTXUI_BUILD_C_BINDINGS=ON
//   #    cmake --build ../../build --target ftxui-c
//   #
//   # 2. Run the example:
//   #    FTXUI_C_LIB_DIR=../../build cargo run --example hello

use ftxui::{App, Theme, border, separator, text, text_bold, vbox};

fn main() {
    App::new()
        .theme(Theme::Nord)
        .run_renderer(|| {
            let items = [
                text_bold("Hello from Rust!"),
                separator(),
                text("Built on FTXUI C bindings"),
                separator(),
                text("Press Ctrl-C to quit"),
            ];
            border(vbox(&items))
        });
}
