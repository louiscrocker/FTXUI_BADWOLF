// build.rs — locates and links the pre-built libftxui-c shared/static library.
//
// Expected build setup:
//   1. Build FTXUI with -DFTXUI_BUILD_C_BINDINGS=ON (produces libftxui-c)
//   2. Set FTXUI_C_LIB_DIR to the directory containing the library, OR place
//      the library in the default search paths.
//   3. cargo build

use std::env;

fn main() {
    // Allow the user to point to the build directory via an env var.
    if let Ok(lib_dir) = env::var("FTXUI_C_LIB_DIR") {
        println!("cargo:rustc-link-search=native={lib_dir}");
    }

    // Link the C bindings layer.
    println!("cargo:rustc-link-lib=dylib=ftxui-c");

    // On macOS we need libc++ for the C++ runtime.
    #[cfg(target_os = "macos")]
    println!("cargo:rustc-link-lib=c++");

    // On Linux we need libstdc++ or libc++.
    #[cfg(target_os = "linux")]
    println!("cargo:rustc-link-lib=stdc++");
}
