# benchmarks/atomic/CMakeLists.txt
#
# Contention benchmark of LumexLib's atomic_shared_ptr against the standard
# library's std::atomic<std::shared_ptr<T>>. Built only with
# LUMEX_BUILD_BENCHMARKS=ON (OFF by default); a Release build without
# sanitizers is the only one whose numbers mean anything.
#
# One executable holds every implementation: the C++11 unit is an object
# library of its own (LumexLib's lock-based implementation sleeping on the
# wait table), the forced lock-based unit gets
# LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED, and the rest is C++20. LumexLib's
# inline ABI namespaces keep the three selections apart in one program.
#
#   cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
#         -DLUMEX_BUILD_BENCHMARKS=ON
#   cmake --build build-bench --target LumexAtomicBenchmark
#   python benchmarks/atomic/run_benchmark.py
#          --exe build-bench/bin/LumexAtomicBenchmark --quick
#
# LumexAtomicBenchmarkRun runs the full sweep (100 repetitions, 30 s pauses:
# about an hour and a half on 20 hardware threads) and rewrites results/.

add_library(LumexAtomicBenchmarkCxx11 OBJECT bench_lumex_cxx11.cpp)
target_link_libraries(LumexAtomicBenchmarkCxx11 PRIVATE lumex::atomic)
set_target_properties(LumexAtomicBenchmarkCxx11 PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_executable(LumexAtomicBenchmark
    bench_atomic_smart_ptr.cpp
    bench_lumex_default.cpp
    bench_lumex_lock_based.cpp
    $<TARGET_OBJECTS:LumexAtomicBenchmarkCxx11>
)
set_source_files_properties(bench_lumex_lock_based.cpp PROPERTIES
    COMPILE_DEFINITIONS LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED
)
target_link_libraries(LumexAtomicBenchmark PRIVATE lumex::atomic)
set_target_properties(LumexAtomicBenchmark PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# `LumexAtomicBenchmarkRun` runs the full sweep and rewrites the CSV, the
# Markdown table and the charts in the source tree.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(LumexAtomicBenchmarkRun
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run_benchmark.py"
            --exe "$<TARGET_FILE:LumexAtomicBenchmark>"
            --out-dir "${CMAKE_CURRENT_SOURCE_DIR}/results"
            --raw-csv "${CMAKE_BINARY_DIR}/atomic_benchmark_raw.csv"
        DEPENDS LumexAtomicBenchmark
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running the atomic smart pointer benchmark (full sweep)"
        VERBATIM
        USES_TERMINAL
    )
endif()

# The 128-bit compare-and-swap of lumex/core/atomic/dwcas against a 64-bit one
# on 1 to 8 threads (ratios; see README.md). A small program of its own, built
# at C++11; on a target without the layer it prints a message and fails.
#
#   cmake --build build-bench --target LumexDwcasBenchmark
#   build-bench/bin/LumexDwcasBenchmark --repetitions 3
add_executable(LumexDwcasBenchmark bench_dwcas.cpp)
target_link_libraries(LumexDwcasBenchmark PRIVATE lumex::atomic)
set_target_properties(LumexDwcasBenchmark PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
