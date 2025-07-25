# lumex/core/math/CMakeLists.txt

set(LUMEX_MATH_NAME LumexCore_math)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_MATH_NAME} INTERFACE)
add_library(Lumex::math ALIAS ${LUMEX_MATH_NAME})

target_include_directories(${LUMEX_MATH_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS ${LUMEX_MATH_NAME} EXPORT ${EXPORT_TO})

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/math
    FILES_MATCHING PATTERN "*.hpp"
)
