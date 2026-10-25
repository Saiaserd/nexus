# Sanitizer support, driven by the NEXUS_ENABLE_ASAN option (set by the `asan`
# preset).
#
#   Linux (GCC/Clang): -fsanitize=address,undefined
#   Windows (MSVC)   : /fsanitize=address  (UBSan is not available on MSVC)
#
# Usage: include(Sanitizers) once, then nexus_enable_sanitizers(<target>) for
# every target we build (libraries and executables).

option(NEXUS_ENABLE_ASAN "Build with AddressSanitizer (+ UBSan on GCC/Clang)" OFF)

if(NEXUS_ENABLE_ASAN AND MSVC)
  # MSVC ASan is incompatible with /RTC* (runtime checks) and incremental
  # linking. Strip them from the default flags of every configuration.
  foreach(_cfg "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
    string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_CXX_FLAGS${_cfg} "${CMAKE_CXX_FLAGS${_cfg}}")
    string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_C_FLAGS${_cfg} "${CMAKE_C_FLAGS${_cfg}}")
  endforeach()
  foreach(_cfg _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
    foreach(_kind EXE SHARED MODULE)
      string(REGEX REPLACE "/INCREMENTAL(:[A-Za-z]+)?" ""
             CMAKE_${_kind}_LINKER_FLAGS${_cfg} "${CMAKE_${_kind}_LINKER_FLAGS${_cfg}}")
      string(APPEND CMAKE_${_kind}_LINKER_FLAGS${_cfg} " /INCREMENTAL:NO")
    endforeach()
  endforeach()
endif()

function(nexus_enable_sanitizers target)
  if(NOT NEXUS_ENABLE_ASAN)
    return()
  endif()

  if(MSVC)
    target_compile_options(${target} PRIVATE /fsanitize=address)
    # The vcpkg static libraries (gtest, ...) are built WITHOUT ASan. MSVC's STL
    # container annotations differ between ASan and non-ASan objects, and linking
    # them together fails with LNK2038 (annotate_vector / annotate_string). Disable
    # the annotations in our own objects so both agree. Trade-off: no
    # container-overflow detection inside std::vector / std::string (heap and stack
    # overflows are still caught).
    target_compile_definitions(${target} PRIVATE
      _DISABLE_VECTOR_ANNOTATION
      _DISABLE_STRING_ANNOTATION)
    # The dynamic ASan runtime ships next to cl.exe (and is on PATH inside a
    # VS developer prompt). Copy it beside executables so they also run from
    # Explorer / ctest without relying on PATH. The debug CRT (/MDd) uses the
    # *_dbg_* variant, the release CRT the plain one.
    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "EXECUTABLE")
      get_filename_component(_cl_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
      set(_asan_dlls
        "${_cl_dir}/clang_rt.asan_dynamic-x86_64.dll"
        "${_cl_dir}/clang_rt.asan_dbg_dynamic-x86_64.dll")
      foreach(_dll IN LISTS _asan_dlls)
        if(EXISTS "${_dll}")
          add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
        else()
          message(WARNING "ASan runtime not found: ${_dll}")
        endif()
      endforeach()
    endif()
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined
      -fno-sanitize-recover=all
      -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  else()
    message(WARNING "nexus_enable_sanitizers: unsupported compiler '${CMAKE_CXX_COMPILER_ID}'")
  endif()
endfunction()
