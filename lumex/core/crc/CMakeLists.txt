# lumex/core/crc/CMakeLists.txt

set(LUMEX_CRC_NAME LumexCore_crc)
set(EXPORT_TO LumexCoreTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_CRC_NAME} ${SOURCES})
add_library(lumex::crc ALIAS ${LUMEX_CRC_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_CRC_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_CRC_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_CRC_NAME} PUBLIC
    lumex::utility
)

# Catalogue tables use std::make_index_sequence (C++14).
set_target_properties(${LUMEX_CRC_NAME} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

lumex_apply_library_versioning(${LUMEX_CRC_NAME} "Lumex CRC Library")

install(TARGETS ${LUMEX_CRC_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/crc
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "LumexCrc"
)
