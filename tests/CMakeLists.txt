find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# One test executable per module. Add new test sources to the matching list.
add_executable(nexus_core_tests
  core/SmokeTest.cpp)

target_link_libraries(nexus_core_tests PRIVATE nexus_core GTest::gtest GTest::gtest_main)

nexus_set_warnings(nexus_core_tests)
nexus_enable_sanitizers(nexus_core_tests)

# Must come after the sanitizer step so the ASan runtime DLL is already beside the exe.
gtest_discover_tests(nexus_core_tests)
