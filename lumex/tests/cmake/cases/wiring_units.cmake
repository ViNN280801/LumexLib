# core/units wiring: a header-only INTERFACE target that links expected, math,
# string_view and utility, the option and its four dependency edges, the
# add_subdirectory order (after expected, math, string_view and utility), the
# standard suites of the table (C++11, 14, 17, 20) in the module directory, in
# data/ and in api/, the oracle and its CTest, the examples, the package config
# alias and umbrella, the Conan component, the install of the headers and the
# extensionless umbrella and the Doxygen page.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

set(_module "lumex/core/units/CMakeLists.txt")
_require_text("${_module}" "add_library(\${LUMEX_UNITS_NAME} INTERFACE)")
_require_text("${_module}" "add_library(lumex::units ALIAS \${LUMEX_UNITS_NAME})")
foreach(_dependency expected math string_view utility)
    _require_text("${_module}" "lumex::${_dependency}")
endforeach()
_require_text("${_module}" "if(LUMEX_INSTALL)")
_require_text("${_module}" "PATTERN \"LumexUnits\"")
_require_text("${_module}" "PATTERN \"*.hpp\"")
foreach(_file LumexUnits README.md data/LumexUnitsData.hpp
              api/LumexUnitsApi.hpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/core/units/${_file}")
        message(FATAL_ERROR "lumex/core/units has no ${_file}")
    endif()
endforeach()
# Header-only: no source file.
file(GLOB_RECURSE _sources "${LUMEX_SOURCE_DIR}/lumex/core/units/*.cpp")
if(_sources)
    message(FATAL_ERROR "core/units must be header-only: ${_sources}")
endif()

# The data header uses nothing of the library: only <cstddef> and <cstdint>,
# and no LUMEX_ macro of another module.
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/units/data/LumexUnitsData.hpp" _data)
string(REGEX MATCHALL "#include [<\"][^>\"]*[>\"]" _data_includes "${_data}")
foreach(_include IN LISTS _data_includes)
    if(NOT _include MATCHES "^#include <(cstddef|cstdint)>$")
        message(FATAL_ERROR
            "lumex/core/units/data/LumexUnitsData.hpp includes ${_include}: "
            "the data header may use <cstddef> and <cstdint> only")
    endif()
endforeach()
string(REGEX MATCHALL "LUMEX_(NOEXCEPT|CONSTEXPR|ATTRIBUTE|ASSERT|HAS)[A-Z_]*"
    _library_macros "${_data}")
if(_library_macros)
    message(FATAL_ERROR
        "lumex/core/units/data/LumexUnitsData.hpp uses library macros: "
        "${_library_macros}")
endif()

# The dependency edges, the option, the module lists.
foreach(_dependency EXPECTED MATH STRING_VIEW UTILITY)
    _require_text("cmake/LumexModules.cmake"
        "lumex_require_module(LUMEX_BUILD_UNITS LUMEX_BUILD_${_dependency})")
endforeach()
_require_text("cmake/LumexModules.cmake" "LUMEX_BUILD_UNITS\n")
_require_text("cmake/LumexModules.cmake" "LumexCore_units")
_require_text("cmake/LumexOptions.cmake" "option(LUMEX_BUILD_UNITS ")

# The order of the add_subdirectory calls: after expected (which brings
# utility), math and string_view.
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/CMakeLists.txt" _core)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_UNITS units)" _units_at)
foreach(_other MATH SPAN OPTIONAL UTILITY STRING_VIEW EXPECTED)
    string(TOLOWER "${_other}" _directory)
    string(FIND "${_core}"
        "lumex_add_subdirectory_if(LUMEX_BUILD_${_other} ${_directory})" _other_at)
    if(_units_at EQUAL -1 OR _other_at EQUAL -1 OR NOT _units_at GREATER _other_at)
        message(FATAL_ERROR
            "units must be added after ${_directory} (it links it)")
    endif()
endforeach()

# The tests: the module directory and its two subdirectories, each with its
# suites, the oracle and its CTest.
_require_text("lumex/tests/core/CMakeLists.txt"
    "lumex_add_subdirectory_if(LUMEX_BUILD_UNITS units)")
foreach(_directory data api)
    _require_text("lumex/tests/core/units/CMakeLists.txt"
        "add_subdirectory(${_directory})")
endforeach()
foreach(_directory "" "data/" "api/")
    set(_tests "lumex/tests/core/units/${_directory}CMakeLists.txt")
    _require_text("${_tests}" "lumex_add_standard_suites(Units")
    _require_text("${_tests}" "MODULE units")
    _require_text("${_tests}" "LINK lumex::units")
endforeach()
foreach(_file LumexUnitsTestSupport.hpp LumexUnitsGlobalNames.cxx11.tests.cpp
              data/LumexUnitsData.cxx11.tests.cpp
              data/LumexUnitsOracle.cxx11.tests.cpp
              api/LumexUnitsLookup.cxx11.tests.cpp
              api/LumexUnitsLookup.cxx20.tests.cpp
              api/LumexUnitsConvert.cxx11.tests.cpp
              oracle/oracle_units.py oracle/LumexUnitsOracleData.hpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/core/units/${_file}")
        message(FATAL_ERROR "lumex/tests/core/units has no ${_file}")
    endif()
endforeach()
_require_text("lumex/tests/LumexTestStandards.cmake"
    "lumex_test_standards_declare(units 11 14 17 20)")
_require_text("lumex/tests/CMakeLists.txt" "set(_ctest_name units.oracle_up_to_date)")
_require_text("lumex/tests/CMakeLists.txt" "add_test(NAME lint.units_ascii")
if(NOT EXISTS "${LUMEX_SOURCE_DIR}/Scripts/CodeTools/check_ascii_files.py")
    message(FATAL_ERROR "Scripts/CodeTools/check_ascii_files.py is missing")
endif()

# The examples.
_require_text("lumex/examples/CMakeLists.txt" "add_subdirectory(units)")
_require_text("lumex/examples/units/CMakeLists.txt" "CXX_STANDARD 11")
foreach(_file example_units.cpp example_units_workflow.cpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/examples/units/${_file}")
        message(FATAL_ERROR "lumex/examples/units has no ${_file}")
    endif()
endforeach()

# Package config alias and umbrella, Conan, the export candidate list.
set(_config_in "cmake/LumexLibConfig.cmake.in")
_require_text("${_config_in}"
    "add_library(lumex::units ALIAS lumex::LumexCore_units)")
_require_text("${_config_in}" "unicode units utility xml")
_require_text("conanfile.py" "\"core_units\", \"units\"")
_require_text("CMakeLists.txt" "LumexCore_units")

# Documentation.
_require_text("Doxyfile.in" "lumex/core/units/README.md")
