add_library(ftxui::screen ALIAS screen)
add_library(ftxui::dom ALIAS dom)
add_library(ftxui::component ALIAS component)
add_library(ftxui::ui ALIAS ui)
export(
  TARGETS screen dom component ui
  NAMESPACE ftxui::
  FILE "${PROJECT_BINARY_DIR}/ftxui-targets.cmake"
)
