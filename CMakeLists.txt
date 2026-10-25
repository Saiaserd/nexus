cmake_minimum_required(VERSION 3.24)

# Windows: static third-party libraries + dynamic CRT (/MD), matching Qt and
# keeping MSVC AddressSanitizer usable. Must be set before project() so that
# the vcpkg toolchain picks it up. Overridable with -DVCPKG_TARGET_TRIPLET=...
if(WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
  set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "vcpkg target triplet")
endif()

project(Nexus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Qt is NOT installed through vcpkg (ADR 0001). It is located through
# CMAKE_PREFIX_PATH, which the presets fill from the QT_ROOT environment
# variable (Windows: scripts/dev.ps1 sets it; Linux CI: install-qt-action).
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

include(NexusQtDeploy)
include(CompilerWarnings)
include(Sanitizers)
include(ClangTidy)

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/parser)
add_subdirectory(src/ui)
add_subdirectory(src/app)
add_subdirectory(tests)
