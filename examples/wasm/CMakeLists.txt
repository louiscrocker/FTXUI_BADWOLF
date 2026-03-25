cmake_minimum_required(VERSION 3.12)

# This subdirectory is only included when FTXUI_BUILD_WASM=ON.
# cmake/ftxui_wasm.cmake must have been included by the parent before this.

add_wasm_target(badwolf_playground hello_wasm.cpp)
target_link_libraries(badwolf_playground PRIVATE ftxui::ui)
