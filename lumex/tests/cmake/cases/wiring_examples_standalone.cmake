# The example and benchmark directories are installed as sources
# (share/examples/<module>, share/benchmarks/<name>) and built as projects of
# their own against the installed package: cmake -S <dir> -B build
# -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib.
#
# * CMake 4 refuses a top-level CMakeLists.txt without a literal
#   cmake_minimum_required and project () call; both were inside the function
#   lumex_example_executable, so no installed example directory configured.
#   The same file serves the tree (a subdirectory with its own project ()) and
#   the installed copy.
# * The directory name decides which module option installs it
#   (cmake/LumexInstallSources.cmake): LUMEX_BUILD_<NAME>.
# * No example installs a binary (the package carries sources only).
# * The settings examples write one shared file, so they take a RESOURCE_LOCK.
# * The protocol export example is skipped, with a message, where nlohmann/json
#   is missing but the library was built to need it.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_require_no_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "${path} must not mention ${needle}")
    endif()
endfunction()

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _options)
file(READ "${LUMEX_SOURCE_DIR}/lumex/examples/CMakeLists.txt" _examples_cmake)

set(_example_count 0)
file(GLOB _example_dirs LIST_DIRECTORIES true "${LUMEX_SOURCE_DIR}/lumex/examples/*")
foreach(_dir IN LISTS _example_dirs)
    if(NOT IS_DIRECTORY "${_dir}" OR NOT EXISTS "${_dir}/CMakeLists.txt")
        continue()
    endif()
    get_filename_component(_name "${_dir}" NAME)
    math(EXPR _example_count "${_example_count} + 1")
    string(TOUPPER "${_name}" _upper)
    string(FIND "${_options}" "option(LUMEX_BUILD_${_upper} " _option)
    if(_option EQUAL -1)
        message(FATAL_ERROR
            "lumex/examples/${_name} has no option LUMEX_BUILD_${_upper}: the install "
            "rule (cmake/LumexInstallSources.cmake) decides by the name of the directory")
    endif()
    string(FIND "${_examples_cmake}" "add_subdirectory(${_name})" _added)
    if(_added EQUAL -1)
        message(FATAL_ERROR "lumex/examples/CMakeLists.txt does not add ${_name}")
    endif()
    file(READ "${_dir}/CMakeLists.txt" _text)
    if(NOT _text MATCHES "(^|\n)cmake_minimum_required[ \t]*\\(VERSION 3\\.16\\)")
        message(FATAL_ERROR
            "lumex/examples/${_name}/CMakeLists.txt has no literal top-level "
            "cmake_minimum_required (VERSION 3.16)")
    endif()
    if(NOT _text MATCHES "(^|\n)project[ \t]*\\(LumexExamples[A-Za-z0-9_]+ LANGUAGES CXX\\)")
        message(FATAL_ERROR
            "lumex/examples/${_name}/CMakeLists.txt has no literal top-level project ()")
    endif()
    string(FIND "${_text}" "cmake_minimum_required" _first)
    string(FIND "${_text}" "include(\${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)" _include)
    if(_include EQUAL -1 OR _include LESS _first)
        message(FATAL_ERROR
            "lumex/examples/${_name}/CMakeLists.txt must include the helper after "
            "cmake_minimum_required and project")
    endif()
    if(_text MATCHES "(^|\n)[ \t]*install[ \t]*\\([ \t\r\n]*(TARGETS|FILES)")
        message(FATAL_ERROR
            "lumex/examples/${_name}/CMakeLists.txt installs a file: the examples are "
            "installed as sources by the root install rule, with no binary")
    endif()
endforeach()
if(_example_count LESS 30)
    message(FATAL_ERROR "found only ${_example_count} example directories")
endif()

# The helper: the standalone find_package runs where the directory's project ()
# has already run, and neither call is inside the function.
file(READ "${LUMEX_SOURCE_DIR}/lumex/examples/cmake/LumexExampleHelpers.cmake" _helper)
if(_helper MATCHES "(^|\n)[ \t]*(cmake_minimum_required|project)[ \t]*\\(")
    message(FATAL_ERROR
        "lumex/examples/cmake/LumexExampleHelpers.cmake calls cmake_minimum_required "
        "or project (): CMake 4 wants them literal in the top-level CMakeLists.txt")
endif()
_require_no_text("lumex/examples/cmake/LumexExampleHelpers.cmake" "install(TARGETS")
_require_no_text("lumex/examples/CMakeLists.txt" "install(TARGETS")
_require_text("lumex/examples/cmake/LumexExampleHelpers.cmake" "find_package(LumexLib REQUIRED)")

# The benchmark directories with a CMakeLists.txt build standalone too.
set(_benchmark_count 0)
file(GLOB _benchmark_dirs LIST_DIRECTORIES true "${LUMEX_SOURCE_DIR}/benchmarks/*")
foreach(_dir IN LISTS _benchmark_dirs)
    if(NOT IS_DIRECTORY "${_dir}" OR NOT EXISTS "${_dir}/CMakeLists.txt")
        continue()
    endif()
    get_filename_component(_name "${_dir}" NAME)
    math(EXPR _benchmark_count "${_benchmark_count} + 1")
    string(TOUPPER "${_name}" _upper)
    string(FIND "${_options}" "option(LUMEX_BUILD_${_upper} " _option)
    if(_option EQUAL -1)
        message(FATAL_ERROR "benchmarks/${_name} has no option LUMEX_BUILD_${_upper}")
    endif()
    file(READ "${_dir}/CMakeLists.txt" _text)
    if(NOT _text MATCHES "(^|\n)cmake_minimum_required[ \t]*\\(VERSION 3\\.16\\)")
        message(FATAL_ERROR
            "benchmarks/${_name}/CMakeLists.txt has no literal top-level cmake_minimum_required")
    endif()
    if(NOT _text MATCHES "(^|\n)project[ \t]*\\(LumexBenchmarks[A-Za-z0-9_]+ LANGUAGES CXX\\)")
        message(FATAL_ERROR
            "benchmarks/${_name}/CMakeLists.txt has no literal top-level project ()")
    endif()
    if(NOT _text MATCHES "find_package[ \t]*\\(LumexLib REQUIRED\\)")
        message(FATAL_ERROR
            "benchmarks/${_name}/CMakeLists.txt does not find the installed package")
    endif()
    if(_text MATCHES "(^|\n)[ \t]*install[ \t]*\\(")
        message(FATAL_ERROR "benchmarks/${_name}/CMakeLists.txt installs a file")
    endif()
endforeach()
if(_benchmark_count LESS 8)
    message(FATAL_ERROR "found only ${_benchmark_count} benchmark directories")
endif()

# Examples that write one shared file are not run at the same time.
_require_text("lumex/examples/settings/CMakeLists.txt" "RESOURCE_LOCK")

# The protocol export example needs nlohmann/json only when the library does.
_require_text("lumex/examples/protocol/CMakeLists.txt" "LumexProtocolExampleExport skipped")
