option(FTXUI_BUILD_DOCS "Set to ON to build docs" OFF)
option(FTXUI_BUILD_EXAMPLES "Set to ON to build examples" OFF)
option(FTXUI_BUILD_MODULES "Build the C++20 modules" OFF)
option(FTXUI_BUILD_TESTS "Set to ON to build tests" OFF)
option(FTXUI_BUILD_C_BINDINGS "Build C language bindings (libftxui-c)" OFF)
option(FTXUI_BUILD_PYTHON_BINDINGS "Build Python bindings via pybind11" OFF)
option(FTXUI_BUILD_TESTS_FUZZER "Set to ON to enable fuzzing" OFF)
option(FTXUI_CLANG_TIDY "Execute clang-tidy" OFF)
option(FTXUI_DEV_WARNINGS "Enable more compiler warnings and warnings as errors" OFF)
option(FTXUI_ENABLE_COVERAGE "Execute code coverage" OFF)
option(FTXUI_ENABLE_INSTALL "Generate the install target" ON)
option(FTXUI_QUIET "Set to ON for FTXUI to be quiet" OFF)

if (FTXUI_BUILD_MODULES)
  cmake_minimum_required(VERSION 3.28.2)
else()
  cmake_minimum_required(VERSION 3.12)
endif()

project(ftxui
  LANGUAGES CXX
  VERSION 6.1.9
  DESCRIPTION "C++ Functional Terminal User Interface."
)


set(FTXUI_MICROSOFT_TERMINAL_FALLBACK_HELP_TEXT "On windows, assume the \
terminal used will be one of Microsoft and use a set of reasonnable fallback \
to counteract its implementations problems.")
if (WIN32)
  option(FTXUI_MICROSOFT_TERMINAL_FALLBACK
    ${FTXUI_MICROSOFT_TERMINAL_FALLBACK_HELP_TEXT} ON)
else()
  option(FTXUI_MICROSOFT_TERMINAL_FALLBACK
    ${FTXUI_MICROSOFT_TERMINAL_FALLBACK_HELP_TEXT} OFF)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/ftxui_message.cmake)

add_library(screen
  include/ftxui/screen/box.hpp
  include/ftxui/screen/cell.hpp
  include/ftxui/screen/color.hpp
  include/ftxui/screen/color_info.hpp
  include/ftxui/screen/image.hpp
  include/ftxui/screen/pixel.hpp
  include/ftxui/screen/screen.hpp
  include/ftxui/screen/string.hpp
  include/ftxui/screen/surface.hpp
  src/ftxui/screen/box.cpp
  src/ftxui/screen/color.cpp
  src/ftxui/screen/color_info.cpp
  src/ftxui/screen/surface.cpp
  src/ftxui/screen/screen.cpp
  src/ftxui/screen/string.cpp
  src/ftxui/screen/terminal.cpp
  src/ftxui/screen/util.hpp
)

add_library(dom
  include/ftxui/dom/canvas.hpp
  include/ftxui/dom/direction.hpp
  include/ftxui/dom/elements.hpp
  include/ftxui/dom/flexbox_config.hpp
  include/ftxui/dom/node.hpp
  include/ftxui/dom/requirement.hpp
  include/ftxui/dom/selection.hpp
  include/ftxui/dom/take_any_args.hpp
  src/ftxui/dom/automerge.cpp
  src/ftxui/dom/selection_style.cpp
  src/ftxui/dom/blink.cpp
  src/ftxui/dom/bold.cpp
  src/ftxui/dom/border.cpp
  src/ftxui/dom/box_helper.cpp
  src/ftxui/dom/box_helper.hpp
  src/ftxui/dom/canvas.cpp
  src/ftxui/dom/clear_under.cpp
  src/ftxui/dom/color.cpp
  src/ftxui/dom/composite_decorator.cpp
  src/ftxui/dom/dbox.cpp
  src/ftxui/dom/dim.cpp
  src/ftxui/dom/flex.cpp
  src/ftxui/dom/flexbox.cpp
  src/ftxui/dom/flexbox_config.cpp
  src/ftxui/dom/flexbox_helper.cpp
  src/ftxui/dom/flexbox_helper.hpp
  src/ftxui/dom/focus.cpp
  src/ftxui/dom/frame.cpp
  src/ftxui/dom/gauge.cpp
  src/ftxui/dom/graph.cpp
  src/ftxui/dom/gridbox.cpp
  src/ftxui/dom/hbox.cpp
  src/ftxui/dom/hyperlink.cpp
  src/ftxui/dom/inverted.cpp
  src/ftxui/dom/italic.cpp
  src/ftxui/dom/linear_gradient.cpp
  src/ftxui/dom/node.cpp
  src/ftxui/dom/node_decorator.cpp
  src/ftxui/dom/paragraph.cpp
  src/ftxui/dom/reflect.cpp
  src/ftxui/dom/scroll_indicator.cpp
  src/ftxui/dom/selection.cpp
  src/ftxui/dom/separator.cpp
  src/ftxui/dom/size.cpp
  src/ftxui/dom/spinner.cpp
  src/ftxui/dom/strikethrough.cpp
  src/ftxui/dom/table.cpp
  src/ftxui/dom/text.cpp
  src/ftxui/dom/underlined.cpp
  src/ftxui/dom/underlined_double.cpp
  src/ftxui/dom/util.cpp
  src/ftxui/dom/vbox.cpp
)

