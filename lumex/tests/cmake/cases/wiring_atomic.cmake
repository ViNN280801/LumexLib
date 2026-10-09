# core/atomic wiring: a header-only target that links Threads::Threads (and
# the compiled lumex::hazard_pointer through a soft edge: the lock-free engine
# exists when that target does), the test suites of each test directory (C++11,
# C++17, C++20, the forced lock-based engine at C++11 and C++20, the forced
# wait table and the build without the lock-free engine) built by
# lumex_add_standard_suites from the table of
# lumex/tests/LumexTestStandards.cmake under the CTest prefixes "atomic.",
# "atomic.smart_ptr." and "atomic.sync." (derived from the directory, pinned by
# wiring_test_names), the test that keeps the module's names out of the global
# namespace in a C++11 file (so every suite compiles it), the examples, the
# package config alias and umbrella, the Conan component and the documented
# switches.

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

# The tests follow the source tree: the module's directory (the global names
# test) and the directories smart_ptr and sync each build the suites of the
# table, the forcing variants included.
_require_text("lumex/tests/core/atomic/CMakeLists.txt" "add_subdirectory(smart_ptr)")
_require_text("lumex/tests/core/atomic/CMakeLists.txt" "add_subdirectory(sync)")
foreach(_directory "" "smart_ptr/" "sync/")
    set(_tests "lumex/tests/core/atomic/${_directory}CMakeLists.txt")
    _require_text("${_tests}" "lumex_add_standard_suites(Atomic")
    _require_text("${_tests}" "MODULE atomic")
    _require_text("${_tests}" "LINK lumex::atomic")
    _require_text("${_tests}"
        "VARIANT lock_based DEFINITIONS LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED")
    _require_text("${_tests}"
        "VARIANT wait_table DEFINITIONS LUMEX_ATOMIC_WAIT_FORCE_TABLE")
    _require_text("${_tests}"
        "VARIANT no_lock_free DEFINITIONS LUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE")
    _require_text("${_tests}" "VARIANT lock_free DEFINITIONS")
    _require_text("${_tests}" "VARIANT lock_free_deferred DEFINITIONS")
    _require_text("${_tests}" "VARIANT std_backed DEFINITIONS")
    _require_text("${_tests}" "LUMEX_ATOMIC_TEST_SHARED_ENGINE=atomic_shared_ptr_lock_free")
    _require_text("${_tests}" "PROPERTIES TIMEOUT")
endforeach()
if(NOT EXISTS
   "${LUMEX_SOURCE_DIR}/lumex/tests/core/atomic/LumexAtomicGlobalNames.cxx11.tests.cpp")
    message(FATAL_ERROR
        "lumex/tests/core/atomic has no LumexAtomicGlobalNames.cxx11.tests.cpp")
endif()
foreach(_file smart_ptr/LumexAtomicSharedPtr.cxx11.tests.cpp
              sync/LumexAtomicWait.cxx11.tests.cpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/core/atomic/${_file}")
        message(FATAL_ERROR "lumex/tests/core/atomic has no ${_file}")
    endif()
endforeach()

# The standards of the suites: C++11, C++17 and C++20, the two forcing
# variants at C++20 only.
set(_table "lumex/tests/LumexTestStandards.cmake")
_require_text("${_table}" "lumex_test_standards_declare(atomic 11 17 20)")
_require_text("${_table}"
    "lumex_test_standards_declare_variant(atomic lock_based 11 20)")
_require_text("${_table}"
    "lumex_test_standards_declare_variant(atomic wait_table 20)")
_require_text("${_table}"
    "lumex_test_standards_declare_variant(atomic no_lock_free 11 20)")
_require_text("${_table}"
    "lumex_test_standards_declare_variant(atomic lock_free 11 20)")
_require_text("${_table}"
    "lumex_test_standards_declare_variant(atomic lock_free_deferred 11 20)")
_require_text("${_table}"
    "lumex_test_standards_declare_variant(atomic std_backed 20)")

_require_text("lumex/tests/core/CMakeLists.txt"
    "lumex_add_subdirectory_if(LUMEX_BUILD_ATOMIC atomic)")
_require_text("lumex/examples/CMakeLists.txt" "add_subdirectory(atomic)")
_require_text("lumex/examples/atomic/CMakeLists.txt" "CXX_STANDARD 11")

set(_config_in "cmake/LumexLibConfig.cmake.in")
_require_text("${_config_in}"
    "add_library(lumex::atomic ALIAS lumex::LumexCore_atomic)")
_require_text("${_config_in}" "atomic base64 circular_buffer")

_require_text("conanfile.py" "\"core_atomic\", \"atomic\"")
# The lock-free engine is built on core/hazard_pointer: a soft edge in CMake
# (the target is linked, and the definition set, only when it exists), a plain
# requirement in the Conan package. There is no lumex_require_module line.
_require_text("${_module}" "if(TARGET lumex::hazard_pointer)")
_require_text("${_module}" "LUMEX_ATOMIC_HAS_HAZARD_POINTER=1")
_require_text("conanfile.py" "requires=[\"core_hazard_pointer\"]")
_require_text("lumex/core/CMakeLists.txt"
    "lumex_add_subdirectory_if(LUMEX_BUILD_HAZARD_POINTER hazard_pointer)")
file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake" _modules_text)
string(FIND "${_modules_text}" "lumex_require_module(LUMEX_BUILD_ATOMIC" _atomic_requires)
if(NOT _atomic_requires EQUAL -1)
    message(FATAL_ERROR "atomic -> hazard_pointer is a soft edge: LumexModules.cmake must not require a module for LUMEX_BUILD_ATOMIC")
endif()
_require_text("lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
    "defined(LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED)")
_require_text("lumex/core/atomic/sync/LumexAtomicWait.hpp"
    "defined(LUMEX_ATOMIC_WAIT_FORCE_TABLE)")
_require_text("lumex/core/atomic/sync/LumexAtomicWait.hpp"
    "__attribute__ ((visibility (\"default\")))")
