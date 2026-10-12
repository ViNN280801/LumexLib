# core/contracts wiring: a compiled library (one .cpp, the handler slot and the
# default handler) built as C++11 that links utility, the option, the choice of
# the evaluation semantic by the cache variable LUMEX_CONTRACTS_SEMANTIC and the
# definition that carries it to the consumers of the target, the dependency
# edge, the export macro, the standard suites of the table (C++11, 14, 17, 20)
# in the module directory and in location/, violation/, handler/ and assert/,
# the consumer fixtures, the package config alias and umbrella, the Conan
# component, the install of the headers and the extensionless umbrella, and the
# credit paragraph for Boost in THIRD-PARTY-NOTICES.md.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_forbid_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "${path} mentions ${needle}")
    endif()
endfunction()

set(_module "lumex/core/contracts/CMakeLists.txt")
_require_text("${_module}" "add_library(\${LUMEX_CONTRACTS_NAME} \${SOURCES})")
_require_text("${_module}" "add_library(lumex::contracts ALIAS \${LUMEX_CONTRACTS_NAME})")
_require_text("${_module}" "lumex::utility")
# Compiled as C++11 on purpose: the exported functions do not depend on the
# standard.
_require_text("${_module}" "CXX_STANDARD 11")
_require_text("${_module}" "CXX_EXTENSIONS OFF")
# The semantic of the build reaches the consumers of the target.
_require_text("${_module}"
    "LUMEX_CONTRACTS_BUILD_SEMANTIC=\${LUMEX_CONTRACTS_SEMANTIC_NORMALIZED}")
_require_text("${_module}" "if(LUMEX_INSTALL)")
_require_text("${_module}" "PATTERN \"LumexContracts\"")
_require_text("${_module}" "PATTERN \"*.hpp\"")
foreach(_file handler/LumexContractsHandler.cpp
              handler/LumexContractsHandler.hpp
              violation/LumexContractsViolation.hpp
              location/LumexContractsSourceLocation.hpp
              assert/LumexContractsAssert.hpp
              LumexContracts README.md)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/core/contracts/${_file}")
        message(FATAL_ERROR "lumex/core/contracts has no ${_file}")
    endif()
endforeach()
# One compiled source: the handler slot is one object per process.
file(GLOB_RECURSE _sources "${LUMEX_SOURCE_DIR}/lumex/core/contracts/*.cpp")
list(LENGTH _sources _source_count)
if(NOT _source_count EQUAL 1)
    message(FATAL_ERROR "core/contracts has ${_source_count} .cpp files, expected the one handler source")
endif()

# The option, its values, the dependency edge.
_require_text("cmake/LumexModules.cmake"
    "lumex_require_module(LUMEX_BUILD_CONTRACTS LUMEX_BUILD_UTILITY)")
_require_text("cmake/LumexOptions.cmake" "option(LUMEX_BUILD_CONTRACTS ")
_require_text("cmake/LumexOptions.cmake" "from C++11)\" ON)")
_require_text("cmake/LumexOptions.cmake" "set(LUMEX_CONTRACTS_SEMANTIC \"ENFORCE\" CACHE STRING")
_require_text("cmake/LumexOptions.cmake"
    "IGNORE OBSERVE ENFORCE QUICK_ENFORCE P2900")
_require_text("cmake/LumexModules.cmake" "Invalid LUMEX_CONTRACTS_SEMANTIC=")
_require_text("cmake/LumexModules.cmake" "LUMEX_CONTRACTS_SEMANTIC_NORMALIZED")
# The order of the add_subdirectory calls: after utility.
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/CMakeLists.txt" _core)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_CONTRACTS contracts)" _contracts_at)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_UTILITY utility)" _utility_at)
if(_contracts_at EQUAL -1)
    message(FATAL_ERROR "lumex/core/CMakeLists.txt does not add contracts")
endif()
if(_contracts_at LESS _utility_at)
    message(FATAL_ERROR "contracts is added before utility, which it links")
endif()

# The export macro, per module.
_require_text("lumex/LumexExport.hpp" "LUMEX_CONTRACTS_API")
_require_text("lumex/LumexExport.hpp" "LumexCore_contracts_EXPORTS")

# The tests: one directory per source directory, every one with its suites.
_require_text("lumex/tests/core/CMakeLists.txt"
    "lumex_add_subdirectory_if(LUMEX_BUILD_CONTRACTS contracts)")
foreach(_directory location violation handler assert)
    _require_text("lumex/tests/core/contracts/CMakeLists.txt" "add_subdirectory(${_directory})")
endforeach()
foreach(_directory "" "location/" "violation/" "handler/" "assert/")
    set(_tests "lumex/tests/core/contracts/${_directory}CMakeLists.txt")
    _require_text("${_tests}" "lumex_add_standard_suites(Contracts")
    _require_text("${_tests}" "MODULE contracts")
    _require_text("${_tests}" "LINK lumex::contracts")
