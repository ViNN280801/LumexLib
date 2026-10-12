# reflection CMakeLists and sources must use LumexAggregateFields +
# vendored nlohmann, never Boost.PFR.

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

set(_ref_cmake "lumex/core/reflection/CMakeLists.txt")
set(_ref_hdr "lumex/core/reflection/field_reflection/LumexFieldReflection.hpp")
set(_agg_hdr "lumex/core/reflection/field_reflection/LumexAggregateFields.hpp")

_require_text("${_ref_cmake}" "lumex_setup_nlohmann_json")
_require_text("${_ref_cmake}" "LUMEX_WITH_FIELD_REFLECTION")
_require_text("${_ref_cmake}" "3rdparty")
_require_text("${_ref_cmake}" "nlohmann")
_forbid_text("${_ref_cmake}" "boost_pfr")
_forbid_text("${_ref_cmake}" "boost/pfr")

_require_text("${_ref_hdr}" "LumexAggregateFields.hpp")
_require_text("${_ref_hdr}" "<nlohmann/json.hpp>")
_forbid_text("${_ref_hdr}" "boost/pfr")
_forbid_text("${_ref_hdr}" "boost::pfr")

_require_text("${_agg_hdr}" "tuple_size_v")
_require_text("${_agg_hdr}" "names_as_array")
_require_text("${_agg_hdr}" "k_max_aggregate_fields")
_require_text("${_agg_hdr}" "LUMEX_DEFINE_FIELD_NAMES")
_forbid_text("${_agg_hdr}" "boost/pfr")
_forbid_text("${_agg_hdr}" "boost::pfr")

# The reflection tests follow the source tree: the field reflection tests have
# a directory of their own, added only with the option and the nlohmann target.
_require_text("lumex/tests/core/reflection/CMakeLists.txt"
    "if(LUMEX_WITH_FIELD_REFLECTION AND TARGET nlohmann_json::nlohmann_json)")
_require_text("lumex/tests/core/reflection/CMakeLists.txt"
    "add_subdirectory(field_reflection)")
_require_text("lumex/tests/core/reflection/field_reflection/CMakeLists.txt"
    "lumex_add_standard_suites(ReflectionFieldReflection")
_require_text("lumex/tests/core/reflection/reflected_enum/CMakeLists.txt"
    "lumex_add_standard_suites(ReflectionReflectedEnum")
_require_text("lumex/tests/core/reflection/var_info/CMakeLists.txt"
    "lumex_add_standard_suites(ReflectionVarInfo")

# The registration of the field names has negative compile checks, run by the
# consumer runner at configure time, and tests of its own.
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_test_name(_ctest_name field_names_compile_checks)")
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "-DCASE=field_names_compile_checks")
foreach(_file CMakeLists.txt check.cpp)
    if(NOT EXISTS
       "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/field_names_compile_checks/${_file}")
        message(FATAL_ERROR "field_names_compile_checks has no ${_file}")
    endif()
endforeach()
foreach(_file LumexFieldNamesRegistration.cxx11.tests.cpp
              LumexFieldNamesRegistration.cxx14.tests.cpp
              LumexFieldNamesGet.cxx11.tests.cpp
              LumexFieldNamesGet.cxx14.tests.cpp
              LumexFieldNamesJson.cxx11.tests.cpp
              LumexFieldNamesStdOptional.cxx17.tests.cpp
              LumexFieldNamesAgreement.cxx20.tests.cpp
              LumexFieldReflectionRegisteredFixtures.hpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/lumex/tests/core/reflection/field_reflection/${_file}")
        message(FATAL_ERROR
            "lumex/tests/core/reflection/field_reflection has no ${_file}")
    endif()
endforeach()
