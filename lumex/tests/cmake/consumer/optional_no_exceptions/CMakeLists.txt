# Consumer fixture for cmake.consumer_optional_no_exceptions_*.
#
# optional built without exceptions: value () of an empty optional calls
# std::terminate, in every overload, and the header has no throw left.
#
# Two ways to switch the exceptions off, one executable each case:
#   -DOPTIONAL_NO_EXCEPTIONS_MODE=compiler  the compiler has no exceptions: -fno-exceptions
#                                           (GCC, Clang) or /EHs-c- with
#                                           _HAS_EXCEPTIONS=0 (cl.exe and
#                                           clang-cl, the standard library
#                                           needs both); the library detects it
#   -DOPTIONAL_NO_EXCEPTIONS_MODE=macro     exceptions stay on and the program
#                                           defines LUMEX_OPTIONAL_HAS_EXCEPTIONS
#                                           as 0
# -DOPTIONAL_NO_EXCEPTIONS_STD=<11|17|20> sets the language standard.
# main.cpp runs the checks (a failure exits with 1) and starts itself again
# for each call that must terminate (fork on POSIX, std::system elsewhere).

cmake_minimum_required(VERSION 3.16)
project(LumexOptionalNoExceptions LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT OPTIONAL_NO_EXCEPTIONS_MODE)
    set(OPTIONAL_NO_EXCEPTIONS_MODE compiler)
endif()
if(NOT OPTIONAL_NO_EXCEPTIONS_STD)
    set(OPTIONAL_NO_EXCEPTIONS_STD 11)
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT FRAMING PROTOCOL UNITS GENERATORS INTERPROCESS REFLECTION STRING
        STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_OPTIONAL ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

add_executable(optional_no_exceptions_consumer main.cpp)
target_link_libraries(optional_no_exceptions_consumer PRIVATE lumex::optional)
set_target_properties(optional_no_exceptions_consumer PROPERTIES
    CXX_STANDARD ${OPTIONAL_NO_EXCEPTIONS_STD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
if(OPTIONAL_NO_EXCEPTIONS_MODE STREQUAL "compiler")
    if(MSVC)
        # cl.exe and clang-cl: the default /EHsc of CMake comes first, the later
        # option wins (clang-cl ignores -fno-exceptions). The standard library
        # needs _HAS_EXCEPTIONS=0 to compile without them.
        target_compile_options(optional_no_exceptions_consumer PRIVATE /EHs-c-)
        target_compile_definitions(optional_no_exceptions_consumer PRIVATE
            _HAS_EXCEPTIONS=0)
    else()
        target_compile_options(optional_no_exceptions_consumer PRIVATE -fno-exceptions)
    endif()
    target_compile_definitions(optional_no_exceptions_consumer PRIVATE
        OPTIONAL_NO_EXCEPTIONS_COMPILER=1)
elseif(OPTIONAL_NO_EXCEPTIONS_MODE STREQUAL "macro")
    target_compile_definitions(optional_no_exceptions_consumer PRIVATE
        LUMEX_OPTIONAL_HAS_EXCEPTIONS=0 OPTIONAL_NO_EXCEPTIONS_MACRO=1)
else()
    message(FATAL_ERROR "OPTIONAL_NO_EXCEPTIONS_MODE: compiler or macro")
endif()
