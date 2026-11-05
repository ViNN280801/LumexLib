# benchmarks/transport/CMakeLists.txt
#
# The frame codecs, the protocol messages and the dialogue of the transport:
# throughput and latency of the codecs, the cost of a protocol message and of
# `describe`, the log call below the threshold, the round trip of an exchange
# on the mocks, and many devices on one io_runner. Built only with
# LUMEX_BUILD_BENCHMARKS=ON (OFF by default) and only where the transport
# compiles the dialogue engine and the testing doubles; a Release build
# without sanitizers is the only one whose numbers mean anything.
#
#   LumexTransportBenchCodec      core/framing codecs (clock: none)
#   LumexTransportBenchProtocol   messages, index match, describe (clock: none)
#   LumexTransportBenchLogging    LUMEX_TRANSPORT_LOG below the threshold,
#                                 with its falsifier (clock: none)
#   LumexTransportBenchDialogue   one exchange on mocks (virtual and real)
#   LumexTransportBenchDevices    16 devices on one io_runner, CPU on mocks
#                                 (virtual clock; no budget verdict)
#   LumexTransportBenchSockets    16 devices on one io_runner over TCP
#                                 loopback with real timers (real clock;
#                                 Linux only); the 16-device budget is
#                                 judged on this one
#
# The compile time of a client over a device table is measured by
# compile_time.py, which compiles outside this build.
#
#   cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
#         -DLUMEX_BUILD_BENCHMARKS=ON
#   cmake --build build-bench --target LumexTransportBenchmarks
#   python benchmarks/transport/run_benchmark.py
#          --build-dir gcc=build-bench-gcc --build-dir clang=build-bench-clang

set(_lumex_transport_bench_sources
    Codec:bench_codec.cpp
    Protocol:bench_protocol.cpp
    Logging:bench_logging.cpp
    Dialogue:bench_dialogue.cpp
    Devices:bench_devices.cpp)
# The load over real sockets reads the CPU time of the runner thread
# (CLOCK_THREAD_CPUTIME_ID, RUSAGE_THREAD).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND _lumex_transport_bench_sources Sockets:bench_sockets.cpp)
endif()

set(LUMEX_TRANSPORT_BENCH_TARGETS "")
foreach(_entry IN LISTS _lumex_transport_bench_sources)
    string(REPLACE ":" ";" _parts "${_entry}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _source)
    set(_target LumexTransportBench${_name})
    add_executable(${_target} ${_source})
    target_link_libraries(${_target} PRIVATE
        lumex::transport lumex::protocol lumex::framing)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    if(MSVC)
        target_compile_options(${_target} PRIVATE /utf-8)
    endif()
    list(APPEND LUMEX_TRANSPORT_BENCH_TARGETS ${_target})
endforeach()

add_custom_target(LumexTransportBenchmarks DEPENDS ${LUMEX_TRANSPORT_BENCH_TARGETS})

# `LumexTransportBenchmarkRun` runs the executables of THIS build tree and
# writes the CSV files and the charts into the build tree; the committed
# results (two toolchains) come from run_benchmark.py with both build trees.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(LumexTransportBenchmarkRun
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run_benchmark.py"
            --build-dir "this=${CMAKE_BINARY_DIR}"
            --out-dir "${CMAKE_BINARY_DIR}/transport_benchmark"
        DEPENDS LumexTransportBenchmarks
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running the transport benchmarks (this build tree)"
        VERBATIM
        USES_TERMINAL
    )
endif()
