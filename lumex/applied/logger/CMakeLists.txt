# lumex/applied/logger/CMakeLists.txt

set(LUMEX_LOGGER_NAME LumexApplied_logger)
set(EXPORT_TO LumexAppliedTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_LOGGER_NAME} ${SOURCES})
add_library(lumex::logger ALIAS ${LUMEX_LOGGER_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_LOGGER_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_LOGGER_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Exactly one reader is compiled into the logger. The definition is PUBLIC so
# consumers, tests, and examples see the same compile-time format.
if(LUMEX_LOGGER_CONFIG_FORMAT_NORMALIZED STREQUAL "PLAIN_TEXT")
    target_compile_definitions(${LUMEX_LOGGER_NAME}
        PUBLIC LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT)
elseif(LUMEX_LOGGER_CONFIG_FORMAT_NORMALIZED STREQUAL "INI")
    if(NOT TARGET lumex::settings)
        message(FATAL_ERROR
            "LUMEX_LOGGER_CONFIG_FORMAT=INI but lumex::settings is missing")
    endif()
    target_compile_definitions(${LUMEX_LOGGER_NAME}
        PUBLIC LUMEX_LOGGER_CONFIG_FORMAT_INI)
    target_link_libraries(${LUMEX_LOGGER_NAME} PRIVATE lumex::settings)
elseif(LUMEX_LOGGER_CONFIG_FORMAT_NORMALIZED STREQUAL "JSON")
    lumex_setup_nlohmann_json()
    target_compile_definitions(${LUMEX_LOGGER_NAME}
        PUBLIC LUMEX_LOGGER_CONFIG_FORMAT_JSON)
    target_link_libraries(${LUMEX_LOGGER_NAME}
        PRIVATE nlohmann_json::nlohmann_json)
elseif(LUMEX_LOGGER_CONFIG_FORMAT_NORMALIZED STREQUAL "YAML")
    target_compile_definitions(${LUMEX_LOGGER_NAME}
        PUBLIC LUMEX_LOGGER_CONFIG_FORMAT_YAML)
elseif(LUMEX_LOGGER_CONFIG_FORMAT_NORMALIZED STREQUAL "XML")
    if(NOT TARGET lumex::xml)
        message(FATAL_ERROR
            "LUMEX_LOGGER_CONFIG_FORMAT=XML but lumex::xml is missing")
    endif()
    target_compile_definitions(${LUMEX_LOGGER_NAME}
        PUBLIC LUMEX_LOGGER_CONFIG_FORMAT_XML)
    target_link_libraries(${LUMEX_LOGGER_NAME} PRIVATE lumex::xml)
else()
    message(FATAL_ERROR
        "Internal error: unsupported normalized logger config format "
        "'${LUMEX_LOGGER_CONFIG_FORMAT_NORMALIZED}'")
endif()

# Link with dbghelp.lib on Windows for symbol resolution (SymGetLineFromAddr64, SymInitialize, etc.)
if(WIN32)
    target_link_libraries(${LUMEX_LOGGER_NAME} PRIVATE dbghelp)
endif()

lumex_apply_library_versioning(${LUMEX_LOGGER_NAME} "Lumex Logger Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_LOGGER_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/applied/logger
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "LumexLogger"
    )
endif()
