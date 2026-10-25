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

# AI layer (Claude Code CLI bridge). Tests run against a fake CLI, never the real claude.
add_executable(fake_claude ai/fake_claude/main.cpp)
nexus_set_warnings(fake_claude)
nexus_enable_sanitizers(fake_claude)

add_executable(nexus_ai_tests
  ai/StreamJsonParserTest.cpp
  ai/AgentRunTest.cpp
  core/TestMain.cpp)

target_link_libraries(nexus_ai_tests PRIVATE nexus_ai Qt6::Test GTest::gtest)
target_compile_definitions(nexus_ai_tests PRIVATE
  NEXUS_AI_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/ai/fixtures"
  NEXUS_FAKE_CLAUDE_PATH="$<TARGET_FILE:fake_claude>")
add_dependencies(nexus_ai_tests fake_claude)

nexus_set_warnings(nexus_ai_tests)
nexus_enable_sanitizers(nexus_ai_tests)
nexus_deploy_qt(nexus_ai_tests)

gtest_discover_tests(nexus_ai_tests)
