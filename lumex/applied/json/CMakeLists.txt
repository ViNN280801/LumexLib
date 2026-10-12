# lumex/applied/json/CMakeLists.txt
#
# Header-only. The public API is built on nlohmann::json, so the module
# compiles against the consumer's own <nlohmann/json.hpp> and carries no
# nlohmann include directory: a compiled library would pin one nlohmann
# version (and its ABI namespace) on every consumer. Consumers put
# nlohmann/json 3.x on their include path (or link nlohmann_json). LumexLib's
# own tests and examples link the vendored 3rdparty/nlohmann copy.

set(LUMEX_JSON_NAME LumexApplied_json)
set(EXPORT_TO LumexAppliedTargets)

add_library(${LUMEX_JSON_NAME} INTERFACE)
add_library(lumex::json ALIAS ${LUMEX_JSON_NAME})

target_include_directories(${LUMEX_JSON_NAME} INTERFACE
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_JSON_NAME} INTERFACE
    lumex::reflection
    lumex::string_view
    lumex::utility
)

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_JSON_NAME} EXPORT ${EXPORT_TO})

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/applied/json
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "LumexJson"
    )
endif()
