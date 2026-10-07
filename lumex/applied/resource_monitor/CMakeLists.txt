# lumex/applied/resource_monitor/CMakeLists.txt

set(LUMEX_RESOURCE_MONITOR_NAME LumexApplied_resource_monitor)
set(EXPORT_TO LumexAppliedTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_RESOURCE_MONITOR_NAME} ${SOURCES})
add_library(lumex::resource_monitor ALIAS ${LUMEX_RESOURCE_MONITOR_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_RESOURCE_MONITOR_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_RESOURCE_MONITOR_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_RESOURCE_MONITOR_NAME} PUBLIC
    lumex::expected
    lumex::filesystem
    lumex::logging
    lumex::optional
    lumex::string_view
    lumex::time
)

lumex_apply_library_versioning(${LUMEX_RESOURCE_MONITOR_NAME} "Lumex Resource Monitor Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_RESOURCE_MONITOR_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/applied/resource_monitor
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "LumexResourceMonitor"
    )
endif()
