# lumex/core/string/CMakeLists.txt

set(LUMEX_STRING_NAME LumexCore_string)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_STRING_NAME} INTERFACE)
add_library(Lumex::string ALIAS ${LUMEX_STRING_NAME})

target_include_directories(${LUMEX_STRING_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS ${LUMEX_STRING_NAME} EXPORT ${EXPORT_TO})

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/string
    FILES_MATCHING PATTERN "*.hpp"
)
