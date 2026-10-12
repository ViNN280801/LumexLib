# core/smart_ptr wiring: a header-only INTERFACE target that links
# lumex::utility (the assertion handler), the option and its dependency edge,
# the add_subdirectory order (after utility and hazard_pointer, before the
# atomic module that will link it softly), the standard suites of the table
# (C++11, 14, 17, 20) in the module directory and in ctl/, detail/, shared/,
# weak/, make/ and interop/, the test that keeps the module's names out of the
# global namespace, the compile-check fixture, the examples, the package
# config alias and umbrella, the Conan component, the install of the headers
# and the extensionless umbrella, the Doxygen page and the credit paragraph in
# THIRD-PARTY-NOTICES.md.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

set(_module "lumex/core/smart_ptr/CMakeLists.txt")
_require_text("${_module}" "add_library(\${LUMEX_SMART_PTR_NAME} INTERFACE)")
_require_text("${_module}" "add_library(lumex::smart_ptr ALIAS \${LUMEX_SMART_PTR_NAME})")
_require_text("${_module}" "lumex::utility")
_require_text("${_module}" "if(LUMEX_INSTALL)")
_require_text("${_module}" "PATTERN \"LumexSmartPtr\"")
_require_text("${_module}" "PATTERN \"*.hpp\"")
foreach(_file LumexSmartPtr README.md
              ctl/LumexSmartPtrCounter.hpp ctl/LumexSmartPtrCtlBase.hpp
              ctl/LumexSmartPtrCtlKinds.hpp
              detail/LumexSmartPtrAccess.hpp detail/LumexSmartPtrConfig.hpp
              detail/LumexSmartPtrTraits.hpp
              shared/LumexSharedPtr.hpp shared/LumexBadWeakPtr.hpp
              shared/LumexEnableSharedFromThis.hpp
              weak/LumexWeakPtr.hpp make/LumexMakeShared.hpp
              interop/LumexSmartPtrStd.hpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/core/smart_ptr/${_file}")
        message(FATAL_ERROR "lumex/core/smart_ptr has no ${_file}")
    endif()
endforeach()
# Header-only: no source file.
file(GLOB_RECURSE _sources "${LUMEX_SOURCE_DIR}/lumex/core/smart_ptr/*.cpp")
if(_sources)
    message(FATAL_ERROR "core/smart_ptr must be header-only: ${_sources}")
endif()

# The dependency edge, the option, the module lists.
_require_text("cmake/LumexModules.cmake"
    "lumex_require_module(LUMEX_BUILD_SMART_PTR LUMEX_BUILD_UTILITY)")
_require_text("cmake/LumexModules.cmake" "LUMEX_BUILD_SMART_PTR\n")
_require_text("cmake/LumexModules.cmake" "LumexCore_smart_ptr")
_require_text("cmake/LumexOptions.cmake" "option(LUMEX_BUILD_SMART_PTR ")
_require_text("cmake/LumexOptions.cmake" "with the own split-count control block, from C++11)\" ON)")
# The atomic module links smart_ptr softly (no lumex_require_module line): the
# edge must not be a hard one.
file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake" _modules)
string(FIND "${_modules}" "lumex_require_module(LUMEX_BUILD_ATOMIC LUMEX_BUILD_SMART_PTR" _atomic_edge)
if(NOT _atomic_edge EQUAL -1)
    message(FATAL_ERROR "the atomic -> smart_ptr edge is soft; no lumex_require_module line")
endif()

# The order of the add_subdirectory calls: after utility and hazard_pointer,
# before atomic.
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/CMakeLists.txt" _core)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_SMART_PTR smart_ptr)" _sp_at)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_UTILITY utility)" _utility_at)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_ATOMIC atomic)" _atomic_at)
if(_sp_at EQUAL -1)
    message(FATAL_ERROR "lumex/core/CMakeLists.txt does not add smart_ptr")
endif()
if(_sp_at LESS _utility_at)
    message(FATAL_ERROR "smart_ptr must be added after utility (it links lumex::utility)")
endif()
if(NOT _sp_at LESS _atomic_at)
    message(FATAL_ERROR "smart_ptr must be added before atomic (the soft edge)")
endif()

# The tests: one directory per source directory, every one with its suites.
_require_text("lumex/tests/core/CMakeLists.txt"
    "lumex_add_subdirectory_if(LUMEX_BUILD_SMART_PTR smart_ptr)")
foreach(_directory ctl detail shared weak make interop)
    _require_text("lumex/tests/core/smart_ptr/CMakeLists.txt" "add_subdirectory(${_directory})")
