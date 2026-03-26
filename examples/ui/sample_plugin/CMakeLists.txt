# CMakeLists.txt for the sample BadWolf plugin shared library.
#
# Build:
#   cmake --build build --target hello_plugin
#
# The resulting libhello_plugin.dylib (macOS) / libhello_plugin.so (Linux) can
# be loaded at runtime via PluginRegistry::Instance().Load("path/to/lib...").

cmake_minimum_required(VERSION 3.12)
project(hello_plugin)

add_library(hello_plugin SHARED hello_plugin.cpp)

target_link_libraries(hello_plugin PRIVATE ui)

target_compile_features(hello_plugin PRIVATE cxx_std_20)

# Shared libraries must be position-independent.
set_property(TARGET hello_plugin PROPERTY POSITION_INDEPENDENT_CODE ON)

# On macOS, suppress undefined symbol warnings — the ftxui symbols are provided
# by the host application at load time.
if(APPLE)
  target_link_options(hello_plugin PRIVATE "-undefined" "dynamic_lookup")
endif()

# Ensure the output filename is predictable across platforms.
set_target_properties(hello_plugin PROPERTIES
  PREFIX "lib"
  OUTPUT_NAME "hello_plugin"
)
