# lumex/tests/fuzz/framing/CMakeLists.txt

# fuzz.framing.<codec>: one target per frame codec of lumex/core/framing (SLIP,
# COBS, delimiter, length prefix, header, Modbus RTU) and one for the stage
# pipelines, with AddressSanitizer; the checks are in
# LumexFramingFuzzSupport.hpp. Like the other fuzz targets they compile the
# sources they need (the error category of the framing, the CRC catalog, the
# contract handler, the assertion handler) instead of linking the module
# libraries, so that the sanitized executable never mixes with libraries built
# without the sanitizer or with another C runtime.
set(_runtime_dir "")
if(MSVC)
    # clang-cl links libFuzzer and the AddressSanitizer runtime from the LLVM
    # distribution next to the compiler: without it the targets are skipped.
    get_filename_component(_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    file(GLOB _runtime_dirs "${_bin}/../lib/clang/*/lib/windows")
    if(NOT _runtime_dirs)
        message(WARNING
            "LUMEX_BUILD_FUZZERS: no clang runtime directory "
            "(${_bin}/../lib/clang/*/lib/windows); fuzz.framing.* is not built")
        return()
    endif()
    list(GET _runtime_dirs 0 _runtime_dir)
    get_filename_component(_runtime_dir "${_runtime_dir}" ABSOLUTE)
endif()

# lumex_add_framing_fuzzer(<name> <source>): the target Lumex<Name>Fuzz from
# <source> and its CTest run fuzz.framing.<name> (30 seconds).
function(lumex_add_framing_fuzzer name source)
    set(_target Lumex${name}Fuzz)
    string(TOLOWER "${name}" _lower)
    add_executable(${_target}
        "${source}"
        "${LumexLib_SOURCE_DIR}/lumex/core/framing/codec/LumexFramingErrc.cpp"
        "${LumexLib_SOURCE_DIR}/lumex/core/crc/catalog/LumexCrcCatalog.cpp"
        "${LumexLib_SOURCE_DIR}/lumex/core/contracts/handler/LumexContractsHandler.cpp"
        "${LumexLib_SOURCE_DIR}/lumex/core/utility/assert/LumexAssert.cpp"
    )
    target_include_directories(${_target} PRIVATE "${LumexLib_SOURCE_DIR}")
    # The export macros of the sources resolve to this executable.
    target_compile_definitions(${_target} PRIVATE
        LumexCore_framing_EXPORTS LumexCore_crc_EXPORTS
        LumexCore_contracts_EXPORTS LUMEX_EXPORTS)
    target_compile_options(${_target} PRIVATE -fsanitize=fuzzer,address,undefined)

    set(_environment "")
    if(MSVC)
        # clang-cl: the libFuzzer runtime of the LLVM distribution is built
        # with the static C runtime; the vector and string annotations of the
        # MSVC STL disagree with the uninstrumented STL inside that runtime
        # (false container overflows), so they are off for the targets.
        set_property(TARGET ${_target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
        target_compile_definitions(${_target} PRIVATE
            _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
        # The objects name the AddressSanitizer libraries, the linker (called
        # directly, not through the clang-cl driver) finds them here;
        # libFuzzer (and its main) is named explicitly. The run finds the
        # AddressSanitizer DLL on PATH.
        target_link_options(${_target} PRIVATE "/LIBPATH:${_runtime_dir}")
        target_link_libraries(${_target} PRIVATE
            "${_runtime_dir}/clang_rt.fuzzer-x86_64.lib")
        file(TO_NATIVE_PATH "${_runtime_dir}" _native_runtime_dir)
        string(REPLACE ";" "\;" _path "$ENV{PATH}")
        set(_environment "PATH=${_native_runtime_dir}\;${_path}")
    else()
        target_link_options(${_target} PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()

    # The target writes its seed corpus (frames made by the codec from
    # synthetic payloads) into the corpus directory it is given.
    set(_corpus "${CMAKE_CURRENT_BINARY_DIR}/corpus_${_lower}")
    file(MAKE_DIRECTORY "${_corpus}")
    lumex_test_name(_ctest_name ${_lower})
    add_test(NAME ${_ctest_name}
        COMMAND ${_target} -max_total_time=30 -rss_limit_mb=2048
                -malloc_limit_mb=1024 -print_final_stats=1 "${_corpus}")
    set_tests_properties(${_ctest_name} PROPERTIES LABELS fuzz TIMEOUT 120)
    if(_environment)
        set_tests_properties(${_ctest_name} PROPERTIES ENVIRONMENT
            "${_environment}")
    endif()
endfunction()

lumex_add_framing_fuzzer(Slip LumexSlipFuzz.cpp)
lumex_add_framing_fuzzer(Cobs LumexCobsFuzz.cpp)
lumex_add_framing_fuzzer(Delimiter LumexDelimiterFuzz.cpp)
lumex_add_framing_fuzzer(LengthPrefix LumexLengthPrefixFuzz.cpp)
lumex_add_framing_fuzzer(Header LumexHeaderFuzz.cpp)
lumex_add_framing_fuzzer(ModbusRtu LumexModbusRtuFuzz.cpp)
lumex_add_framing_fuzzer(Pipeline LumexPipelineFuzz.cpp)
