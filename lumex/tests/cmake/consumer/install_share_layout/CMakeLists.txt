# Fixture of cmake.install_share_layout_header_only and
# cmake.install_share_layout_all_modules (see main.cpp).
#
# While configuring, installs LumexLib into a prefix under this build tree and
# checks the layout of the package: the examples and the benchmarks go to
# share/examples and share/benchmarks as sources, per module, with the
# helpers they include, and no example binary is installed. Then it builds the
# installed source directories as projects of their own against that prefix
# (-DLumexLib_DIR=<prefix>/lib/cmake/LumexLib), the way a consumer does.
#
# LUMEX_INSTALL_CASE_PROFILE:
#   header_only  the header-only modules: nothing is compiled for the
#                library, every installed example directory is built and run,
#                and the span benchmark is built. The documentation is asked
#                for with a finished tree (LUMEX_DOCUMENTATION_PREBUILT_DIR, a
#                few files standing for the Doxygen output): it has to land in
#                share/doc/html, without the files the install leaves out;
#   all_modules  every module, the library built and installed: every
#                installed example and benchmark directory is configured, and
#                the ones that carry the pre-existing defects of the install
#                (the protocol examples with their helper headers, xml and
#                settings, which need the Xml targets before the Applied ones)
#                are built. find_package(LumexLib) of the whole package has to
#                work.
#
# The nested LumexLib is not built whole: the ALL targets publish_distr and
# lumex_copy_compile_commands write into the LumexLib checkout. The library
# targets are named one by one instead.

cmake_minimum_required(VERSION 3.16)
project(InstallShareLayout LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT LUMEX_INSTALL_CASE_PROFILE)
    message(FATAL_ERROR "pass -DLUMEX_INSTALL_CASE_PROFILE=header_only|all_modules")
endif()

include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")

set(_lumex_build "${CMAKE_BINARY_DIR}/lumexlib-build")
set(_lumex_prefix "${CMAKE_BINARY_DIR}/lumexlib-prefix")
set(_lumex_dir "${_lumex_prefix}/lib/cmake/LumexLib")
file(REMOVE_RECURSE "${_lumex_build}" "${_lumex_prefix}")

# _lumex_run(<what> <command>...): runs the command, stops with the tail of
# its output when it fails. The output of the last run is in _lumex_out.
function(_lumex_run what)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _out
    )
    set(_lumex_out "${_out}" PARENT_SCOPE)
    if(NOT _rc EQUAL 0)
        string(LENGTH "${_out}" _length)
        if(_length GREATER 6000)
            math(EXPR _from "${_length} - 6000")
            string(SUBSTRING "${_out}" ${_from} -1 _out)
        endif()
        message(FATAL_ERROR "${what} failed (${_rc}):\n${_out}")
    endif()
endfunction()

function(_lumex_require_path path what)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "${what}: ${path} is missing from the package")
    endif()
endfunction()

function(_lumex_require_no_path path what)
    if(EXISTS "${path}")
        message(FATAL_ERROR "${what}: ${path} must not be in the package")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# The nested LumexLib: its modules, the install into the prefix.
# ---------------------------------------------------------------------------
set(_lumex_args
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_INSTALL_PREFIX=${_lumex_prefix}"
    -DCMAKE_INSTALL_LIBDIR=lib
    -DLUMEX_INSTALL=ON
    -DLUMEX_BUILD_TESTS=OFF
    -DLUMEX_BUILD_EXAMPLES=OFF
    -DLUMEX_BUILD_BENCHMARKS=OFF
    -DLUMEX_BUILD_DOCUMENTATION=OFF
    -DLUMEX_GENERATE_BUILD_INFO=OFF)
if(LUMEX_INSTALL_CASE_PROFILE STREQUAL "header_only")
    set(_on_modules MATH OPTIONAL SPAN STRING STRING_VIEW GENERATORS)
    list(APPEND _lumex_args -DLUMEX_BUILD_CORE=ON -DLUMEX_BUILD_APPLIED=OFF
        -DLUMEX_BUILD_XML=OFF)
    foreach(_option IN LISTS LUMEX_CORE_MODULE_OPTIONS)
        string(REGEX REPLACE "^LUMEX_BUILD_" "" _name "${_option}")
        if(_name IN_LIST _on_modules)
            list(APPEND _lumex_args "-D${_option}=ON")
        else()
            list(APPEND _lumex_args "-D${_option}=OFF")
        endif()
    endforeach()
    # A finished documentation tree, as the release script makes one.
    set(_documentation_dir "${CMAKE_BINARY_DIR}/documentation")
    file(REMOVE_RECURSE "${_documentation_dir}")
    file(WRITE "${_documentation_dir}/html/index.html" "<html></html>\n")
    file(WRITE "${_documentation_dir}/html/a00001.html" "<html></html>\n")
    file(WRITE "${_documentation_dir}/html/a00001.map" "map\n")
    file(WRITE "${_documentation_dir}/html/a00001.md5" "md5\n")
    list(APPEND _lumex_args -DLUMEX_BUILD_DOCUMENTATION=ON
        "-DLUMEX_DOCUMENTATION_PREBUILT_DIR=${_documentation_dir}")
    set(_example_dirs generators math optional span string string_view)
    set(_example_build_dirs ${_example_dirs})
    set(_benchmark_dirs span)
    set(_benchmark_build_dirs span)
