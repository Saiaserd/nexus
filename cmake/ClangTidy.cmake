# Optional clang-tidy integration (off by default; CI turns it on).
#   cmake --preset tidy        or      -DNEXUS_ENABLE_CLANG_TIDY=ON
# Must be included BEFORE the targets are created (CMAKE_CXX_CLANG_TIDY is read at
# target creation). Third-party and generated code is filtered by HeaderFilterRegex in
# .clang-tidy; the moc/autogen sources are skipped explicitly below.
option(NEXUS_ENABLE_CLANG_TIDY "Run clang-tidy on our C++ sources during the build" OFF)

if(NEXUS_ENABLE_CLANG_TIDY)
  find_program(NEXUS_CLANG_TIDY_EXE NAMES clang-tidy
    HINTS "$ENV{NEXUS_LLVM_BIN}" "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin" "$ENV{VCINSTALLDIR}/Tools/Llvm/bin")
  if(NOT NEXUS_CLANG_TIDY_EXE)
    message(FATAL_ERROR "NEXUS_ENABLE_CLANG_TIDY=ON but clang-tidy was not found")
  endif()
  message(STATUS "clang-tidy: ${NEXUS_CLANG_TIDY_EXE}")
  set(CMAKE_CXX_CLANG_TIDY "${NEXUS_CLANG_TIDY_EXE}")
  # Do not lint generated sources (moc, rcc, uic).
  set(CMAKE_AUTOGEN_ORIGIN_DEPENDS OFF)
endif()
