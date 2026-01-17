# lumex/applied/logging/CMakeLists.txt

# Here is not LumexLogger, but simple Logger, because it transferred
# from another project and just kept this naming.

set(LUMEX_LOGGER_NAME LumexApplied_logger)
set(EXPORT_TO LumexAppliedTargets)

add_library(${LUMEX_LOGGER_NAME} Logger.cpp)
add_library(Lumex::logger ALIAS ${LUMEX_LOGGER_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_LOGGER_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_LOGGER_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link with dbghelp.lib on Windows for symbol resolution (SymGetLineFromAddr64, SymInitialize, etc.)
if(WIN32)
    target_link_libraries(${LUMEX_LOGGER_NAME} PRIVATE dbghelp)
endif()

lumex_apply_library_versioning(${LUMEX_LOGGER_NAME} "Lumex Logger Library")

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
        PATTERN "Logger"
)
