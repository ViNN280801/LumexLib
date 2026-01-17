# lumex/core/expected/CMakeLists.txt

set(LUMEX_EXPECTED_NAME LumexCore_expected)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_EXPECTED_NAME} INTERFACE)
add_library(Lumex::expected ALIAS ${LUMEX_EXPECTED_NAME})

target_include_directories(${LUMEX_EXPECTED_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS ${LUMEX_EXPECTED_NAME} EXPORT ${EXPORT_TO})

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/expected
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "Expected"
)
