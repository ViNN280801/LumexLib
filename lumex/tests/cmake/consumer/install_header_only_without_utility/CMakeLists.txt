# Fixture of cmake.install_header_only_without_utility (see main.cpp).
#
# While configuring, installs LumexLib into a prefix under this build tree
# with LUMEX_BUILD_UTILITY OFF, every module that requires utility OFF and the
# five header-only modules (string, math, generators, optional, atomic) ON.
# Then finds that prefix with find_package(LumexLib) and builds main.cpp
# against the installed targets only: at C++11 (the executable that runs) and,
# when the compiler has it, at C++20.
#
# The five modules are INTERFACE targets, so the nested LumexLib tree is
# configured and installed without a build: building it would also run its
# ALL targets that write into the LumexLib checkout (publish_distr,
# lumex_copy_compile_commands).

cmake_minimum_required(VERSION 3.16)
project(LumexInstallHeaderOnlyWithoutUtility LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(_header_only_modules ATOMIC GENERATORS MATH OPTIONAL STRING)
# Every other core module: UTILITY itself, and the modules that require it
# directly or through another module (cmake/LumexModules.cmake). The applied
# and XML groups are switched off as a whole.
set(_other_core_modules
    BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM FMT
    REFLECTION STRING_VIEW TEMPORARY TIME UTILITY)

set(_lumex_build "${CMAKE_BINARY_DIR}/lumexlib-build")
set(_lumex_prefix "${CMAKE_BINARY_DIR}/lumexlib-prefix")
file(REMOVE_RECURSE "${_lumex_build}" "${_lumex_prefix}")

set(_lumex_args
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_INSTALL_PREFIX=${_lumex_prefix}"
    -DLUMEX_INSTALL=ON
    -DLUMEX_BUILD_TESTS=OFF
    -DLUMEX_BUILD_EXAMPLES=OFF
    -DLUMEX_BUILD_BENCHMARKS=OFF
    -DLUMEX_BUILD_DOCUMENTATION=OFF
    -DLUMEX_BUILD_CORE=ON
    -DLUMEX_BUILD_APPLIED=OFF
    -DLUMEX_BUILD_XML=OFF)
foreach(_module IN LISTS _header_only_modules)
    list(APPEND _lumex_args -DLUMEX_BUILD_${_module}=ON)
endforeach()
foreach(_module IN LISTS _other_core_modules)
    list(APPEND _lumex_args -DLUMEX_BUILD_${_module}=OFF)
endforeach()
if(NOT "${CMAKE_CXX_FLAGS}" STREQUAL "")
    list(APPEND _lumex_args "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
endif()
if(NOT "${LUMEX_CLANG_STDLIB}" STREQUAL "")
    list(APPEND _lumex_args "-DLUMEX_CLANG_STDLIB=${LUMEX_CLANG_STDLIB}")
endif()
if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND _lumex_args -A "${CMAKE_GENERATOR_PLATFORM}")
endif()
if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND _lumex_args -T "${CMAKE_GENERATOR_TOOLSET}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${LUMEX_SOURCE_DIR}" -B "${_lumex_build}" -G "${CMAKE_GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        ${_lumex_args}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "LumexLib configure failed (${_rc})\n${_out}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${_lumex_build}" --config Release
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "LumexLib install failed (${_rc})\n${_out}")
endif()

# Only the prefix: a LumexLib installed elsewhere on this machine must not
# satisfy the search.
find_package(LumexLib REQUIRED PATHS "${_lumex_prefix}" NO_DEFAULT_PATH)

# The prefix is the slice this case is about: no utility module in it.
if(TARGET lumex::utility OR EXISTS "${_lumex_prefix}/include/lumex/core/utility/LumexUtility")
    message(FATAL_ERROR "the prefix contains the utility module; the case needs it absent")
endif()

set(_targets
    lumex::atomic lumex::math lumex::number_generator lumex::optional lumex::string)
foreach(_target IN LISTS _targets)
    if(NOT TARGET ${_target})
        message(FATAL_ERROR "find_package(LumexLib) did not provide ${_target}")
    endif()
endforeach()

add_executable(install_header_only_without_utility_consumer main.cpp)
target_link_libraries(install_header_only_without_utility_consumer PRIVATE ${_targets})
set_target_properties(install_header_only_without_utility_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    add_executable(install_header_only_without_utility_cxx20 main.cpp)
    target_link_libraries(install_header_only_without_utility_cxx20 PRIVATE ${_targets})
    set_target_properties(install_header_only_without_utility_cxx20 PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endif()
