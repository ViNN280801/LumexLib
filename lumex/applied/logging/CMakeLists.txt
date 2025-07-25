# lumex/applied/logging/CMakeLists.txt

set(LUMEX_LOGGING_NAME LumexApplied_logging)
set(EXPORT_TO LumexAppliedTargets)

add_library(${LUMEX_LOGGING_NAME} LumexLogging.cpp)
add_library(Lumex::logging ALIAS ${LUMEX_LOGGING_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_LOGGING_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_LOGGING_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_LOGGING_NAME} PUBLIC
    Lumex::environment
    Lumex::filesystem
    Lumex::string
    Lumex::time
)

install(TARGETS ${LUMEX_LOGGING_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/applied/logging
    FILES_MATCHING PATTERN "*.hpp"
)