add_library(component
  include/ftxui/component/animation.hpp
  include/ftxui/component/app.hpp
  include/ftxui/component/captured_mouse.hpp
  include/ftxui/component/component.hpp
  include/ftxui/component/component_base.hpp
  include/ftxui/component/component_options.hpp
  include/ftxui/component/event.hpp
  include/ftxui/component/loop.hpp
  include/ftxui/component/mouse.hpp
  include/ftxui/component/receiver.hpp
  include/ftxui/component/screen_interactive.hpp
  include/ftxui/component/task.hpp
  src/ftxui/component/animation.cpp
  src/ftxui/component/button.cpp
  src/ftxui/component/catch_event.cpp
  src/ftxui/component/checkbox.cpp
  src/ftxui/component/collapsible.cpp
  src/ftxui/component/component.cpp
  src/ftxui/component/component_options.cpp
  src/ftxui/component/container.cpp
  src/ftxui/component/dropdown.cpp
  src/ftxui/component/event.cpp
  src/ftxui/component/hoverable.cpp
  src/ftxui/component/input.cpp
  src/ftxui/component/loop.cpp
  src/ftxui/component/maybe.cpp
  src/ftxui/component/menu.cpp
  src/ftxui/component/modal.cpp
  src/ftxui/component/radiobox.cpp
  src/ftxui/component/renderer.cpp
  src/ftxui/component/resizable_split.cpp
  src/ftxui/component/app.cpp
  src/ftxui/component/slider.cpp
  src/ftxui/component/task.cpp
  src/ftxui/component/task_internal.hpp
  src/ftxui/component/task_queue.cpp
  src/ftxui/component/task_queue.hpp
  src/ftxui/component/task_runner.cpp
  src/ftxui/component/task_runner.hpp
  src/ftxui/component/terminal_input_parser.cpp
  src/ftxui/component/terminal_input_parser.hpp
  src/ftxui/component/util.cpp
  src/ftxui/component/window.cpp
)

