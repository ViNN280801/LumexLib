# lumex/applied/CMakeLists.txt
#
# logging before hardware / resource_monitor / settings: those modules
# target_link_libraries lumex::logging (ALIAS must already exist).

lumex_add_subdirectory_if(LUMEX_BUILD_LOGGING logging)
lumex_add_subdirectory_if(LUMEX_BUILD_HARDWARE hardware)
lumex_add_subdirectory_if(LUMEX_BUILD_JSON json)
# Settings precedes logger because LUMEX_LOGGER_CONFIG_FORMAT=INI makes the
# logger PRIVATE-link lumex::settings at configure time.
lumex_add_subdirectory_if(LUMEX_BUILD_SETTINGS settings)
lumex_add_subdirectory_if(LUMEX_BUILD_LOGGER logger)
lumex_add_subdirectory_if(LUMEX_BUILD_RESOURCE_MONITOR resource_monitor)
lumex_add_subdirectory_if(LUMEX_BUILD_SERIAL serial)
