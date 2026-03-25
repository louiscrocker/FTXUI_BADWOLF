vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO ArthurSonzogni/FTXUI
  REF "v${VERSION}"
  SHA512 0  # placeholder — update with real SHA512 before publishing
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS
    -DFTXUI_BUILD_EXAMPLES=OFF
    -DFTXUI_BUILD_TESTS=OFF
    -DFTXUI_BUILD_DOCS=OFF
    -DFTXUI_ENABLE_INSTALL=ON
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
  PACKAGE_NAME ftxui
  CONFIG_PATH lib/cmake/ftxui
)

# Remove debug include/ (duplicate of release)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# Install license
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# Install usage instructions
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
