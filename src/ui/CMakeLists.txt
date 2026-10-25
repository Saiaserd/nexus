# UI layer: all Qt Widgets code lives here.
add_library(nexus_ui STATIC
  MainWindow.h
  MainWindow.cpp)

target_include_directories(nexus_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(nexus_ui PUBLIC Qt6::Widgets)

nexus_set_warnings(nexus_ui)
nexus_enable_sanitizers(nexus_ui)
