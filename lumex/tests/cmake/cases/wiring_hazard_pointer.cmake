# core/hazard_pointer wiring: a compiled library (one .cpp, the engine) built
# as C++11 that links utility and span, the option and the dependency edges, the
# export macro, the standard suites of the table (C++11, 14, 17, 20) in the
# module directory and in base/, holder/ and engine/, the consumer fixtures
# (test hooks, two shared libraries, static destruction, the rejected uses), the package config alias and
# umbrella, the Conan component, the install of the headers and the
# extensionless umbrella, and the credit paragraph in THIRD-PARTY-NOTICES.md.

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

set(_module "lumex/core/hazard_pointer/CMakeLists.txt")
_require_text("${_module}" "add_library(\${LUMEX_HAZARD_POINTER_NAME} \${SOURCES})")
_require_text("${_module}" "add_library(lumex::hazard_pointer ALIAS \${LUMEX_HAZARD_POINTER_NAME})")
_require_text("${_module}" "lumex::utility")
_require_text("${_module}" "lumex::span")
_require_text("${_module}" "Threads::Threads")
# Compiled as C++11 on purpose: the exported functions do not depend on the
# standard.
_require_text("${_module}" "CXX_STANDARD 11")
_require_text("${_module}" "CXX_EXTENSIONS OFF")
# The pthread key destructor lives in the library: it stays mapped.
_require_text("${_module}" "LINKER:-z,nodelete")
_require_text("${_module}" "if(LUMEX_INSTALL)")
_require_text("${_module}" "PATTERN \"LumexHazardPointer\"")
_require_text("${_module}" "PATTERN \"*.hpp\"")
foreach(_file engine/LumexHazardPointerDomain.cpp
              engine/LumexHazardPointerEngine.hpp
              base/LumexHazardPointerObjBase.hpp
              holder/LumexHazardPointerHolder.hpp
              holder/LumexHazardPointerBatch.hpp
              LumexHazardPointer README.md)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/core/hazard_pointer/${_file}")
        message(FATAL_ERROR "lumex/core/hazard_pointer has no ${_file}")
    endif()
endforeach()
# One engine source: the domain is one object per process.
file(GLOB_RECURSE _sources "${LUMEX_SOURCE_DIR}/lumex/core/hazard_pointer/*.cpp")
list(LENGTH _sources _source_count)
if(NOT _source_count EQUAL 1)
    message(FATAL_ERROR "core/hazard_pointer has ${_source_count} .cpp files, expected the one engine source")
endif()

# The dependency edges.
_require_text("cmake/LumexModules.cmake"
    "lumex_require_module(LUMEX_BUILD_HAZARD_POINTER LUMEX_BUILD_UTILITY)")
_require_text("cmake/LumexModules.cmake"
    "lumex_require_module(LUMEX_BUILD_HAZARD_POINTER LUMEX_BUILD_SPAN)")
_require_text("cmake/LumexOptions.cmake" "option(LUMEX_BUILD_HAZARD_POINTER ")
_require_text("cmake/LumexOptions.cmake" "<hazard_pointer>)\" ON)")
# The order of the add_subdirectory calls: after utility and span.
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/CMakeLists.txt" _core)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_HAZARD_POINTER hazard_pointer)" _hp_at)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_UTILITY utility)" _utility_at)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_SPAN span)" _span_at)
if(_hp_at EQUAL -1)
    message(FATAL_ERROR "lumex/core/CMakeLists.txt does not add hazard_pointer")
endif()
if(_hp_at LESS _utility_at OR _hp_at LESS _span_at)
    message(FATAL_ERROR "hazard_pointer is added before utility or span, which it links")
endif()

# The export macro, per module.
_require_text("lumex/LumexExport.hpp" "LUMEX_HAZARD_POINTER_API")
_require_text("lumex/LumexExport.hpp" "LumexCore_hazard_pointer_EXPORTS")

