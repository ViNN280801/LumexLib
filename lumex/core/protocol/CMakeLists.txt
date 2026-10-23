# lumex/core/protocol/CMakeLists.txt

set(LUMEX_PROTOCOL_NAME LumexCore_protocol)
set(EXPORT_TO LumexCoreTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_PROTOCOL_NAME} ${SOURCES})
add_library(lumex::protocol ALIAS ${LUMEX_PROTOCOL_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_PROTOCOL_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_PROTOCOL_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# The table DSL is header-only; the error category is compiled here, so that
# an error code made in any module of a process compares equal to the
# enumerator (a category is compared by address). framing gives the byte
# order of the fields and the codec type of a protocol, span the views of the
# run-time entries, utility the keyword macros and the index sequences;
# reflection (enumeration fields), expected (results of encode and decode)
# and string_view are the requirements of the module in the design, declared
# once so that the module graph does not change with each part.
target_link_libraries(${LUMEX_PROTOCOL_NAME} PUBLIC
    lumex::expected
    lumex::framing
    lumex::reflection
    lumex::span
    lumex::string_view
    lumex::utility
)

# The library is compiled as C++11 on purpose, like core/framing: its one
# exported function does not depend on the standard.
set_target_properties(${LUMEX_PROTOCOL_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

lumex_apply_library_versioning(${LUMEX_PROTOCOL_NAME} "Lumex Protocol Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_PROTOCOL_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/protocol
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "LumexProtocol"
    )
endif()
