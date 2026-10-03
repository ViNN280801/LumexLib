# core/atomic wiring: a header-only target that links Threads::Threads, five
# test suites (C++11, C++17, C++20, and C++20 with the forced lock-based
# implementation or the forced wait table) built by lumex_add_standard_suites
# from the table of lumex/tests/LumexTestStandards.cmake under the CTest
# prefix "atomic." (derived from the directory, pinned by wiring_test_names),
# the test that keeps the module's names out of the global namespace in a
# C++11 file (so every suite compiles it), the examples, the package config
# alias and umbrella, the Conan component and the documented switches.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

set(_module "lumex/core/atomic/CMakeLists.txt")
_require_text("${_module}" "add_library(\${LUMEX_ATOMIC_NAME} INTERFACE)")
_require_text("${_module}" "add_library(lumex::atomic ALIAS \${LUMEX_ATOMIC_NAME})")
_require_text("${_module}" "find_package(Threads REQUIRED)")
_require_text("${_module}" "Threads::Threads")
_require_text("${_module}" "if(LUMEX_INSTALL)")
_require_text("${_module}" "PATTERN \"LumexAtomic\"")

set(_tests "lumex/tests/core/atomic/CMakeLists.txt")
_require_text("${_tests}" "lumex_add_standard_suites(Atomic")
_require_text("${_tests}" "MODULE atomic")
_require_text("${_tests}" "LINK lumex::atomic")
_require_text("${_tests}"
    "VARIANT lock_based DEFINITIONS LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED")
_require_text("${_tests}"
    "VARIANT wait_table DEFINITIONS LUMEX_ATOMIC_WAIT_FORCE_TABLE")
_require_text("${_tests}" "PROPERTIES TIMEOUT")
if(NOT EXISTS
   "${LUMEX_SOURCE_DIR}/lumex/tests/core/atomic/LumexAtomicGlobalNames.cxx11.tests.cpp")
    message(FATAL_ERROR
        "lumex/tests/core/atomic has no LumexAtomicGlobalNames.cxx11.tests.cpp")
endif()

# The standards of the suites: C++11, C++17 and C++20, the two forcing
# variants at C++20 only.
set(_table "lumex/tests/LumexTestStandards.cmake")
_require_text("${_table}" "lumex_test_standards_declare(atomic 11 17 20)")
_require_text("${_table}"
    "lumex_test_standards_declare_variant(atomic lock_based 20)")
_require_text("${_table}"
    "lumex_test_standards_declare_variant(atomic wait_table 20)")

_require_text("lumex/tests/core/CMakeLists.txt"
    "lumex_add_subdirectory_if(LUMEX_BUILD_ATOMIC atomic)")
_require_text("lumex/examples/CMakeLists.txt" "add_subdirectory(atomic)")
_require_text("lumex/examples/atomic/CMakeLists.txt" "CXX_STANDARD 11")

set(_config_in "cmake/LumexLibConfig.cmake.in")
_require_text("${_config_in}"
    "add_library(lumex::atomic ALIAS lumex::LumexCore_atomic)")
_require_text("${_config_in}" "atomic base64 circular_buffer")

_require_text("conanfile.py" "\"core_atomic\", \"atomic\"")
_require_text("lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
    "defined(LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED)")
_require_text("lumex/core/atomic/sync/LumexAtomicWait.hpp"
    "defined(LUMEX_ATOMIC_WAIT_FORCE_TABLE)")
_require_text("lumex/core/atomic/sync/LumexAtomicWait.hpp"
    "__attribute__ ((visibility (\"default\")))")
