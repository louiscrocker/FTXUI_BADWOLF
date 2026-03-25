# cmake/ftxui_wasm.cmake
# Emscripten/WebAssembly build support for FTXUI BadWolf.
#
# Usage (from project root):
#   emcmake cmake -S . -B build-wasm -DFTXUI_BUILD_WASM=ON
#   cmake --build build-wasm
#
# The helper macro `add_wasm_target(name sources...)` creates an executable
# target pre-configured with the required Emscripten flags.

# ── Detection ─────────────────────────────────────────────────────────────────

# Check for Emscripten via the EMSCRIPTEN environment variable (set by emcmake)
# or by finding emcc on the PATH.
if(DEFINED ENV{EMSCRIPTEN})
  set(_FTXUI_EMSCRIPTEN_FOUND TRUE)
else()
  find_program(_EMCC_EXECUTABLE emcc)
  if(_EMCC_EXECUTABLE)
    set(_FTXUI_EMSCRIPTEN_FOUND TRUE)
  else()
    set(_FTXUI_EMSCRIPTEN_FOUND FALSE)
  endif()
endif()

# ── Option ────────────────────────────────────────────────────────────────────

option(FTXUI_BUILD_WASM
  "Build Emscripten/WebAssembly targets (requires Emscripten toolchain)"
  OFF)

if(FTXUI_BUILD_WASM AND NOT _FTXUI_EMSCRIPTEN_FOUND)
  message(WARNING
    "[ftxui_wasm] FTXUI_BUILD_WASM=ON but Emscripten was not detected.\n"
    "  Run CMake through 'emcmake cmake ...' or set the EMSCRIPTEN env var.\n"
    "  Continuing without WASM targets.")
  set(FTXUI_BUILD_WASM OFF CACHE BOOL "" FORCE)
endif()

# ── Emscripten compiler/linker flags ──────────────────────────────────────────

set(FTXUI_WASM_COMPILE_FLAGS
  -sUSE_SDL=0
  -sUSE_PTHREADS=1
  -sPTHREAD_POOL_SIZE=4
  -sALLOW_MEMORY_GROWTH=1
)

set(FTXUI_WASM_LINK_FLAGS
  -sUSE_SDL=0
  -sUSE_PTHREADS=1
  -sPTHREAD_POOL_SIZE=4
  -sALLOW_MEMORY_GROWTH=1
  "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
  -sMODULARIZE=1
  "-sEXPORT_NAME=BadWolf"
)

# ── Helper macro ──────────────────────────────────────────────────────────────

# add_wasm_target(name source1 [source2 ...])
#
# Creates an executable <name> configured for Emscripten output.
# After calling this macro link additional libraries with:
#   target_link_libraries(<name> PRIVATE ...)
macro(add_wasm_target _wasm_target_name)
  set(_wasm_sources ${ARGN})

  add_executable(${_wasm_target_name} ${_wasm_sources})

  # Output .js + .wasm alongside the HTML shell.
  set_target_properties(${_wasm_target_name} PROPERTIES
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  )

  target_compile_options(${_wasm_target_name}
    PRIVATE ${FTXUI_WASM_COMPILE_FLAGS}
  )

  # Pass flags as a space-separated string for the linker.
  string(JOIN " " _wasm_link_flags_str ${FTXUI_WASM_LINK_FLAGS})
  set_target_properties(${_wasm_target_name} PROPERTIES
    LINK_FLAGS "${_wasm_link_flags_str}"
  )

  # Copy the HTML shell next to the output JS/WASM files.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wasm_shell.html")
    add_custom_command(TARGET ${_wasm_target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/wasm_shell.html"
        "${CMAKE_CURRENT_BINARY_DIR}/${_wasm_target_name}.html"
      COMMENT "Copying WASM shell HTML for ${_wasm_target_name}"
    )
  endif()

  message(STATUS "[ftxui_wasm] Registered WASM target: ${_wasm_target_name}")
endmacro()
