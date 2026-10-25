# Parser layer: Markdown lexing for syntax highlighting (QtCore only, no Widgets).
add_library(nexus_parser STATIC
  MarkdownLexer.h
  MarkdownLexer.cpp)

# Headers are included as "parser/MarkdownLexer.h" -> expose the src/ directory.
target_include_directories(nexus_parser PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(nexus_parser PUBLIC Qt6::Core)

nexus_set_warnings(nexus_parser)
nexus_enable_sanitizers(nexus_parser)
