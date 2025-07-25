# lumex/core/filesystem/CMakeLists.txt

set(LUMEX_FILESYSTEM_NAME LumexCore_filesystem)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_FILESYSTEM_NAME} LumexFilesystem.cpp)
add_library(Lumex::filesystem ALIAS ${LUMEX_FILESYSTEM_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_FILESYSTEM_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_FILESYSTEM_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_FILESYSTEM_NAME} PUBLIC Lumex::utility)

if(WIN32)
    target_link_libraries(${LUMEX_FILESYSTEM_NAME} PRIVATE shlwapi)
endif()

install(TARGETS ${LUMEX_FILESYSTEM_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/filesystem
    FILES_MATCHING PATTERN "*.hpp"
)
