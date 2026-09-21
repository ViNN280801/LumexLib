# lumex/applied/settings/CMakeLists.txt

set(LUMEX_SETTINGS_NAME LumexApplied_settings)
set(EXPORT_TO LumexAppliedTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_SETTINGS_NAME} ${SOURCES})
add_library(lumex::settings ALIAS ${LUMEX_SETTINGS_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_SETTINGS_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_SETTINGS_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_SETTINGS_NAME} PUBLIC
    lumex::filesystem
    lumex::logging
    lumex::time
)

# XML settings is the reusable store, not the logger config reader. Link
# LumexXml only when that module is in this configure. INI still builds
# without XML; factory then returns nullptr for XML.
if(TARGET lumex::xml)
    target_link_libraries(${LUMEX_SETTINGS_NAME} PUBLIC lumex::xml)
    target_compile_definitions(${LUMEX_SETTINGS_NAME}
        PUBLIC LUMEX_SETTINGS_WITH_XML)
endif()

# JSON settings uses the same vendored nlohmann header as logger JSON and
# field reflection. The INTERFACE target is not exported. INI still builds
# without it; factory then returns nullptr for JSON.
if(EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/nlohmann/json.hpp")
    lumex_setup_nlohmann_json()
    target_link_libraries(${LUMEX_SETTINGS_NAME}
        PRIVATE nlohmann_json::nlohmann_json)
    target_compile_definitions(${LUMEX_SETTINGS_NAME}
        PUBLIC LUMEX_SETTINGS_WITH_JSON)
endif()

lumex_apply_library_versioning(${LUMEX_SETTINGS_NAME} "Lumex Settings Library")

install(TARGETS ${LUMEX_SETTINGS_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/applied/settings
    FILES_MATCHING 
        PATTERN "*.hpp"
        PATTERN "LumexSettings"
)
