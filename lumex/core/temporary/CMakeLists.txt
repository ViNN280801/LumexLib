# lumex/core/temporary/CMakeLists.txt

set(LUMEX_TEMPORARY_NAME LumexCore_temporary)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_TEMPORARY_NAME} LumexTemporary.cpp)
add_library(Lumex::temporary ALIAS ${LUMEX_TEMPORARY_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_TEMPORARY_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_TEMPORARY_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_TEMPORARY_NAME} PUBLIC
    Lumex::environment
    Lumex::filesystem
)

install(TARGETS ${LUMEX_TEMPORARY_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/temporary
    FILES_MATCHING PATTERN "*.hpp"
)
