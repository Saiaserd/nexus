find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# One test executable per module. Add new test sources to the matching list.
add_executable(nexus_core_tests
  core/SmokeTest.cpp
  core/PathOrderTest.cpp
  core/VaultTest.cpp
  core/NoteFilesTest.cpp
  core/VaultWatcherTest.cpp
  core/ForceLayoutTest.cpp
  core/TestMain.cpp)

target_link_libraries(nexus_core_tests PRIVATE nexus_core Qt6::Core Qt6::Test GTest::gtest)

nexus_deploy_qt(nexus_core_tests)
# Timing assertions only make sense in optimized builds (debug / asan check correctness only).
target_compile_definitions(nexus_core_tests PRIVATE $<$<CONFIG:Release>:NEXUS_ENFORCE_PERF>)
nexus_set_warnings(nexus_core_tests)
nexus_enable_sanitizers(nexus_core_tests)

# Must come after the sanitizer step so the ASan runtime DLL is already beside the exe.
# Performance tests (suite names ending in Perf) get the `perf` label; they run by default.
gtest_discover_tests(nexus_core_tests TEST_FILTER "-*Perf.*")
gtest_discover_tests(nexus_core_tests TEST_FILTER "*Perf.*" PROPERTIES LABELS perf)

# Parser tests (link Qt, so the Qt runtime has to be deployed beside the exe).
add_executable(nexus_parser_tests
  parser/MarkdownLexerTest.cpp)

target_link_libraries(nexus_parser_tests PRIVATE nexus_parser GTest::gtest GTest::gtest_main)

nexus_set_warnings(nexus_parser_tests)
nexus_enable_sanitizers(nexus_parser_tests)
nexus_deploy_qt(nexus_parser_tests)

gtest_discover_tests(nexus_parser_tests)

# GUI smoke test: start the real application, show the main window, quit.
# Non-Windows uses the offscreen platform plugin so it runs headless (CI); on Windows the
# native plugin is used (the offscreen plugin is not deployed by windeployqt).
add_test(NAME nexus_gui_smoke COMMAND nexus --quit-after-show)
set_tests_properties(nexus_gui_smoke PROPERTIES TIMEOUT 60)
if(NOT WIN32)
  set_tests_properties(nexus_gui_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# UI tests (Widgets, offscreen platform; own main() creates the QApplication).
add_executable(nexus_ui_tests
  ui/UiTestMain.cpp
  ui/ThemeTest.cpp)

target_compile_definitions(nexus_ui_tests PRIVATE NEXUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(nexus_ui_tests PRIVATE nexus_ui GTest::gtest)

nexus_set_warnings(nexus_ui_tests)
nexus_enable_sanitizers(nexus_ui_tests)
nexus_deploy_qt(nexus_ui_tests)

# windeployqt only ships the windows platform plugin; the tests run on the offscreen one.
find_package(Qt6 REQUIRED COMPONENTS Gui)
if(WIN32 AND TARGET Qt6::QOffscreenIntegrationPlugin)
  add_custom_command(TARGET nexus_ui_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>
            $<TARGET_FILE_DIR:nexus_ui_tests>/platforms/
    VERBATIM)
endif()

gtest_discover_tests(nexus_ui_tests)
