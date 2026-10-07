# core/span wiring: a header-only INTERFACE target without a dependency edge
# (it includes the header-only macro headers of core/utility, which are
# installed whatever LUMEX_BUILD_UTILITY says), the standard suites of the
# table (C++11, C++17, C++20) in the module directory and in view/, the test
# that keeps the module's names out of the global namespace, the compile-check
# fixture, the examples, the package config alias and umbrella, the Conan
# component and the install of the headers and the extensionless umbrella.

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

set(_module "lumex/core/span/CMakeLists.txt")
_require_text("${_module}" "add_library(\${LUMEX_SPAN_NAME} INTERFACE)")
_require_text("${_module}" "add_library(lumex::span ALIAS \${LUMEX_SPAN_NAME})")
_require_text("${_module}" "if(LUMEX_INSTALL)")
_require_text("${_module}" "PATTERN \"LumexSpan\"")
_require_text("${_module}" "PATTERN \"*.hpp\"")
# Header-only and self-contained: nothing to link, no second module needed.
_forbid_text("${_module}" "target_link_libraries")

# No dependency edge: span includes only the header-only macro headers, the
# way optional does, so it builds with every other module switched off.
file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake" _modules)
string(FIND "${_modules}" "lumex_require_module(LUMEX_BUILD_SPAN" _edge)
if(NOT _edge EQUAL -1)
    message(FATAL_ERROR "LUMEX_BUILD_SPAN must not require another module")
endif()
_require_text("cmake/LumexOptions.cmake" "option(LUMEX_BUILD_SPAN ")
_require_text("cmake/LumexOptions.cmake" "core/span (header-only C++11 backport of std::span)\" ON)")

# The tests: one directory per source directory, every one with its suites.
_require_text("lumex/tests/core/CMakeLists.txt"
    "lumex_add_subdirectory_if(LUMEX_BUILD_SPAN span)")
_require_text("lumex/tests/core/span/CMakeLists.txt" "add_subdirectory(view)")
foreach(_directory "" "view/")
    set(_tests "lumex/tests/core/span/${_directory}CMakeLists.txt")
    _require_text("${_tests}" "lumex_add_standard_suites(Span")
    _require_text("${_tests}" "MODULE span")
    _require_text("${_tests}" "LINK lumex::span")
endforeach()
_require_text("lumex/tests/core/span/view/CMakeLists.txt"
    "SOURCES LumexSpanTestSupport.hpp")
foreach(_file LumexSpanGlobalNames.cxx11.tests.cpp
              LumexSpanGlobalNames.cxx20.tests.cpp
              view/LumexSpanConstruction.cxx11.tests.cpp
              view/LumexSpanConstexpr.cxx11.tests.cpp
              view/LumexSpanObservers.cxx11.tests.cpp
              view/LumexSpanSubviews.cxx11.tests.cpp
              view/LumexSpanBytes.cxx11.tests.cpp
              view/LumexSpanTraits.cxx11.tests.cpp
              view/LumexSpan.cxx17.tests.cpp
              view/LumexSpanRanges.cxx20.tests.cpp
              view/LumexSpanStdDifferential.cxx20.tests.cpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/core/span/${_file}")
        message(FATAL_ERROR "lumex/tests/core/span has no ${_file}")
    endif()
endforeach()

# The standards of the suites: C++11 (the floor), C++17 (deduction guides,
# std::byte) and C++20 (std::span, ranges); nothing else is branched on.
_require_text("lumex/tests/LumexTestStandards.cmake"
    "lumex_test_standards_declare(span 11 17 20)")

# The negative compile checks, run by the consumer runner at configure time.
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_test_name(_ctest_name span_compile_checks)")
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "-DCASE=span_compile_checks")
foreach(_file CMakeLists.txt check.cpp)
    if(NOT EXISTS
       "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/span_compile_checks/${_file}")
        message(FATAL_ERROR "span_compile_checks has no ${_file}")
    endif()
endforeach()
_require_text("lumex/tests/cmake/consumer/span_compile_checks/CMakeLists.txt"
    "set(_standards 11 17 20)")

# The header-only install case builds the module from an install prefix
# without core/utility.
_require_text("lumex/tests/cmake/consumer/install_header_only_without_utility/CMakeLists.txt"
    "set(_header_only_modules ATOMIC GENERATORS MATH OPTIONAL SPAN STRING)")
_require_text("lumex/tests/cmake/consumer/install_header_only_without_utility/main.cpp"
    "lumex/core/span/LumexSpan")

_require_text("lumex/examples/CMakeLists.txt" "add_subdirectory(span)")
_require_text("lumex/examples/span/CMakeLists.txt" "CXX_STANDARD 11")
foreach(_file example_span.cpp example_span_workflow.cpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/examples/span/${_file}")
        message(FATAL_ERROR "lumex/examples/span has no ${_file}")
    endif()
endforeach()

set(_config_in "cmake/LumexLibConfig.cmake.in")
_require_text("${_config_in}"
    "add_library(lumex::span ALIAS lumex::LumexCore_span)")
_require_text("${_config_in}" "optional reflection span string")

_require_text("conanfile.py" "\"core_span\", \"span\"")
_require_text("CMakeLists.txt" "LumexCore_span")
_require_text("cmake/LumexModules.cmake" "LumexCore_span")
