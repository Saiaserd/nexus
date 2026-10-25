# UI layer: all Qt Widgets code lives here.
find_package(Qt6 6.8 REQUIRED COMPONENTS Svg)

add_library(nexus_ui STATIC
  CrashDialogs.h
  CrashDialogs.cpp
  Fonts.h
  Fonts.cpp
  Icons.h
  Icons.cpp
  MainWindow.h
  MainWindow.cpp
  DevOptions.h
  DevOptions.cpp
  Theme.h
  Theme.cpp
  ThemeManager.h
  ThemeManager.cpp)

# Resources of a static library: qt_add_resources keeps them linked into the final executable.
qt_add_resources(nexus_ui nexus_ui_resources
  PREFIX "/ui"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
  FILES
    resources/app.qss.in
    resources/fonts/JetBrainsMono-Regular.ttf
    resources/fonts/JetBrainsMono-Medium.ttf
    resources/icons/alert-circle.svg
    resources/icons/alert-triangle.svg
    resources/icons/check.svg
    resources/icons/check-on-dark.svg
    resources/icons/check-on-light.svg
    resources/icons/chevron-down.svg
    resources/icons/chevron-right.svg
    resources/icons/close.svg
    resources/icons/collapse-all.svg
    resources/icons/file-plus.svg
    resources/icons/file.svg
    resources/icons/folder-plus.svg
    resources/icons/folder.svg
    resources/icons/logo.svg
    resources/icons/refresh.svg)

target_include_directories(nexus_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(nexus_ui PUBLIC Qt6::Widgets Qt6::Svg)

nexus_set_warnings(nexus_ui)
nexus_enable_sanitizers(nexus_ui)
