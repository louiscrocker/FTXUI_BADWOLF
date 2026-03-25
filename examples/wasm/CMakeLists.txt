cmake_minimum_required(VERSION 3.12)

# This subdirectory is only included when FTXUI_BUILD_WASM=ON.
# cmake/ftxui_wasm.cmake must have been included by the parent before this.

add_wasm_target(badwolf_playground hello_wasm.cpp)
target_link_libraries(badwolf_playground PRIVATE ftxui::ui)

add_wasm_target(badwolf_webgl_demo webgl_demo.cpp)
target_link_libraries(badwolf_webgl_demo PRIVATE ftxui::ui)

# Link WebGL when building for WASM
if(EMSCRIPTEN)
  target_link_options(badwolf_webgl_demo PRIVATE
    "-sUSE_WEBGL2=1"
    "-sFULL_ES2=1"
    "-sFULL_ES3=1"
  )

  # Copy the WebGL shell HTML next to the output JS/WASM files.
  add_custom_command(TARGET badwolf_webgl_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/webgl_shell.html"
      "${CMAKE_CURRENT_BINARY_DIR}/badwolf_webgl_demo.html"
    COMMENT "Copying WebGL shell HTML for badwolf_webgl_demo"
  )
endif()

