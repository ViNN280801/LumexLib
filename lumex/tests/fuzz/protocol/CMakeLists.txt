# lumex/tests/fuzz/protocol/CMakeLists.txt

# fuzz.protocol.layouts: the decoders of the three layouts of
# lumex/core/protocol over the synthetic gauge table (every field kind), with
# AddressSanitizer. fuzz.protocol.devices: dispatch over every layer of the
# thirteen de-identified device tables. The targets compile the sources they
# need (the error category, the assertion handler; the devices also the CRC
# catalog and the contract handler of the codecs a table names) instead of
# linking the module libraries, so that the sanitized executable never mixes
# with libraries built without the sanitizer or with another C runtime.
# lumex_add_protocol_fuzzer(<target> <test name> <source> [<extra source>...])
function(lumex_add_protocol_fuzzer target test_name source)
    add_executable(${target}
        "${source}"
        "${LumexLib_SOURCE_DIR}/lumex/core/protocol/error/LumexProtocolErrc.cpp"
        "${LumexLib_SOURCE_DIR}/lumex/core/utility/assert/LumexAssert.cpp"
        ${ARGN}
    )
    target_include_directories(${target} PRIVATE "${LumexLib_SOURCE_DIR}")
    # The export macros of the sources resolve to this executable.
    target_compile_definitions(${target} PRIVATE
        LumexCore_protocol_EXPORTS LUMEX_EXPORTS)
    target_compile_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)

    set(_environment "")
    if(MSVC)
        # clang-cl: the libFuzzer runtime of the LLVM distribution is built with
        # the static C runtime; the vector and string annotations of the MSVC STL
        # disagree with the uninstrumented STL inside that runtime (false
        # container overflows), so they are off for this target.
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
        target_compile_definitions(${target} PRIVATE
            _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
        get_filename_component(_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        file(GLOB _runtime_dirs "${_bin}/../lib/clang/*/lib/windows")
        list(GET _runtime_dirs 0 _runtime_dir)
        get_filename_component(_runtime_dir "${_runtime_dir}" ABSOLUTE)
        # The objects name the AddressSanitizer libraries, the linker (called
        # directly, not through the clang-cl driver) finds them here; libFuzzer
        # (and its main) is named explicitly. The run finds the AddressSanitizer
        # DLL on PATH.
        target_link_options(${target} PRIVATE "/LIBPATH:${_runtime_dir}")
        target_link_libraries(${target} PRIVATE
            "${_runtime_dir}/clang_rt.fuzzer-x86_64.lib")
        file(TO_NATIVE_PATH "${_runtime_dir}" _native_runtime_dir)
        string(REPLACE ";" "\;" _path "$ENV{PATH}")
        set(_environment "PATH=${_native_runtime_dir}\;${_path}")
    else()
        target_link_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()

    set(_corpus "${CMAKE_CURRENT_BINARY_DIR}/corpus_${test_name}")
    file(MAKE_DIRECTORY "${_corpus}")
    lumex_test_name(_ctest_name ${test_name})
    add_test(NAME ${_ctest_name}
        COMMAND ${target} -max_total_time=30 -rss_limit_mb=2048
                -malloc_limit_mb=1024 -print_final_stats=1 "${_corpus}")
    set_tests_properties(${_ctest_name} PROPERTIES LABELS fuzz TIMEOUT 120)
    if(_environment)
        set_tests_properties(${_ctest_name} PROPERTIES ENVIRONMENT "${_environment}")
    endif()
endfunction()

lumex_add_protocol_fuzzer(LumexProtocolLayoutFuzz layouts
    LumexProtocolLayoutFuzz.cpp)
lumex_add_protocol_fuzzer(LumexProtocolDevicesFuzz devices
    LumexProtocolDevicesFuzz.cpp
    "${LumexLib_SOURCE_DIR}/lumex/core/framing/codec/LumexFramingErrc.cpp"
    "${LumexLib_SOURCE_DIR}/lumex/core/crc/catalog/LumexCrcCatalog.cpp"
    "${LumexLib_SOURCE_DIR}/lumex/core/contracts/handler/LumexContractsHandler.cpp")
