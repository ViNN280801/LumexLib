# core/expected global names: the umbrella has no global `unexpected` (the
# MinGW runtime declares a function of that name in <eh.h>) and no global
# `in_place`; the other global names stay. The negative compile-check fixture
# and the test with a program's own global `unexpected` are registered.

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

# No header puts the class or the tag into the global namespace: the
# using-declarations that stay are the ones listed below. The using-declaration
# of `unexpected` inside lumex::core::expected::result is not global and stays.
set(_headers
    lumex/core/expected/error/Unexpected.hpp
    lumex/core/expected/error/BadExpectedAccess.hpp
    lumex/core/expected/result/Expected.hpp
    lumex/core/expected/result/ExpectedTypes.hpp
    lumex/core/expected/result/ExpectedVoid.hpp
    lumex/core/expected/result/SuccessFailure.hpp)
foreach(_header IN LISTS _headers)
    _forbid_text("${_header}" "\nusing lumex::core::expected::error::unexpected;")
    _forbid_text("${_header}" "\nusing lumex::core::expected::result::in_place;")
endforeach()
_require_text("lumex/core/expected/result/Expected.hpp" "\nusing error::unexpected;")

# The global names that stay.
_require_text("lumex/core/expected/error/BadExpectedAccess.hpp"
    "\nusing lumex::core::expected::error::bad_expected_access;")
_require_text("lumex/core/expected/result/Expected.hpp"
    "\nusing lumex::core::expected::result::expected;")
_require_text("lumex/core/expected/result/Expected.hpp"
    "\nusing lumex::core::expected::result::make_unexpected;")
_require_text("lumex/core/expected/result/ExpectedTypes.hpp"
    "\nusing lumex::core::expected::result::in_place_tag;")
_require_text("lumex/core/expected/result/ExpectedTypes.hpp"
    "\nusing lumex::core::expected::result::unexpect;")
_require_text("lumex/core/expected/result/ExpectedTypes.hpp"
    "\nusing lumex::core::expected::result::unexpect_t;")

# The test with a program's own global `unexpected` next to the umbrella.
_require_text("lumex/tests/core/expected/error/ExpectedGlobalNames.cxx11.tests.cpp"
    "lumex/core/expected/Expected")

# The negative compile checks, run by the consumer runner at configure time.
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_test_name(_ctest_name expected_compile_checks)")
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "-DCASE=expected_compile_checks")
foreach(_file CMakeLists.txt check.cpp)
    if(NOT EXISTS
       "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/expected_compile_checks/${_file}")
        message(FATAL_ERROR "expected_compile_checks has no ${_file}")
    endif()
endforeach()
_require_text("lumex/tests/cmake/consumer/expected_compile_checks/CMakeLists.txt"
    "set(_standards 11 17 20)")
_require_text("lumex/tests/cmake/consumer/expected_compile_checks/check.cpp"
    "LUMEX_EXPECTED_BAD_CASE")
