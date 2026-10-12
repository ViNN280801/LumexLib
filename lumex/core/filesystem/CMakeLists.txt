# lumex/core/filesystem/CMakeLists.txt

set(LUMEX_FILESYSTEM_NAME LumexCore_filesystem)
set(EXPORT_TO LumexCoreTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_FILESYSTEM_NAME} ${SOURCES})
add_library(lumex::filesystem ALIAS ${LUMEX_FILESYSTEM_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_FILESYSTEM_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_FILESYSTEM_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_FILESYSTEM_NAME} PUBLIC lumex::utility)

# The path conversions to and from wchar_t (to_wide_string, from_wide_string)
# are lumex::core::unicode::convert::to_wide / to_utf8, used by the sources
# only; unicode is header-only (an INTERFACE target for the include path and
# the configure-time edge).
target_link_libraries(${LUMEX_FILESYSTEM_NAME} PRIVATE lumex::unicode)

if(WIN32)
    target_link_libraries(${LUMEX_FILESYSTEM_NAME} PRIVATE shlwapi)
endif()

lumex_apply_library_versioning(${LUMEX_FILESYSTEM_NAME} "Lumex Filesystem Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_FILESYSTEM_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/filesystem
        FILES_MATCHING 
            PATTERN "*.hpp"
            PATTERN "LumexFilesystem"
    )
endif()