elseif(LUMEX_INSTALL_CASE_PROFILE STREQUAL "all_modules")
    list(APPEND _lumex_args -DLUMEX_BUILD_CORE=ON -DLUMEX_BUILD_APPLIED=ON
        -DLUMEX_BUILD_XML=ON)
    set(_example_dirs
        atomic base64 circular_buffer contracts crc environment exceptions
        expected filesystem fmt framing generators hardware hazard_pointer
        interprocess json logger logging math optional protocol reflection
        resource_monitor serial settings smart_ptr span string string_view
        temporary time transport unicode utility xml)
    set(_example_build_dirs protocol settings xml interprocess)
    set(_benchmark_dirs
        atomic circular_buffer expected fmt hazard_pointer optional span
        string_view transport)
    set(_benchmark_build_dirs expected)
else()
    message(FATAL_ERROR "unknown profile '${LUMEX_INSTALL_CASE_PROFILE}'")
endif()

if(NOT "${CMAKE_CXX_FLAGS}" STREQUAL "")
    list(APPEND _lumex_args "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
endif()
if(NOT "${LUMEX_CLANG_STDLIB}" STREQUAL "")
    list(APPEND _lumex_args "-DLUMEX_CLANG_STDLIB=${LUMEX_CLANG_STDLIB}")
endif()
set(_generator_args -G "${CMAKE_GENERATOR}")
if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND _generator_args -A "${CMAKE_GENERATOR_PLATFORM}")
endif()
if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND _generator_args -T "${CMAKE_GENERATOR_TOOLSET}")
endif()

_lumex_run("LumexLib configure"
    "${CMAKE_COMMAND}" -S "${LUMEX_SOURCE_DIR}" -B "${_lumex_build}"
    ${_generator_args} "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    ${_lumex_args})

if(LUMEX_INSTALL_CASE_PROFILE STREQUAL "all_modules")
    # The libraries one by one: the names of the generator's target listing
    # (Ninja and the Makefiles generators have one; a Visual Studio or Xcode
    # build would have to build everything, ALL targets included, and those
    # write into the checkout, so the case reports itself skipped there).
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${_lumex_build}" --target help
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _lumex_out
        ERROR_VARIABLE _lumex_err
    )
    if(NOT _rc EQUAL 0)
        message(STATUS "LUMEX_CONSUMER_SKIP: the generator '${CMAKE_GENERATOR}' has no "
            "target listing (--target help), and the case must not build the ALL targets "
            "of LumexLib (they write into the checkout)")
        return()
    endif()
    string(REGEX MATCHALL
        "(^|\n)(\\.\\.\\. )?(LumexCore_|LumexApplied_|LumexXml|lumex_3rdparty_)[A-Za-z0-9_]*"
        _listed "${_lumex_out}")
    set(_library_targets "")
    foreach(_line IN LISTS _listed)
        string(REGEX REPLACE "^[\n]?(\\.\\.\\. )?" "" _name "${_line}")
        list(APPEND _library_targets "${_name}")
    endforeach()
    list(REMOVE_DUPLICATES _library_targets)
    if(NOT _library_targets)
        message(FATAL_ERROR "no library target in the listing of ${_lumex_build}:\n${_lumex_out}")
    endif()
    _lumex_run("LumexLib build"
        "${CMAKE_COMMAND}" --build "${_lumex_build}" --config Release
        --target ${_library_targets})
endif()

_lumex_run("LumexLib install"
    "${CMAKE_COMMAND}" --install "${_lumex_build}" --config Release)

