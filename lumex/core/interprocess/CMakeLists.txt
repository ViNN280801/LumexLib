# lumex/core/interprocess/CMakeLists.txt

set(LUMEX_INTERPROCESS_NAME LumexCore_interprocess)
set(EXPORT_TO LumexCoreTargets)

# Every module globs its sources. This one keeps the code of each operating
# system in a file of its own (lock/LumexInterprocessLockWindows.cpp and
# lock/LumexInterprocessLockPosix.cpp) and compiles only the file of the
# target: a file guarded to nothing on the other system would leave an empty
# object, which a static library build of MSVC reports (LNK4221). Each of the
# two files stops with #error when it is compiled on the wrong system.
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
if(WIN32)
    list(FILTER SOURCES EXCLUDE REGEX "Posix\\.cpp$")
else()
    list(FILTER SOURCES EXCLUDE REGEX "Windows\\.cpp$")
endif()
add_library(${LUMEX_INTERPROCESS_NAME} ${SOURCES})
add_library(lumex::interprocess ALIAS ${LUMEX_INTERPROCESS_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_INTERPROCESS_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_INTERPROCESS_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# expected is the result of every operation, filesystem gives the path of the
# lock file and creates its directory, reflection declares the error kind
# (LUMEX_DEFINE_REFLECTED_ENUM, header-only), utility gives the keyword macros.
target_link_libraries(${LUMEX_INTERPROCESS_NAME} PUBLIC
    lumex::expected
    lumex::filesystem
    lumex::reflection
    lumex::utility
)

# The library is compiled as C++11 on purpose, like core/framing: its exported
# class does not depend on the standard of the program that uses it.
set_target_properties(${LUMEX_INTERPROCESS_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

lumex_apply_library_versioning(${LUMEX_INTERPROCESS_NAME} "Lumex Interprocess Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_INTERPROCESS_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/interprocess
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "LumexInterprocess"
    )
endif()
