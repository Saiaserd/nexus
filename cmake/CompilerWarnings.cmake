# nexus_set_warnings(<target>)
#
# Strict warnings, treated as errors, applied ONLY to the given target (PRIVATE).
# Third-party code is not affected: Qt and vcpkg packages come in as IMPORTED
# targets, whose include directories CMake passes as SYSTEM includes
# (-isystem / /external:I). For MSVC we additionally silence warnings coming
# from those external headers with /external:W0.
function(nexus_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4
      /WX
      /permissive-
      /external:W0)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Werror)
  else()
    message(WARNING "nexus_set_warnings: unknown compiler '${CMAKE_CXX_COMPILER_ID}'")
  endif()
endfunction()
