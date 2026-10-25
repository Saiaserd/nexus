find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# One test executable per module. Add new test sources to the matching list.
add_executable(nexus_core_tests
  core/SmokeTest.cpp
  core/PathOrderTest.cpp
  core/VaultTest.cpp
  core/NoteFilesTest.cpp)

target_link_libraries(nexus_core_tests PRIVATE nexus_core Qt6::Core GTest::gtest GTest::gtest_main)

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
