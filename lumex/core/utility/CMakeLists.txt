# lumex/core/utility/CMakeLists.txt

set(LUMEX_UTILITY_NAME LumexCore_utility)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_UTILITY_NAME} INTERFACE)
add_library(Lumex::utility ALIAS ${LUMEX_UTILITY_NAME})

target_include_directories(${LUMEX_UTILITY_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS ${LUMEX_UTILITY_NAME} EXPORT ${EXPORT_TO})

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/utility
    FILES_MATCHING PATTERN "*.hpp"
)
