# benchmarks/hazard_pointer/CMakeLists.txt
#
# The hazard pointer module of this library against the usual alternatives for
# a read-mostly pointer (a plain unprotected load as the lower bound, a
# std::shared_mutex read lock, std::atomic_load of a std::shared_ptr) and,
# when a local copy is given, libcds HP and a port of the libc++ pull request
# engine. Built only with LUMEX_BUILD_BENCHMARKS=ON (OFF by default); a
# Release build without sanitizers is the only one whose numbers mean anything.
#
#   cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
#         -DLUMEX_BUILD_BENCHMARKS=ON
#   cmake --build build-bench --target LumexHazardPointerBenchmark
#   python benchmarks/hazard_pointer/run_benchmark.py
#          --exe build-bench/bin/LumexHazardPointerBenchmark --quick
#          --out-dir build-bench/hazard_pointer-quick
#
# The comparison with other engines needs sources that are not part of this
# repository; point the build at a local copy:
#
#   -DLUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR=<libcds checkout (the cds/
#       directory and src/ with hp.cpp, dhp.cpp, init.cpp, thread_data.cpp)>
#   -DLUMEX_HAZARD_POINTER_BENCH_LLVM_DIR=<directory with hazard_pointer_port.hpp,
#       a port of the engine of llvm-project#218218 to namespace llvm_port>
#
# The adapters (adapter_libcds.hpp, adapter_llvm.hpp) are in this directory and
# are only compiled when the corresponding directory is given.
#
# LumexHazardPointerBenchmarkRun runs the full sweep and rewrites results/.

set(LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR "" CACHE PATH
    "A libcds checkout to compare with (cds/ headers and src/); empty: no libcds series")
set(LUMEX_HAZARD_POINTER_BENCH_LLVM_DIR "" CACHE PATH
    "A directory with hazard_pointer_port.hpp (port of the libc++ engine); empty: no libc++ series")

add_executable(LumexHazardPointerBenchmark bench_hazard_pointer.cpp)
target_link_libraries(LumexHazardPointerBenchmark PRIVATE lumex::hazard_pointer)
target_include_directories(LumexHazardPointerBenchmark PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(LumexHazardPointerBenchmark PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR
   AND EXISTS "${LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR}/cds/gc/hp.h")
    message(STATUS "hazard_pointer benchmark: libcds from ${LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR}")
    target_compile_definitions(LumexHazardPointerBenchmark PRIVATE LUMEX_HP_BENCH_LIBCDS)
    target_include_directories(LumexHazardPointerBenchmark SYSTEM PRIVATE
        "${LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR}")
    target_sources(LumexHazardPointerBenchmark PRIVATE
        "${LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR}/src/hp.cpp"
        "${LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR}/src/dhp.cpp"
        "${LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR}/src/init.cpp"
        "${LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR}/src/thread_data.cpp"
        "${LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR}/src/hp_thread_local.cpp")
elseif(LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR)
    message(WARNING "LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR has no cds/gc/hp.h; no libcds series")
endif()

if(LUMEX_HAZARD_POINTER_BENCH_LLVM_DIR
   AND EXISTS "${LUMEX_HAZARD_POINTER_BENCH_LLVM_DIR}/hazard_pointer_port.hpp")
    message(STATUS "hazard_pointer benchmark: libc++ engine port from ${LUMEX_HAZARD_POINTER_BENCH_LLVM_DIR}")
    target_compile_definitions(LumexHazardPointerBenchmark PRIVATE LUMEX_HP_BENCH_LLVM)
    target_include_directories(LumexHazardPointerBenchmark PRIVATE
        "${LUMEX_HAZARD_POINTER_BENCH_LLVM_DIR}")
elseif(LUMEX_HAZARD_POINTER_BENCH_LLVM_DIR)
    message(WARNING "LUMEX_HAZARD_POINTER_BENCH_LLVM_DIR has no hazard_pointer_port.hpp; no libc++ series")
endif()

# `LumexHazardPointerBenchmarkRun` runs the full sweep and rewrites the CSV, the
# Markdown table and the charts in the source tree.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(LumexHazardPointerBenchmarkRun
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run_benchmark.py"
            --exe "$<TARGET_FILE:LumexHazardPointerBenchmark>"
            --out-dir "${CMAKE_CURRENT_SOURCE_DIR}/results"
        DEPENDS LumexHazardPointerBenchmark
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running the hazard pointer benchmark (full sweep)"
        VERBATIM
        USES_TERMINAL
    )
endif()