add_library(ui
  include/ftxui/ui.hpp
  include/ftxui/ui/app.hpp
  include/ftxui/ui/background_task.hpp
  include/ftxui/ui/command_palette.hpp
  include/ftxui/ui/datatable.hpp
  include/ftxui/ui/dialog.hpp
  include/ftxui/ui/form.hpp
  include/ftxui/ui/grid.hpp
  include/ftxui/ui/keymap.hpp
  include/ftxui/ui/layout.hpp
  include/ftxui/ui/list.hpp
  include/ftxui/ui/log_panel.hpp
  include/ftxui/ui/mvu.hpp
  include/ftxui/ui/notification.hpp
  include/ftxui/ui/router.hpp
  include/ftxui/ui/tree.hpp
  include/ftxui/ui/command_palette.hpp
  include/ftxui/ui/grid.hpp
  include/ftxui/ui/progress.hpp
  include/ftxui/ui/simple_table.hpp
  include/ftxui/ui/state.hpp
  include/ftxui/ui/textinput.hpp
  include/ftxui/ui/theme.hpp
  include/ftxui/ui/canvas.hpp
  include/ftxui/ui/charts.hpp
  include/ftxui/ui/widgets.hpp
  include/ftxui/ui/wizard.hpp
  src/ftxui/ui/canvas.cpp
  src/ftxui/ui/charts.cpp
  src/ftxui/ui/dialog.cpp
  src/ftxui/ui/form.cpp
  src/ftxui/ui/grid.cpp
  src/ftxui/ui/keymap.cpp
  src/ftxui/ui/layout.cpp
  src/ftxui/ui/log_panel.cpp
  src/ftxui/ui/notification.cpp
  src/ftxui/ui/progress.cpp
  src/ftxui/ui/simple_table.cpp
  src/ftxui/ui/textinput.cpp
  src/ftxui/ui/theme.cpp
  src/ftxui/ui/tree.cpp
  src/ftxui/ui/router.cpp
  src/ftxui/ui/command_palette.cpp
  src/ftxui/ui/widgets.cpp
  src/ftxui/ui/wizard.cpp
  include/ftxui/ui/geojson.hpp
  include/ftxui/ui/geomap.hpp
  src/ftxui/ui/geojson.cpp
  src/ftxui/ui/geomap.cpp
  include/ftxui/ui/process_panel.hpp
  src/ftxui/ui/process_panel.cpp
  include/ftxui/ui/globe.hpp
  src/ftxui/ui/globe.cpp
  include/ftxui/ui/world_map_data.hpp
  src/ftxui/ui/world_map_data.cpp
  include/ftxui/ui/filepicker.hpp
  include/ftxui/ui/config_editor.hpp
  src/ftxui/ui/filepicker.cpp
  src/ftxui/ui/config_editor.cpp
  include/ftxui/ui/markdown.hpp
  src/ftxui/ui/markdown.cpp
  include/ftxui/ui/virtual_list.hpp
  src/ftxui/ui/virtual_list.cpp
  include/ftxui/ui/sortable_table.hpp
  src/ftxui/ui/sortable_table.cpp
  include/ftxui/ui/debug_overlay.hpp
  src/ftxui/ui/debug_overlay.cpp
  include/ftxui/ui/event_recorder.hpp
  src/ftxui/ui/event_recorder.cpp
  include/ftxui/ui/ui_builder.hpp
  src/ftxui/ui/ui_builder.cpp
  include/ftxui/ui/inspector.hpp
  src/ftxui/ui/inspector.cpp
  src/ftxui/ui/lcars.cpp
  src/ftxui/ui/alert.cpp
  include/ftxui/ui/bind.hpp
  include/ftxui/ui/reactive_list.hpp
  include/ftxui/ui/model_binding.hpp
  include/ftxui/ui/data_context.hpp
  include/ftxui/ui/reactive_decorators.hpp
  src/ftxui/ui/reactive_decorators.cpp
  include/ftxui/ui/animation.hpp
  src/ftxui/ui/animation.cpp
  include/ftxui/ui/particles.hpp
  src/ftxui/ui/particles.cpp
  include/ftxui/ui/transitions.hpp
  include/ftxui/ui/galaxy_map.hpp
  src/ftxui/ui/galaxy_map.cpp
  include/ftxui/ui/network_reactive.hpp
  src/ftxui/ui/network_reactive.cpp
  include/ftxui/ui/collab.hpp
  src/ftxui/ui/collab.cpp
  include/ftxui/ui/llm_bridge.hpp
  src/ftxui/ui/llm_bridge.cpp
  include/ftxui/ui/live_source.hpp
  src/ftxui/ui/live_source.cpp
  include/ftxui/ui/registry.hpp
  src/ftxui/ui/registry.cpp
  include/ftxui/ui/rich_text.hpp
  src/ftxui/ui/rich_text.cpp
  include/ftxui/ui/webgl_renderer.hpp
  src/ftxui/ui/webgl_renderer.cpp
  include/ftxui/ui/wasm_bridge.hpp
  src/ftxui/ui/wasm_bridge.cpp
  include/ftxui/ui/typewriter.hpp
  src/ftxui/ui/typewriter.cpp
)

target_link_libraries(dom PUBLIC screen)
target_link_libraries(component PUBLIC dom)
target_link_libraries(ui PUBLIC component)

include(cmake/ftxui_set_options.cmake)
ftxui_set_options(screen)
ftxui_set_options(dom)
ftxui_set_options(component)
ftxui_set_options(ui)

include(cmake/ftxui_coverage.cmake)
ftxui_check_coverage(screen)
ftxui_check_coverage(dom)
ftxui_check_coverage(component)
ftxui_check_coverage(ui)

include(cmake/ftxui_test.cmake)
include(cmake/ftxui_benchmark.cmake)
include(cmake/ftxui_fuzzer.cmake)
include(cmake/iwyu.cmake)
include(cmake/ftxui_export.cmake)
include(cmake/ftxui_install.cmake)
include(cmake/ftxui_package.cmake)
include(cmake/ftxui_modules.cmake)

add_subdirectory(examples)
add_subdirectory(doc)
add_subdirectory(apps)

# ── C language bindings ──────────────────────────────────────────────────────
if(FTXUI_BUILD_C_BINDINGS)
  enable_language(C)

  add_library(ftxui-c
    src/ftxui_c/ftxui_c.cpp
  )
  target_include_directories(ftxui-c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(ftxui-c PRIVATE ui component dom screen)
  set_target_properties(ftxui-c PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
  )

  add_executable(ftxui_c_test src/ftxui_c/test_c_api.c)
  target_link_libraries(ftxui_c_test PRIVATE ftxui-c)
  target_include_directories(ftxui_c_test PRIVATE include)
endif()

# You can generate ./examples_modules/ by running
# ./tools/generate_examples_modules.sh
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples_modules/CMakeLists.txt")
  add_subdirectory(examples_modules)
endif()

# ── Python bindings ──────────────────────────────────────────────────────────
if(FTXUI_BUILD_PYTHON_BINDINGS)
  find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
  include(FetchContent)
  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.11.1
  )
  FetchContent_MakeAvailable(pybind11)
  add_subdirectory(python)
endif()

include(cmake/ftxui_wasm.cmake)
if(FTXUI_BUILD_WASM)
  add_subdirectory(examples/wasm)
endif()
