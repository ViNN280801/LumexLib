# lumex/core/exceptions/CMakeLists.txt

set(LUMEX_EXCEPTIONS_NAME LumexCore_exceptions)
set(EXPORT_TO LumexCoreTargets)

file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Listed explicitly so the header still shows up as a target source in IDEs.
# LumexExceptionWrapper.cpp holds the process-wide report sink.
add_library(${LUMEX_EXCEPTIONS_NAME} ${SOURCES} LumexExceptionWrapper.hpp)
add_library(lumex::exceptions ALIAS ${LUMEX_EXCEPTIONS_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_EXCEPTIONS_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_EXCEPTIONS_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_EXCEPTIONS_NAME} PUBLIC
    lumex::environment
    lumex::filesystem
    lumex::string
    lumex::time
    lumex::utility
)

if(WIN32)
    target_link_libraries(${LUMEX_EXCEPTIONS_NAME} PRIVATE dbghelp)
elseif(UNIX AND NOT APPLE)
    find_library(EXECINFO_LIBRARY execinfo)

    if(EXECINFO_LIBRARY)
        target_link_libraries(${LUMEX_EXCEPTIONS_NAME} PRIVATE execinfo)
        message(STATUS "Linking with execinfo library: ${EXECINFO_LIBRARY}")
    else()
        message(STATUS "execinfo library not found - using built-in glibc backtrace functions")
    endif()
endif()

lumex_apply_library_versioning(${LUMEX_EXCEPTIONS_NAME} "Lumex Exceptions Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_EXCEPTIONS_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/exceptions
        FILES_MATCHING 
            PATTERN "*.hpp"
            PATTERN "LumexException"
    )
endif()
