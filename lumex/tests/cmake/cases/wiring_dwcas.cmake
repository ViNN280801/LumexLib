# core/atomic/dwcas wiring: the 128-bit compare-and-swap layer (x86-64 only)
# inside the header-only atomic module. Its tests are a module of their own in
# lumex/tests/LumexTestStandards.cmake (atomic.dwcas: C++11, 14, 17, 20, the
# variants builtin and msvc_wrapper that run the same sources on the other
# backends), built by lumex_add_standard_suites under the CTest prefix
# "atomic.dwcas."; the umbrella includes the layer; the backends, the CPU
# check and the switches are the ones the documentation names; the consumer
# fixture compiles the misuse the types stop and the configuration of the
# targets this machine cannot run.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

set(_tests "lumex/tests/core/atomic/dwcas/CMakeLists.txt")
_require_text("${_tests}" "lumex_add_standard_suites(Dwcas")
_require_text("${_tests}" "MODULE atomic.dwcas")
_require_text("${_tests}" "LINK lumex::atomic")
_require_text("${_tests}" "VARIANT builtin")
_require_text("${_tests}"
    "DEFINITIONS LUMEX_DWCAS_BACKEND=3 LUMEX_TEST_DWCAS_BACKEND_EXPECTED=3")
_require_text("${_tests}" "VARIANT msvc_wrapper")
_require_text("${_tests}"
    "DEFINITIONS LUMEX_DWCAS_BACKEND=2 LUMEX_TEST_DWCAS_BACKEND_EXPECTED=2")
_require_text("${_tests}" "msvc_stand_in")
_require_text("${_tests}" "target_link_libraries(\${_target} PRIVATE atomic)")
_require_text("${_tests}" "LUMEX_USE_TSAN")
_require_text("lumex/tests/core/atomic/CMakeLists.txt" "add_subdirectory(dwcas)")
foreach(_file LumexDwcasWord LumexDwcasConcurrent LumexDwcasCpu LumexDwcasConfig)
    if(NOT EXISTS
       "${LUMEX_SOURCE_DIR}/lumex/tests/core/atomic/dwcas/${_file}.cxx11.tests.cpp")
        message(FATAL_ERROR "lumex/tests/core/atomic/dwcas has no ${_file}.cxx11.tests.cpp")
    endif()
endforeach()
if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/core/atomic/dwcas/msvc_stand_in/intrin.h")
    message(FATAL_ERROR "the MSVC intrinsic stand-in is missing")
endif()

set(_table "lumex/tests/LumexTestStandards.cmake")
_require_text("${_table}" "lumex_test_standards_declare(atomic.dwcas 11 14 17 20)")
_require_text("${_table}"
    "lumex_test_standards_declare_variant(atomic.dwcas builtin 11 14 17 20)")
_require_text("${_table}"
    "lumex_test_standards_declare_variant(atomic.dwcas msvc_wrapper 11 14 17 20)")

# The layer is part of the module umbrella and of the installed headers.
_require_text("lumex/core/atomic/LumexAtomic" "dwcas/LumexDwcasWord.hpp")
_require_text("lumex/core/atomic/CMakeLists.txt" "PATTERN \"*.hpp\"")

# The configuration: x86-64 only, no x32, the escape hatch, the three
# backends and the rejection of a backend the compiler cannot implement.
set(_config "lumex/core/atomic/dwcas/LumexDwcasConfig.hpp")
_require_text("${_config}" "defined(LUMEX_ATOMIC_DISABLE_DWCAS)")
_require_text("${_config}" "!defined(_M_ARM64EC) && !defined(__ILP32__)")
_require_text("${_config}" "#define LUMEX_DWCAS_BACKEND_ASM 1")
_require_text("${_config}" "#define LUMEX_DWCAS_BACKEND_MSVC 2")
_require_text("${_config}" "#define LUMEX_DWCAS_BACKEND_BUILTIN 3")
_require_text("${_config}" "#define LUMEX_ATOMIC_HAS_DWCAS LUMEX_ATOMIC_DWCAS_TARGET_OK")
_require_text("${_config}" "#error \"LUMEX_DWCAS_BACKEND must be 1 (asm), 2 (msvc) or 3 (builtin)\"")

# The CPU check: CPUID leaf 1, ECX bit 13; a message and an abort.
set(_cpu "lumex/core/atomic/dwcas/LumexDwcasCpu.hpp")
_require_text("${_cpu}" "cmpxchg16b_ecx_bit = 1u << 13")
_require_text("${_cpu}" "std::abort ()")
_require_text("${_cpu}" "LUMEX_ATOMIC_DISABLE_DWCAS")

# The backends: the instruction written out, the intrinsic, the builtins with
# a type that may alias.
_require_text("lumex/core/atomic/dwcas/LumexDwcasBackendAsm.hpp"
    "lock cmpxchg16b")
_require_text("lumex/core/atomic/dwcas/LumexDwcasBackendMsvc.hpp"
    "_InterlockedCompareExchange128")
_require_text("lumex/core/atomic/dwcas/LumexDwcasBackendBuiltin.hpp"
    "may_alias")
_require_text("lumex/core/atomic/dwcas/LumexDwcasWord.hpp"
    "class alignas (16) dwcas_word")
_require_text("lumex/core/atomic/dwcas/LumexDwcasWord.hpp"
    "mutable std::uint64_t lo_;")

# The consumer fixture and its registration.
foreach(_file CMakeLists.txt check.cpp config_only.cpp)
    if(NOT EXISTS
       "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/dwcas_compile_checks/${_file}")
        message(FATAL_ERROR "consumer/dwcas_compile_checks has no ${_file}")
    endif()
endforeach()
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "-DCASE=dwcas_compile_checks")
_require_text("lumex/tests/cmake/CMakeLists.txt" "lumex_add_cmake_test(wiring_dwcas)")
_require_text("lumex/tests/cmake/consumer/dwcas_compile_checks/CMakeLists.txt"
    "-undef")

# The micro-benchmark.
_require_text("benchmarks/atomic/CMakeLists.txt" "add_executable(LumexDwcasBenchmark bench_dwcas.cpp)")