# ---------------------------------------------------------------------------
# The package: find_package of everything that was installed.
# ---------------------------------------------------------------------------
# A LumexLib installed elsewhere on this machine must not satisfy the search.
# The config includes its target files in dependency order: the applied
# settings link lumex::LumexXml, so the Xml targets come before the applied
# ones.
find_package(LumexLib REQUIRED PATHS "${_lumex_prefix}" NO_DEFAULT_PATH)
if(NOT TARGET lumex::Lumex)
    message(FATAL_ERROR "find_package(LumexLib) did not provide lumex::Lumex")
endif()
if(LUMEX_INSTALL_CASE_PROFILE STREQUAL "all_modules")
    foreach(_target lumex::xml lumex::settings lumex::transport lumex::protocol
            lumex::framing lumex::utility)
        if(NOT TARGET ${_target})
            message(FATAL_ERROR "find_package(LumexLib) did not provide ${_target}")
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
# The layout of the package.
# ---------------------------------------------------------------------------
_lumex_require_path("${_lumex_prefix}/share/LumexLib/LICENSE" "the license")
_lumex_require_path("${_lumex_prefix}/share/examples/cmake/LumexExampleHelpers.cmake"
    "the helper the examples include")
_lumex_require_path("${_lumex_prefix}/share/examples/README.md" "the examples guide")
_lumex_require_path("${_lumex_prefix}/share/benchmarks/README.md" "the benchmarks guide")
foreach(_dir IN LISTS _example_dirs)
    _lumex_require_path("${_lumex_prefix}/share/examples/${_dir}/CMakeLists.txt"
        "examples/${_dir}")
endforeach()
foreach(_dir IN LISTS _benchmark_dirs)
    foreach(_file CMakeLists.txt README.md results)
        _lumex_require_path("${_lumex_prefix}/share/benchmarks/${_dir}/${_file}"
            "benchmarks/${_dir}")
    endforeach()
endforeach()
# The documentation is optional: share/doc/html when it was asked for (the
# files of the Doxygen output that are not for reading stay out), nothing
# otherwise. Its old place, share/LumexLib/docs, is gone.
if(_documentation_dir)
    _lumex_require_path("${_lumex_prefix}/share/doc/html/index.html" "the documentation")
    _lumex_require_path("${_lumex_prefix}/share/doc/html/a00001.html" "the documentation")
    _lumex_require_no_path("${_lumex_prefix}/share/doc/html/a00001.map" "a map file of the documentation")
    _lumex_require_no_path("${_lumex_prefix}/share/doc/html/a00001.md5" "an md5 file of the documentation")
else()
    _lumex_require_no_path("${_lumex_prefix}/share/doc" "the documentation that was not asked for")
endif()
_lumex_require_no_path("${_lumex_prefix}/share/LumexLib/docs" "the old documentation place")

if(LUMEX_INSTALL_CASE_PROFILE STREQUAL "all_modules")
    # The protocol examples carry their helper headers, and the udev rules stay
    # where the permission hint of the transport names them.
    foreach(_header example_protocol_beacon.hpp example_protocol_lamp.hpp
            example_protocol_loopback.hpp)
        _lumex_require_path("${_lumex_prefix}/share/examples/protocol/${_header}"
            "the helper header of the protocol examples")
    endforeach()
    _lumex_require_path("${_lumex_prefix}/share/examples/xml/file_for_example_3.xml"
        "the data file of the xml example 3")
    _lumex_require_path("${_lumex_prefix}/share/LumexLib/udev" "the udev rules")
    _lumex_require_no_path("${_lumex_prefix}/share/examples/transport/udev"
        "a second copy of the udev rules")
endif()

# No binary of an example or of a benchmark anywhere in the package.
file(GLOB_RECURSE _installed LIST_DIRECTORIES false "${_lumex_prefix}/*")
foreach(_file IN LISTS _installed)
    get_filename_component(_name "${_file}" NAME)
    if(_name MATCHES "^Lumex[A-Za-z0-9]*(Example|Bench|Benchmark)[A-Za-z0-9_]*(\\.exe)?$")
        message(FATAL_ERROR "the package carries a binary: ${_file}")
    endif()
