# Consumer fixture for cmake.consumer_string_view_no_exceptions_* and
# cmake.consumer_string_view_exceptions_*.
#
# The string views are header-only and must compile with -fno-exceptions (GCC,
# Clang): the calls that throw std::out_of_range (at, substr, copy and the
# compare overloads with a position) call std::terminate, and operator<<
# catches nothing. One executable per case:
#   -DSV_EXCEPTIONS=OFF  built with -fno-exceptions; main.cpp uses the view,
#                        operator<< with padding included, and (POSIX) forks a
#                        child for each call that must terminate
#   -DSV_EXCEPTIONS=ON   built normally; main.cpp checks that the same calls
#                        throw std::out_of_range
# -DSV_STD=<11|17|20> sets the language standard. Only the include directory of
# the checkout is needed.

cmake_minimum_required(VERSION 3.16)
project(LumexStringViewNoExceptions LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT SV_STD)
    set(SV_STD 11)
endif()
if(NOT DEFINED SV_EXCEPTIONS)
    set(SV_EXCEPTIONS OFF)
endif()
if(MSVC AND NOT SV_EXCEPTIONS)
    message("LUMEX_CONSUMER_SKIP: MSVC has no -fno-exceptions")
    return()
endif()

add_executable(string_view_no_exceptions_consumer main.cpp)
target_include_directories(string_view_no_exceptions_consumer PRIVATE "${LUMEX_SOURCE_DIR}")
set_target_properties(string_view_no_exceptions_consumer PROPERTIES
    CXX_STANDARD ${SV_STD} CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(SV_EXCEPTIONS)
    target_compile_definitions(string_view_no_exceptions_consumer PRIVATE SV_WITH_EXCEPTIONS=1)
else()
    target_compile_options(string_view_no_exceptions_consumer PRIVATE -fno-exceptions)
endif()
