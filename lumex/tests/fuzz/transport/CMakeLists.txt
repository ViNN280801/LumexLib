# lumex/tests/fuzz/transport/CMakeLists.txt

# fuzz.transport.hid_descriptor: the HID report descriptor analysis of
# lumex/applied/transport/hid (analyze_report_descriptor), with
# AddressSanitizer. Like the protocol target, it compiles the sources it needs
# (the analysis and the error category of the transport, plus the assertion
# handler the expected type reaches) instead of linking the module library, so
# that the sanitized executable never mixes with libraries built without the
# sanitizer or with another C runtime. The HID transport (and with it this
# target) exists only with LUMEX_TRANSPORT_WITH_HID.
if(NOT LUMEX_TRANSPORT_WITH_HID)
    return()
endif()

# clang-cl links libFuzzer and the AddressSanitizer runtime from the LLVM
# distribution next to the compiler: without it the target is skipped.
if(MSVC)
    get_filename_component(_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    file(GLOB _runtime_dirs "${_bin}/../lib/clang/*/lib/windows")
    if(NOT _runtime_dirs)
        message(WARNING
            "LUMEX_BUILD_FUZZERS: no clang runtime directory "
            "(${_bin}/../lib/clang/*/lib/windows); fuzz.transport.hid_descriptor "
            "is not built")
        return()
    endif()
    list(GET _runtime_dirs 0 _runtime_dir)
    get_filename_component(_runtime_dir "${_runtime_dir}" ABSOLUTE)
endif()

set(_target LumexHidDescriptorFuzz)
add_executable(${_target}
    LumexHidDescriptorFuzz.cpp
    "${LumexLib_SOURCE_DIR}/lumex/applied/transport/hid/LumexHidReportDescriptor.cpp"
    "${LumexLib_SOURCE_DIR}/lumex/applied/transport/error/LumexTransportErrc.cpp"
    "${LumexLib_SOURCE_DIR}/lumex/core/utility/assert/LumexAssert.cpp"
)
target_include_directories(${_target} PRIVATE "${LumexLib_SOURCE_DIR}")
# Asio's headers (the error category maps its codes); no library of Asio.
target_link_libraries(${_target} PRIVATE lumex::3rdparty_asio)
if(WIN32)
    target_compile_definitions(${_target} PRIVATE _WIN32_WINNT=0x0A00)
endif()
# The export macros of the sources resolve to this executable.
target_compile_definitions(${_target} PRIVATE
    LumexApplied_transport_EXPORTS LUMEX_EXPORTS)
target_compile_options(${_target} PRIVATE -fsanitize=fuzzer,address)

set(_environment "")
if(MSVC)
    # clang-cl: the libFuzzer runtime of the LLVM distribution is built with
    # the static C runtime; the vector and string annotations of the MSVC STL
    # disagree with the uninstrumented STL inside that runtime (false
    # container overflows), so they are off for this target.
    set_property(TARGET ${_target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
    target_compile_definitions(${_target} PRIVATE
        _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
    # The objects name the AddressSanitizer libraries, the linker (called
    # directly, not through the clang-cl driver) finds them here; libFuzzer
    # (and its main) is named explicitly. The run finds the AddressSanitizer
    # DLL on PATH.
    target_link_options(${_target} PRIVATE "/LIBPATH:${_runtime_dir}")
    target_link_libraries(${_target} PRIVATE
        "${_runtime_dir}/clang_rt.fuzzer-x86_64.lib")
    file(TO_NATIVE_PATH "${_runtime_dir}" _native_runtime_dir)
    string(REPLACE ";" "\;" _path "$ENV{PATH}")
    set(_environment "PATH=${_native_runtime_dir}\;${_path}")
else()
    target_link_options(${_target} PRIVATE -fsanitize=fuzzer,address)
endif()

# The target writes its seed corpus (synthetic descriptors) into the corpus
# directory it is given.
set(_corpus "${CMAKE_CURRENT_BINARY_DIR}/corpus")
file(MAKE_DIRECTORY "${_corpus}")
lumex_test_name(_ctest_name hid_descriptor)
add_test(NAME ${_ctest_name}
    COMMAND ${_target} -max_total_time=30 -rss_limit_mb=2048
            -print_final_stats=1 "${_corpus}")
set_tests_properties(${_ctest_name} PROPERTIES LABELS fuzz TIMEOUT 120)
if(_environment)
    set_tests_properties(${_ctest_name} PROPERTIES ENVIRONMENT "${_environment}")
endif()
