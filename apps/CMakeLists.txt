option(FTXUI_BUILD_APPS "Build reference applications (ftop, ftgit, ftdb)" OFF)
if(NOT FTXUI_BUILD_APPS)
  return()
endif()

function(ftxui_app name)
  add_executable(${name} ${name}/main.cpp)
  target_link_libraries(${name} PRIVATE ftxui::ui ftxui::component ftxui::dom ftxui::screen)
  target_include_directories(${name} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
  )
  target_compile_features(${name} PRIVATE cxx_std_20)
  set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps"
  )
endfunction()

ftxui_app(ftop)
ftxui_app(ftgit)

# ftdb needs sqlite3
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
  ftxui_app(ftdb)
  target_link_libraries(ftdb PRIVATE SQLite::SQLite3)
  message(STATUS "ftdb: SQLite3 found — building database browser")
else()
  # Try to find sqlite3 manually (e.g. Homebrew on macOS)
  find_library(SQLITE3_LIB sqlite3 HINTS /opt/homebrew/lib /usr/local/lib /usr/lib)
  find_path(SQLITE3_INCLUDE sqlite3.h HINTS /opt/homebrew/include /usr/local/include /usr/include)
  if(SQLITE3_LIB AND SQLITE3_INCLUDE)
    ftxui_app(ftdb)
    target_include_directories(ftdb PRIVATE ${SQLITE3_INCLUDE})
    target_link_libraries(ftdb PRIVATE ${SQLITE3_LIB})
    message(STATUS "ftdb: SQLite3 found manually (${SQLITE3_LIB}) — building database browser")
  else()
    message(STATUS "ftdb: SQLite3 not found — skipping database browser")
  endif()
endif()
