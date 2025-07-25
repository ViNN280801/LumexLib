# lumex/core/generators/number_generator/CMakeLists.txt

set(LUMEX_NUMBER_GENERATOR_NAME LumexCore_number_generator)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_NUMBER_GENERATOR_NAME} INTERFACE)
add_library(Lumex::number_generator ALIAS ${LUMEX_NUMBER_GENERATOR_NAME})

target_include_directories(${LUMEX_NUMBER_GENERATOR_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS ${LUMEX_NUMBER_GENERATOR_NAME} EXPORT ${EXPORT_TO})

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/generators/number_generator
    FILES_MATCHING PATTERN "*.hpp"
)
