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
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(LumexFmtBenchmarkRun
        COMMAND LumexFmtBenchmark
            "${CMAKE_CURRENT_SOURCE_DIR}/results/format_benchmark.csv"
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/plot_results.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/results/format_benchmark.csv"
        DEPENDS LumexFmtBenchmark
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running the LumexFormat benchmark and plotting the results"
        VERBATIM
    )
endif()
