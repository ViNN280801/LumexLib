# benchmarks/fmt/CMakeLists.txt
#
# LumexFormat throughput against std::format, std::ostringstream,
# std::snprintf and std::to_string. Built only with LUMEX_BUILD_BENCHMARKS=ON
# (OFF by default); C++20 so std::format takes part. Run a Release build:
#
#   cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
#         -DLUMEX_BUILD_BENCHMARKS=ON
#   cmake --build build-bench --target LumexFmtBenchmark
#   build-bench/bin/LumexFmtBenchmark benchmarks/fmt/results/format_benchmark.csv
#   python benchmarks/fmt/plot_results.py benchmarks/fmt/results/format_benchmark.csv

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install (cmake -S <prefix>/share/benchmarks/fmt
# -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib), a subproject in the tree.
project(LumexBenchmarksFmt LANGUAGES CXX)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    find_package(LumexLib REQUIRED)
    foreach(_lumex_required lumex::fmt)
        if(NOT TARGET ${_lumex_required})
            message(FATAL_ERROR
                "${_lumex_required} not found: the package was built without that module "
                "(LumexLib_DIR=${LumexLib_DIR})")
        endif()
    endforeach()
    # Only a Release build gives numbers that mean anything.
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    endif()
endif()

add_executable(LumexFmtBenchmark bench_format.cpp)
target_link_libraries(LumexFmtBenchmark PRIVATE lumex::fmt)
set_target_properties(LumexFmtBenchmark PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(MSVC)
    target_compile_options(LumexFmtBenchmark PRIVATE /utf-8)
endif()

# `LumexFmtBenchmarkRun` rebuilds the CSV and the charts in the source tree.
# Standalone (the installed package), the results go to the build tree: the
# sources of a package are not written to.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_lumex_results_dir "${CMAKE_BINARY_DIR}/results")
else()
    set(_lumex_results_dir "${CMAKE_CURRENT_SOURCE_DIR}/results")
endif()
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(LumexFmtBenchmarkRun
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_lumex_results_dir}"
        COMMAND LumexFmtBenchmark
            "${_lumex_results_dir}/format_benchmark.csv"
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/plot_results.py"
            "${_lumex_results_dir}/format_benchmark.csv"
        DEPENDS LumexFmtBenchmark
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running the LumexFormat benchmark and plotting the results"
        VERBATIM
    )
endif()
