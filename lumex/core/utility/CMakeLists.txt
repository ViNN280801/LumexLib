# lumex/core/utility/CMakeLists.txt

set(LUMEX_UTILITY_NAME LumexCore_utility)
set(EXPORT_TO LumexCoreTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_UTILITY_NAME} ${SOURCES})
add_library(lumex::utility ALIAS ${LUMEX_UTILITY_NAME})

if(WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(${LUMEX_UTILITY_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_UTILITY_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Public headers call into two system libraries from inline code: the dump
# header uses std::thread and pthread_sigmask, the debug header resolves
# symbol names with dladdr. Before glibc 2.34 those live in libpthread and
# libdl, so every consumer of the headers needs both on its link line
# (CMAKE_DL_LIBS is empty where dladdr is part of libc).
find_package(Threads REQUIRED)

target_link_libraries(${LUMEX_UTILITY_NAME} PUBLIC
    lumex::math
    lumex::span
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

# The dump header links dbghelp and shlwapi with #pragma comment(lib), which
# MSVC and clang-cl honour and GCC ignores. MinGW names them here, PUBLIC
# because the inline code of the header calls them in every consumer.
if(MINGW)
    target_link_libraries(${LUMEX_UTILITY_NAME} PUBLIC dbghelp shlwapi)
endif()

lumex_apply_library_versioning(${LUMEX_UTILITY_NAME} "Lumex Utility Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_UTILITY_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/utility
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "LumexUtility"
    )
endif()
