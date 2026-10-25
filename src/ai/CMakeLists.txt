# AI layer: Claude Code CLI bridge (QtCore only, no Widgets).
find_package(tl-expected CONFIG REQUIRED)

add_library(nexus_ai STATIC
  AgentEvent.h
  StreamJsonParser.h
  StreamJsonParser.cpp
  AgentRun.h
  AgentRun.cpp)

# Headers are included as "ai/AgentRun.h" -> expose the src/ directory.
target_include_directories(nexus_ai PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(nexus_ai PUBLIC tl::expected Qt6::Core)

nexus_set_warnings(nexus_ai)
nexus_enable_sanitizers(nexus_ai)
