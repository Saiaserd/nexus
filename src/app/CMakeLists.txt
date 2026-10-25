add_executable(nexus main.cpp)

target_link_libraries(nexus PRIVATE nexus_ui)

nexus_set_warnings(nexus)
nexus_enable_sanitizers(nexus)
nexus_deploy_qt(nexus)
