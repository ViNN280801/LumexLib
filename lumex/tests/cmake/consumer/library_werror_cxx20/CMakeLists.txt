# Fixture of cmake.consumer_library_werror_cxx20 (see main.cpp).
#
# Builds the libraries of every module the way create_release.sh does: a
# global CMAKE_CXX_STANDARD of 20 and LUMEX_WERROR ON. A library target that
# has a CXX_STANDARD gets the strict set of maximum_standard_compliance
# (-Wswitch-default and -Wswitch-enum among it, cmake/LumexBuild.cmake), and
# only then: the trees of the scoped runs pin no standard on a library target,
# so a switch without a default, or one that does not name every enumerator,
# in a library source passed all of them and failed the release script. One
# run here compiles every library source under that set.

cmake_minimum_required(VERSION 3.16)
project(LumexLibraryWerrorCxx20 LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(LUMEX_INSTALL OFF CACHE BOOL "" FORCE)
set(LUMEX_WERROR ON CACHE BOOL "" FORCE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

# Every library is built by the default target; the executable only has to
# link, and it names the transport, the largest library, so that a skipped
# module cannot hide a failed build.
add_executable(library_werror_cxx20_consumer main.cpp)
if(TARGET lumex::transport)
    target_link_libraries(library_werror_cxx20_consumer PRIVATE lumex::transport)
endif()
