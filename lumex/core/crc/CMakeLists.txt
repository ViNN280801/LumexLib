# lumex/core/crc/CMakeLists.txt

set(LUMEX_CRC_NAME LumexCore_crc)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_CRC_NAME} LumexCrc.cpp)
add_library(Lumex::crc ALIAS ${LUMEX_CRC_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_CRC_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_CRC_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_CRC_NAME} PUBLIC
    Lumex::utility
)

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
