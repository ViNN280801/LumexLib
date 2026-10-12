# The CMake target namespace must stay lowercase `lumex::`, matching the C++
# namespace. Target, package, and export file names stay CamelCase on purpose
# (`LumexCore_*`, `LumexLib`, `LumexLibConfig.cmake`), so this case pins the
# namespace token only.

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
        message(FATAL_ERROR "${path} still mentions ${needle}")
    endif()
endfunction()

set(_modules_cmake "cmake/LumexModules.cmake")
set(_config_in "cmake/LumexLibConfig.cmake.in")
set(_gtest_cmake "cmake/LumexGoogleTest.cmake")
set(_helpers_cmake "lumex/examples/cmake/LumexExampleHelpers.cmake")
set(_utility_tests_cmake "lumex/tests/core/utility/CMakeLists.txt")

# Assembled at runtime so this case file does not itself contain the old
# spelling that workspace-wide audits grep for.
string(CONCAT _old_ns "Lumex" "::")

_require_text("${_modules_cmake}" "NAMESPACE lumex::")
_forbid_text("${_modules_cmake}" "NAMESPACE ${_old_ns}")
_forbid_text("${_modules_cmake}" "${_old_ns}")

_require_text("${_config_in}" "lumex::Lumex")
_require_text("${_config_in}" "add_library(lumex::utility ALIAS lumex::LumexCore_utility)")
_require_text("${_config_in}" "check_required_components(LumexLib)")
_forbid_text("${_config_in}" "${_old_ns}")

_require_text("${_gtest_cmake}" "lumex::gtest_main_cxx11")
_require_text("${_gtest_cmake}" "lumex::gtest_main_cxx17")
_forbid_text("${_gtest_cmake}" "${_old_ns}")

_require_text("${_helpers_cmake}" "lumex::")
_forbid_text("${_helpers_cmake}" "${_old_ns}")

_require_text("${_utility_tests_cmake}" "lumex::utility")
_forbid_text("${_utility_tests_cmake}" "${_old_ns}")
