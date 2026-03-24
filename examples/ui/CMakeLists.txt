set(DIRECTORY_LIB ui)

example(charts_demo)
example(form_demo)
example(mvu_counter)
example(mvu_todo)
example(layout_demo)
example(keymap_demo)
example(state_demo)
example(simple_table_demo)
example(progress_demo)
example(datatable_demo)
example(tree_demo)
example(router_demo)
example(command_palette_demo)
example(notification_demo)
example(wizard_demo)
example(log_demo)
example(list_demo)
example(filepicker_demo)
example(config_demo)
example(showcase)
example(new_features_demo)
example(geomap_demo)
example(world_ops_demo)

# Copy world map GeoJSON data next to the binary at build time so examples
# can locate it at runtime via their relative-path search.
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/world_ne110m.geojson"
  "${CMAKE_CURRENT_BINARY_DIR}/world_ne110m.geojson"
  COPYONLY
)
