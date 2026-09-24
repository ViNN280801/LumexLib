# lumex/core/reflection/CMakeLists.txt

set(LUMEX_REFLECTION_NAME LumexCore_reflection)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_REFLECTION_NAME} INTERFACE)
add_library(lumex::reflection ALIAS ${LUMEX_REFLECTION_NAME})

target_include_directories(${LUMEX_REFLECTION_NAME} INTERFACE
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_REFLECTION_NAME} INTERFACE
    lumex::utility
)

# field_reflection/ uses vendored nlohmann/json and LumexAggregateFields
# (no Boost.PFR). Pulled in only when LUMEX_WITH_FIELD_REFLECTION is ON.
if(LUMEX_WITH_FIELD_REFLECTION)
    lumex_setup_nlohmann_json()
    target_include_directories(${LUMEX_REFLECTION_NAME} INTERFACE
        $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}/3rdparty>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(${LUMEX_REFLECTION_NAME} INTERFACE LUMEX_WITH_FIELD_REFLECTION)
    message(STATUS "Building with LumexFieldReflection (nlohmann/json) enabled")
endif()

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_REFLECTION_NAME} EXPORT ${EXPORT_TO})

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/reflection
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "LumexReflection"
    )
endif()

if(LUMEX_INSTALL AND LUMEX_WITH_FIELD_REFLECTION)
    install(
        DIRECTORY ${LumexLib_SOURCE_DIR}/3rdparty/nlohmann
        DESTINATION include
    )
endif()
