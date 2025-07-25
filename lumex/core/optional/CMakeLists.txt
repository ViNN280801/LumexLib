# lumex/core/optional/CMakeLists.txt

set(LUMEX_OPTIONAL_NAME LumexCore_optional)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_OPTIONAL_NAME} INTERFACE)
add_library(Lumex::optional ALIAS ${LUMEX_OPTIONAL_NAME})

target_include_directories(${LUMEX_OPTIONAL_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS ${LUMEX_OPTIONAL_NAME} EXPORT ${EXPORT_TO})

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/optional
    FILES_MATCHING PATTERN "*.hpp"
)
