# lumex/core/framing/CMakeLists.txt

set(LUMEX_FRAMING_NAME LumexCore_framing)
set(EXPORT_TO LumexCoreTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_FRAMING_NAME} ${SOURCES})
add_library(lumex::framing ALIAS ${LUMEX_FRAMING_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_FRAMING_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_FRAMING_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# The codecs are header-only; the error category is compiled here, so that an
# error code made in any module of a process compares equal to the enumerator
# (a category is compared by address). contracts checks the codec options,
# crc computes the checksums of the stages, expected is the result of encode,
# span is the byte view of the interface, utility gives the keyword macros.
target_link_libraries(${LUMEX_FRAMING_NAME} PUBLIC
    lumex::contracts
    lumex::crc
    lumex::expected
    lumex::span
    lumex::utility
)

# The library is compiled as C++11 on purpose, like core/contracts: its one
# exported function does not depend on the standard.
set_target_properties(${LUMEX_FRAMING_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

lumex_apply_library_versioning(${LUMEX_FRAMING_NAME} "Lumex Framing Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_FRAMING_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/framing
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "LumexFraming"
    )
endif()
