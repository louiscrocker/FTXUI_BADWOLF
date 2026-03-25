cmake_minimum_required(VERSION 3.15)
project(pyftxui CXX)

pybind11_add_module(_ftxui src/bindings.cpp)

target_link_libraries(_ftxui PRIVATE ui component dom screen)

target_include_directories(_ftxui PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(_ftxui PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    # Place the .so next to the Python package so `import _ftxui` works
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/pyftxui
)
