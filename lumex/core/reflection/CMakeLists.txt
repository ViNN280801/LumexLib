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

# field_reflection/ is header-only and includes <nlohmann/json.hpp>.
# Do not put 3rdparty on this INTERFACE target: that include directory
# would shadow a consumer's own nlohmann/json.hpp in every translation
# unit that links lumex::reflection. The vendored header stays private
# to Lumex TUs that explicitly link nlohmann_json::nlohmann_json
# (field-reflection tests). Logger and settings already link it PRIVATE.
if(LUMEX_WITH_FIELD_REFLECTION)
    lumex_setup_nlohmann_json()
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