endforeach()
_require_text("lumex/tests/LumexTestStandards.cmake"
    "lumex_test_standards_declare(contracts 11 14 17 20)")
foreach(_file LumexContractsGlobalNames.cxx11.tests.cpp
              LumexContractsTestSupport.hpp
              location/LumexContractsSourceLocation.cxx11.tests.cpp
              location/LumexContractsSourceLocation.cxx20.tests.cpp
              violation/LumexContractsViolation.cxx11.tests.cpp
              handler/LumexContractsHandler.cxx11.tests.cpp
              assert/LumexContractsAssert.cxx11.tests.cpp
              assert/LumexContractsAssert.cxx14.tests.cpp
              assert/LumexContractsAssertDeath.cxx11.tests.cpp
              assert/LumexContractsAssertOverrideIgnore.cxx11.tests.cpp
              assert/LumexContractsAssertOverrideObserve.cxx11.tests.cpp
              assert/LumexContractsAssertOverrideEnforce.cxx11.tests.cpp
              assert/LumexContractsAssertOverrideQuickEnforce.cxx11.tests.cpp
              assert/LumexContractsAssertOverrideP2900Fallback.cxx11.tests.cpp
              assert/LumexContractsAssertNative.cxx11.tests.cpp
              assert/LumexContractsAssertNativeOtherSemantic.cxx11.tests.cpp
              assert/LumexContractsAssertWithdrawnContracts.cxx11.tests.cpp
              assert/LumexContractsAssertBuildDefault.cxx11.tests.cpp
              assert/LumexContractsAssertNdebug.cxx11.tests.cpp
              assert/LumexContractsAssertNoNdebug.cxx11.tests.cpp
              assert/LumexContractsAssertCatch.cxx11.tests.cpp
              assert/LumexContractsAssertNoCatch.cxx11.tests.cpp
              assert/LumexContractsAssertNoexcept.cxx11.tests.cpp
              assert/LumexContractsAssertConstexpr.cxx11.tests.cpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/core/contracts/${_file}")
        message(FATAL_ERROR "lumex/tests/core/contracts has no ${_file}")
    endif()
endforeach()

# The consumer fixtures.
foreach(_semantic ignore observe enforce quick_enforce p2900)
    _require_text("lumex/tests/cmake/CMakeLists.txt"
        "lumex_add_consumer_test(consumer_contracts_build_semantic_\${_semantic}")
endforeach()
_require_text("lumex/tests/cmake/CMakeLists.txt" "ignore observe enforce quick_enforce p2900")
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_add_consumer_test(consumer_contracts_two_modules contracts_two_modules)")
foreach(_fixture contracts_build_semantic contracts_two_modules)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/${_fixture}/CMakeLists.txt")
        message(FATAL_ERROR "no fixture ${_fixture}")
    endif()
endforeach()
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_test_name(_ctest_name contracts_compile_checks)")
_require_text("lumex/tests/cmake/CMakeLists.txt" "-DCASE=contracts_compile_checks")
foreach(_file CMakeLists.txt check.cpp)
    if(NOT EXISTS
       "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/contracts_compile_checks/${_file}")
        message(FATAL_ERROR "contracts_compile_checks has no ${_file}")
    endif()
endforeach()
_require_text("lumex/tests/cmake/consumer/contracts_compile_checks/CMakeLists.txt"
    "set(_standards 11 17 20)")
# The two-modules fixture shares the library, with hidden visibility in the
# library.
_require_text("lumex/tests/cmake/consumer/contracts_two_modules/CMakeLists.txt"
    "CMAKE_CXX_VISIBILITY_PRESET hidden")
# The package config alias and umbrella, the Conan component.
set(_config_in "cmake/LumexLibConfig.cmake.in")
_require_text("${_config_in}"
    "add_library(lumex::contracts ALIAS lumex::LumexCore_contracts)")
_require_text("${_config_in}" "circular_buffer contracts crc")
_require_text("conanfile.py" "\"core_contracts\", \"contracts\"")
_require_text("conanfile.py" "[\"LumexCore_contracts\"], [\"core_utility\"]")
_require_text("CMakeLists.txt" "LumexCore_contracts")
_require_text("cmake/LumexModules.cmake" "LumexCore_contracts")

# The module does not touch LUMEX_ASSERT of the utility module.
_forbid_text("lumex/core/contracts/assert/LumexContractsAssert.hpp" "lumex_assert_handler")

# The credit paragraph for the designs the module follows (ideas only).
_require_text("THIRD-PARTY-NOTICES.md" "Boost.Assert and Boost.Contract")
_require_text("THIRD-PARTY-NOTICES.md" "Boost Software License 1.0")