# The tests: one directory per source directory, every one with its suites.
# The long runs (soak, ThreadSanitizer) are a fixture outside the default run.
_require_text("cmake/LumexOptions.cmake" "option(LUMEX_BUILD_SOAK_TESTS ")
_require_text("lumex/tests/cmake/CMakeLists.txt" "if(LUMEX_BUILD_SOAK_TESTS)")
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_add_hazard_pointer_soak_test(hazard_pointer_tsan tsan")
if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/hazard_pointer_soak/main.cpp")
    message(FATAL_ERROR "no fixture hazard_pointer_soak")
endif()
_require_text("lumex/tests/core/CMakeLists.txt"
    "lumex_add_subdirectory_if(LUMEX_BUILD_HAZARD_POINTER hazard_pointer)")
foreach(_directory base holder engine)
    _require_text("lumex/tests/core/hazard_pointer/CMakeLists.txt" "add_subdirectory(${_directory})")
endforeach()
foreach(_directory "" "base/" "holder/" "engine/")
    set(_tests "lumex/tests/core/hazard_pointer/${_directory}CMakeLists.txt")
    _require_text("${_tests}" "lumex_add_standard_suites(HazardPointer")
    _require_text("${_tests}" "MODULE hazard_pointer")
    _require_text("${_tests}" "LINK lumex::hazard_pointer")
endforeach()
_require_text("lumex/tests/LumexTestStandards.cmake"
    "lumex_test_standards_declare(hazard_pointer 11 14 17 20)")
foreach(_file LumexHazardPointerGlobalNames.cxx11.tests.cpp
              LumexHazardPointerTestSupport.hpp
              LumexHazardPointerTestStructures.hpp
              base/LumexHazardPointerObjBase.cxx11.tests.cpp
              base/LumexHazardPointerObjBase.cxx17.tests.cpp
              base/LumexHazardPointerObjBase.cxx20.tests.cpp
              holder/LumexHazardPointerHolder.cxx11.tests.cpp
              holder/LumexHazardPointerBatch.cxx11.tests.cpp
              holder/LumexHazardPointerBatch.cxx20.tests.cpp
              holder/LumexHazardPointerExtension.cxx11.tests.cpp
              engine/LumexHazardPointerEngine.cxx11.tests.cpp
              engine/LumexHazardPointerStress.cxx11.tests.cpp
              engine/LumexHazardPointerAba.cxx11.tests.cpp
              engine/LumexHazardPointerNaive.cxx11.tests.cpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/core/hazard_pointer/${_file}")
        message(FATAL_ERROR "lumex/tests/core/hazard_pointer has no ${_file}")
    endif()
endforeach()

# The consumer fixtures.
foreach(_fixture hooks two_modules static_destruction)
    _require_text("lumex/tests/cmake/CMakeLists.txt"
        "lumex_add_consumer_test(consumer_hazard_pointer_${_fixture}")
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/hazard_pointer_${_fixture}/CMakeLists.txt")
        message(FATAL_ERROR "no fixture hazard_pointer_${_fixture}")
    endif()
endforeach()
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_test_name(_ctest_name hazard_pointer_compile_checks)")
_require_text("lumex/tests/cmake/CMakeLists.txt" "-DCASE=hazard_pointer_compile_checks")
foreach(_file CMakeLists.txt check.cpp)
    if(NOT EXISTS
       "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/hazard_pointer_compile_checks/${_file}")
        message(FATAL_ERROR "hazard_pointer_compile_checks has no ${_file}")
    endif()
endforeach()
_require_text("lumex/tests/cmake/consumer/hazard_pointer_compile_checks/CMakeLists.txt"
    "set(_standards 11 17 20)")
# The hooks fixture builds the engine itself with the hooks switched on and
# does not link the library of the tree.
_require_text("lumex/tests/cmake/consumer/hazard_pointer_hooks/CMakeLists.txt"
    "LUMEX_HAZARD_POINTER_TEST_HOOKS")
# The two-libraries fixture shares the library, with hidden visibility in the
# two libraries.
_require_text("lumex/tests/cmake/consumer/hazard_pointer_two_modules/CMakeLists.txt"
    "CMAKE_CXX_VISIBILITY_PRESET hidden")
# The package config alias and umbrella, the Conan component.
set(_config_in "cmake/LumexLibConfig.cmake.in")
_require_text("${_config_in}"
    "add_library(lumex::hazard_pointer ALIAS lumex::LumexCore_hazard_pointer)")
_require_text("${_config_in}" "fmt hazard_pointer")
_require_text("conanfile.py" "\"core_hazard_pointer\", \"hazard_pointer\"")
_require_text("conanfile.py" "[\"core_utility\", \"core_span\"]")
_require_text("CMakeLists.txt" "LumexCore_hazard_pointer")
_require_text("cmake/LumexModules.cmake" "LumexCore_hazard_pointer")

# The credit paragraph for the designs the engine follows (ideas only).
_require_text("THIRD-PARTY-NOTICES.md" "Hazard pointers")
_require_text("THIRD-PARTY-NOTICES.md" "No source text of these projects is copied")
