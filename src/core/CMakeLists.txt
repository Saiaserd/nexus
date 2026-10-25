# Core layer: pure C++ (no Qt Widgets; QtCore only if really needed).
find_package(tl-expected CONFIG REQUIRED)

add_library(nexus_core STATIC
  Expected.h
  NumberParser.h
  NumberParser.cpp
  FileError.h
  NoteInfo.h
  PathOrder.h
  PathOrder.cpp
  Vault.h
  Vault.cpp)

# Headers are included as "core/Expected.h" -> expose the src/ directory.
target_include_directories(nexus_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
# QtCore only (ADR 0004); never link Widgets here.
target_link_libraries(nexus_core PUBLIC tl::expected Qt6::Core)

nexus_set_warnings(nexus_core)
nexus_enable_sanitizers(nexus_core)
