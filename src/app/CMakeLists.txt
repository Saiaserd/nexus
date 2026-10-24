add_executable(nexus main.cpp)

target_link_libraries(nexus PRIVATE Qt6::Core Qt6::Widgets)

nexus_set_warnings(nexus)
nexus_enable_sanitizers(nexus)
nexus_deploy_qt(nexus)