endforeach()
foreach(_directory "" "ctl/" "detail/" "shared/" "weak/" "make/" "interop/")
    set(_tests "lumex/tests/core/smart_ptr/${_directory}CMakeLists.txt")
    _require_text("${_tests}" "lumex_add_standard_suites(SmartPtr")
    _require_text("${_tests}" "MODULE smart_ptr")
    _require_text("${_tests}" "LINK lumex::smart_ptr")
    _require_text("${_tests}" "LABELS tsan")
endforeach()
_require_text("lumex/tests/core/smart_ptr/shared/CMakeLists.txt" "SOAK_FILTER *Soak*")
foreach(_file LumexSmartPtrGlobalNames.cxx11.tests.cpp
              LumexSmartPtrTestSupport.hpp LumexSmartPtrTestScenarios.hpp
              ctl/LumexSmartPtrCounter.cxx11.tests.cpp
              ctl/LumexSmartPtrCtlBase.cxx11.tests.cpp
              detail/LumexSmartPtrTraits.cxx11.tests.cpp
              detail/LumexSmartPtrAccess.cxx11.tests.cpp
              shared/LumexSharedPtrConstruction.cxx11.tests.cpp
              shared/LumexSharedPtrModifiers.cxx11.tests.cpp
              shared/LumexSharedPtrObservers.cxx11.tests.cpp
              shared/LumexSharedPtrDeleters.cxx11.tests.cpp
              shared/LumexSharedPtrCasts.cxx11.tests.cpp
              shared/LumexSharedPtrEnableSharedFromThis.cxx11.tests.cpp
              shared/LumexSharedPtrStress.cxx11.tests.cpp
              shared/LumexSharedPtrAba.cxx11.tests.cpp
              shared/LumexSharedPtr.cxx17.tests.cpp
              shared/LumexSharedPtr.cxx20.tests.cpp
              weak/LumexWeakPtr.cxx11.tests.cpp
              make/LumexMakeShared.cxx11.tests.cpp
              interop/LumexSmartPtrStd.cxx11.tests.cpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/core/smart_ptr/${_file}")
        message(FATAL_ERROR "lumex/tests/core/smart_ptr has no ${_file}")
    endif()
endforeach()
# The standards of the suites: every one of the four, because the module is
# header-only and must work at each (C++17 adds arrays' deduction guides and
# C++20 the three-way comparison).
_require_text("lumex/tests/LumexTestStandards.cmake"
    "lumex_test_standards_declare(smart_ptr 11 14 17 20)")

# The negative compile checks, run by the consumer runner at configure time.
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_test_name(_ctest_name smart_ptr_compile_checks)")
_require_text("lumex/tests/cmake/CMakeLists.txt" "-DCASE=smart_ptr_compile_checks")
foreach(_file CMakeLists.txt check.cpp)
    if(NOT EXISTS
       "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/smart_ptr_compile_checks/${_file}")
        message(FATAL_ERROR "smart_ptr_compile_checks has no ${_file}")
    endif()
endforeach()
_require_text("lumex/tests/cmake/consumer/smart_ptr_compile_checks/CMakeLists.txt"
    "set(_standards 11 17 20)")
_require_text("lumex/tests/cmake/consumer/smart_ptr_compile_checks/CMakeLists.txt"
    "LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS")

# The examples.
_require_text("lumex/examples/CMakeLists.txt" "add_subdirectory(smart_ptr)")
_require_text("lumex/examples/smart_ptr/CMakeLists.txt" "CXX_STANDARD 11")
foreach(_file example_smart_ptr.cpp example_smart_ptr_workflow.cpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/examples/smart_ptr/${_file}")
        message(FATAL_ERROR "lumex/examples/smart_ptr has no ${_file}")
    endif()
endforeach()

# Package config alias and umbrella, Conan, the export candidate list.
set(_config_in "cmake/LumexLibConfig.cmake.in")
_require_text("${_config_in}"
    "add_library(lumex::smart_ptr ALIAS lumex::LumexCore_smart_ptr)")
_require_text("${_config_in}" "span string smart_ptr")
_require_text("conanfile.py" "\"core_smart_ptr\", \"smart_ptr\"")
_require_text("CMakeLists.txt" "LumexCore_smart_ptr")

# Documentation and credits.
_require_text("Doxyfile.in" "lumex/core/smart_ptr/README.md")
_require_text("THIRD-PARTY-NOTICES.md" "## Smart pointers (Anthony Williams, Folly)")
_require_text("THIRD-PARTY-NOTICES.md" "No source text of these projects is copied")
