# core/utility/numeric ordering wiring: the header with the three ordering
# classes is in the utility umbrella and is included by the safe comparator,
# the safe comparator has no C++20 guard around its three-way part, the tests
# of the utility module start at C++11 and the compile-check fixture is
# registered.

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

set(_ordering "lumex/core/utility/numeric/LumexOrdering.hpp")
set(_comparator "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp")

# The classes exist in every standard; the alias picks the standard ones only
# where <compare> exists.
foreach(_class strong_ordering weak_ordering partial_ordering)
    _require_text("${_ordering}" "class ${_class}")
    _require_text("${_ordering}" "using ${_class}_t")
endforeach()
_require_text("${_ordering}" "#if LUMEX_HAS_THREE_WAY_COMPARISON")
_require_text("${_ordering}" "LUMEX_ORDERING_LINK_ONCE")

# The directory has two headers now, so each include guard names its file.
_require_text("${_ordering}" "#ifndef LUMEX_CORE_UTILITY_NUMERIC_ORDERING_HPP")
_require_text("${_comparator}"
    "#ifndef LUMEX_CORE_UTILITY_NUMERIC_SAFE_NUMERIC_COMPARATOR_HPP")

# The umbrella and the comparator include the header; the comparator's
# three-way part is not guarded by the C++20 macro any more.
_require_text("lumex/core/utility/LumexUtility"
    "#include \"numeric/LumexOrdering.hpp\"")
_require_text("${_comparator}"
    "#include \"lumex/core/utility/numeric/LumexOrdering.hpp\"")
_forbid_text("${_comparator}" "#if LUMEX_HAS_THREE_WAY_COMPARISON")
_forbid_text("${_comparator}" "@since C++20")
_forbid_text("${_comparator}" "std::strong_ordering::")
_forbid_text("${_comparator}" "std::partial_ordering::")

# The tests of the directory run from C++11 and the shared value tables are
# listed with the suites.
foreach(_file
        LumexOrdering.cxx11.tests.cpp
        LumexOrderingLinkage.cxx11.tests.cpp
        LumexOrdering.cxx20.tests.cpp
        LumexSafeThreeWayCompare.cxx11.tests.cpp
        LumexSafeThreeWayCompare.cxx20.tests.cpp
        LumexNumericSamples.hpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/core/utility/numeric/${_file}")
        message(FATAL_ERROR "lumex/tests/core/utility/numeric has no ${_file}")
    endif()
endforeach()
_require_text("lumex/tests/core/utility/numeric/CMakeLists.txt"
    "SOURCES LumexNumericSamples.hpp")

# The negative compile checks, run by the consumer runner at configure time.
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_test_name(_ctest_name ordering_compile_checks)")
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "-DCASE=ordering_compile_checks")
foreach(_file CMakeLists.txt check.cpp)
    if(NOT EXISTS
       "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/ordering_compile_checks/${_file}")
        message(FATAL_ERROR "ordering_compile_checks has no ${_file}")
    endif()
endforeach()
_require_text("lumex/tests/cmake/consumer/ordering_compile_checks/CMakeLists.txt"
    "set(_standards 11 17 20)")
