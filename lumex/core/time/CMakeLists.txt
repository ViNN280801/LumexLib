# lumex/core/time/CMakeLists.txt

set(LUMEX_TIME_NAME LumexCore_time)
set(EXPORT_TO LumexCoreTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_TIME_NAME} ${SOURCES})
add_library(lumex::time ALIAS ${LUMEX_TIME_NAME})

if(WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(${LUMEX_TIME_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_TIME_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_TIME_NAME} PUBLIC
    lumex::environment
)

lumex_apply_library_versioning(${LUMEX_TIME_NAME} "Lumex Time Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_TIME_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/time
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "LumexTime"
    )
endif()