endforeach()
if(EXISTS "${_lumex_prefix}/bin")
    file(GLOB _bin LIST_DIRECTORIES false "${_lumex_prefix}/bin/*")
    foreach(_file IN LISTS _bin)
        get_filename_component(_name "${_file}" NAME)
        if(_name MATCHES "Example|Bench")
            message(FATAL_ERROR "bin/ carries ${_name}")
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
# The installed source directories as projects of their own.
# ---------------------------------------------------------------------------
# nlohmann/json 3.x is the consumer's: a package config of the vendored header
# stands for the one a consumer would have.
set(_nlohmann_dir "${CMAKE_BINARY_DIR}/nlohmann-package")
file(MAKE_DIRECTORY "${_nlohmann_dir}")
file(WRITE "${_nlohmann_dir}/nlohmann_jsonConfig.cmake" "
if(NOT TARGET nlohmann_json::nlohmann_json)
  add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
  set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES \"${LUMEX_SOURCE_DIR}/3rdparty\")
endif()
")
include(CMakePackageConfigHelpers)
write_basic_package_version_file("${_nlohmann_dir}/nlohmann_jsonConfigVersion.cmake"
    VERSION 3.12.0 COMPATIBILITY SameMajorVersion)

# A consumer of an installed Clang build uses the standard library the package
# was built with.
set(_standalone_flags "${CMAKE_CXX_FLAGS}")
if(LUMEX_CLANG_STDLIB STREQUAL "LIBCXX")
    set(_standalone_flags "${_standalone_flags} -stdlib=libc++")
endif()
set(_standalone_common
    ${_generator_args} "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_CXX_FLAGS=${_standalone_flags}"
    "-DLumexLib_DIR=${_lumex_dir}"
    "-DCMAKE_PREFIX_PATH=${_nlohmann_dir}")

# _lumex_standalone(<kind> <directory> <build>): configures share/<kind>/<directory>
# as a project; with <build> TRUE builds it (and runs the examples of the
# header-only profile).
function(_lumex_standalone kind dir build)
    set(_source "${_lumex_prefix}/share/${kind}/${dir}")
    string(SUBSTRING "${kind}" 0 2 _short)
    set(_binary "${CMAKE_BINARY_DIR}/${_short}/${dir}")
    file(REMOVE_RECURSE "${_binary}")
    _lumex_run("configure of share/${kind}/${dir}"
        "${CMAKE_COMMAND}" -S "${_source}" -B "${_binary}" ${_standalone_common})
    if(build)
        _lumex_run("build of share/${kind}/${dir}"
            "${CMAKE_COMMAND}" --build "${_binary}" --config Release)
        if(kind STREQUAL "examples" AND LUMEX_INSTALL_CASE_PROFILE STREQUAL "header_only"
           AND UNIX)
            file(READ "${_source}/CMakeLists.txt" _text)
            string(REGEX MATCHALL "NAME[ \t\r\n]+[A-Za-z0-9_]+" _names "${_text}")
            foreach(_entry IN LISTS _names)
                string(REGEX REPLACE "^NAME[ \t\r\n]+" "" _name "${_entry}")
                if(EXISTS "${_binary}/${_name}")
                    _lumex_run("run of ${_name} (share/examples/${dir})"
                        "${_binary}/${_name}")
                endif()
            endforeach()
        endif()
    endif()
endfunction()

foreach(_dir IN LISTS _example_dirs)
    if(_dir IN_LIST _example_build_dirs)
        _lumex_standalone(examples ${_dir} TRUE)
    else()
        _lumex_standalone(examples ${_dir} FALSE)
    endif()
endforeach()
foreach(_dir IN LISTS _benchmark_dirs)
    if(_dir IN_LIST _benchmark_build_dirs)
        _lumex_standalone(benchmarks ${_dir} TRUE)
    else()
        _lumex_standalone(benchmarks ${_dir} FALSE)
    endif()
endforeach()

if(LUMEX_INSTALL_CASE_PROFILE STREQUAL "all_modules")
    # Without nlohmann/json the protocol examples configure and skip the export
    # example, with a message, instead of failing. (The library is built with
    # LUMEX_PROTOCOL_WITH_JSON: its export needs the consumer's header.)
    set(_no_json_build "${CMAKE_BINARY_DIR}/ex/protocol_without_json")
    file(REMOVE_RECURSE "${_no_json_build}")
    _lumex_run("configure of share/examples/protocol without nlohmann/json"
        "${CMAKE_COMMAND}" -S "${_lumex_prefix}/share/examples/protocol"
        -B "${_no_json_build}" ${_standalone_common}
        -DCMAKE_DISABLE_FIND_PACKAGE_nlohmann_json=ON)
    if(NOT _lumex_out MATCHES "LumexProtocolExampleExport[^\n]*skipped")
        message(FATAL_ERROR
            "the export example is not reported as skipped without nlohmann/json:\n${_lumex_out}")
    endif()
    _lumex_run("build of share/examples/protocol without nlohmann/json"
        "${CMAKE_COMMAND}" --build "${_no_json_build}" --config Release)
endif()

add_executable(install_share_layout_consumer main.cpp)
target_link_libraries(install_share_layout_consumer PRIVATE lumex::Lumex)
set_target_properties(install_share_layout_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
