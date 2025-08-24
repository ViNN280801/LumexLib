# lumex/core/circular_buffer/CMakeLists.txt

set(LUMEX_CIRCULAR_BUFFER_NAME LumexCore_circular_buffer)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_CIRCULAR_BUFFER_NAME} INTERFACE)
add_library(Lumex::circular_buffer ALIAS ${LUMEX_CIRCULAR_BUFFER_NAME})

target_include_directories(${LUMEX_CIRCULAR_BUFFER_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS ${LUMEX_CIRCULAR_BUFFER_NAME} EXPORT ${EXPORT_TO})

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/circular_buffer
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "CircularBuffer"
)
