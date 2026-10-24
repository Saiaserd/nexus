# nexus_deploy_qt(<target>)
#
# Windows only: after building <target>, run windeployqt so the Qt DLLs and
# plugins sit next to the executable. This makes the binary runnable from
# anywhere (Explorer, ctest, IDEs) without editing PATH. No-op on other
# platforms, where Qt is found through rpath / the system loader.
function(nexus_deploy_qt target)
  if(NOT WIN32)
    return()
  endif()
  if(NOT TARGET Qt6::windeployqt)
    message(WARNING "Qt6::windeployqt not found; ${target} will need Qt on PATH")
    return()
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND Qt6::windeployqt
            --no-translations --no-compiler-runtime --no-system-d3d-compiler
            --no-opengl-sw
            $<TARGET_FILE:${target}>
    COMMENT "windeployqt: deploying Qt runtime for ${target}"
    VERBATIM)
endfunction()
