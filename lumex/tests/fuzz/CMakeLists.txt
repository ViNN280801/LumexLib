# lumex/tests/fuzz/CMakeLists.txt

# libFuzzer targets (LUMEX_BUILD_FUZZERS, OFF by default; transport and
# protocol design, G3): one directory per module, each target with a CTest
# run of a bounded time under the label fuzz (`ctest -L fuzz`), outside the
# default runs (their names start with fuzz.). A finding becomes a regression
# case of the module's unit suite.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING
        "LUMEX_BUILD_FUZZERS needs Clang (libFuzzer); "
        "${CMAKE_CXX_COMPILER_ID} builds no fuzz target")
    return()
endif()

lumex_add_subdirectory_if(LUMEX_BUILD_PROTOCOL protocol)
