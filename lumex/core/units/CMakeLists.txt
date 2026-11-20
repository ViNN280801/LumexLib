# lumex/core/units/CMakeLists.txt

set(LUMEX_UNITS_NAME LumexCore_units)
set(EXPORT_TO LumexCoreTargets)

# Header-only: the catalog is constexpr tables in data/ and the lookup and the
# conversions are inline functions in api/, so there is no library to build.
add_library(${LUMEX_UNITS_NAME} INTERFACE)
add_library(lumex::units ALIAS ${LUMEX_UNITS_NAME})

target_include_directories(${LUMEX_UNITS_NAME} INTERFACE
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# api/ takes and returns lumex_string_view and expected, the keyword and
# attribute macros come from utility, and the conversions of the degree, the
# minute and the second of arc use the constant pi of math.
target_link_libraries(${LUMEX_UNITS_NAME} INTERFACE
    lumex::expected
    lumex::math
    lumex::string_view
    lumex::utility
)

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_UNITS_NAME} EXPORT ${EXPORT_TO})

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/units
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "LumexUnits"
    )
endif()
