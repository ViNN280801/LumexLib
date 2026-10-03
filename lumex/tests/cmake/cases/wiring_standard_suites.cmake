# One test suite per C++ standard (lumex/tests/LumexTestStandards.cmake):
# every test directory against the table of standards.
#
# - Every directory under lumex/tests with *.tests.cpp files (other than
#   cmake/ and support/) has a table entry under its module key (its CTest
#   prefix without the trailing dot), and every entry has such a directory.
# - A converted directory calls lumex_add_standard_suites exactly once, with
#   MODULE <its key>, calls none of add_executable, add_test,
#   lumex_test_use_gtest and lumex_gtest_discover_tests itself, names every
#   test source <Stem>.cxx<std>.tests.cpp with <std> a standard of its entry,
#   has a file of its lowest standard, and passes VARIANT <name> for exactly
#   the variants of its entry.
# - A directory on the transition list is not converted yet; it must not
#   call the helper (a converted directory leaves the list).
# - The helper and the table functions behave as documented.

include("${LUMEX_SOURCE_DIR}/cmake/LumexTestNames.cmake")
include("${LUMEX_SOURCE_DIR}/lumex/tests/LumexTestStandards.cmake")

# Re-entered by the checks of failing calls below: one call, which must stop
# with an error.
if(DEFINED LUMEX_STANDARD_SUITES_EXPECT_FAIL)
    if(LUMEX_STANDARD_SUITES_EXPECT_FAIL STREQUAL "outside_scheme")
        lumex_test_standards_select(_out base64 20
            LumexBase64.cxx11.tests.cpp LumexBase64.tests.cpp)
    elseif(LUMEX_STANDARD_SUITES_EXPECT_FAIL STREQUAL "standard_of_other_module")
        lumex_test_standards_select(_out base64 20
            LumexBase64.cxx11.tests.cpp LumexBase64.cxx14.tests.cpp)
    elseif(LUMEX_STANDARD_SUITES_EXPECT_FAIL STREQUAL "no_lowest_file")
        lumex_test_standards_select(_out base64 11 LumexBase64.cxx17.tests.cpp)
    elseif(LUMEX_STANDARD_SUITES_EXPECT_FAIL STREQUAL "unknown_module")
        lumex_test_standards_get(_out no_such_module)
    elseif(LUMEX_STANDARD_SUITES_EXPECT_FAIL STREQUAL "unknown_variant")
        lumex_test_standards_get(_out base64 VARIANT lock_based)
    elseif(LUMEX_STANDARD_SUITES_EXPECT_FAIL STREQUAL "descending")
        lumex_test_standards_declare(no_such_module 20 17)
    elseif(LUMEX_STANDARD_SUITES_EXPECT_FAIL STREQUAL "unknown_standard")
        lumex_test_standards_declare(no_such_module 11 15)
    elseif(LUMEX_STANDARD_SUITES_EXPECT_FAIL STREQUAL "duplicate_module")
        lumex_test_standards_declare(base64 11)
    elseif(LUMEX_STANDARD_SUITES_EXPECT_FAIL STREQUAL "variant_outside_module")
        lumex_test_standards_declare_variant(base64 forced 14)
    endif()
    return()
endif()

# Test directories not converted yet, by module key. Remove a module when its
# directory calls lumex_add_standard_suites; the list goes away when it is
# empty.
set(_transition
    atomic
    crc
    environment
    expected
    fmt
    generators.number_generator
    hardware
    json
    logger
    logging
    optional
    reflection
    resource_monitor
    serial
    settings
    string
    string_view
    temporary
    time
    utility
)

set(_errors "")

# --- The table functions --------------------------------------------------

function(_expect_equal what got expected)
    if(NOT "${got}" STREQUAL "${expected}")
        message(FATAL_ERROR "${what}: got '${got}', expected '${expected}'")
    endif()
endfunction()

lumex_test_standards_of_file(_std "LumexBase64.cxx17.tests.cpp")
_expect_equal("standard of LumexBase64.cxx17.tests.cpp" "${_std}" "17")
lumex_test_standards_of_file(_std "/any/dir/Expected_Void2.cxx26.tests.cpp")
_expect_equal("standard of Expected_Void2.cxx26.tests.cpp" "${_std}" "26")
foreach(_name
        LumexBase64.tests.cpp LumexBase64.cxx15.tests.cpp
        LumexBase64.cxx98.tests.cpp CircularBuffer.header.cxx11.tests.cpp
        LumexBase64.cxx17.tests.hpp LumexBase64.cxx17.test.cpp
        .cxx11.tests.cpp 1Base64.cxx11.tests.cpp)
    lumex_test_standards_of_file(_std "${_name}")
    _expect_equal("standard of ${_name}" "${_std}" "")
endforeach()

lumex_test_standards_get(_standards base64)
_expect_equal("standards of base64" "${_standards}" "11;17;20")
lumex_test_standards_get(_standards atomic VARIANT lock_based)
_expect_equal("standards of atomic/lock_based" "${_standards}" "20")

# A suite takes the files of its standard and of every lower one, lower
# standards first, by name within a standard.
set(_files B.cxx20.tests.cpp A.cxx17.tests.cpp Z.cxx11.tests.cpp A.cxx11.tests.cpp)
lumex_test_standards_select(_selected base64 11 ${_files})
_expect_equal("C++11 selection" "${_selected}" "A.cxx11.tests.cpp;Z.cxx11.tests.cpp")
lumex_test_standards_select(_selected base64 17 ${_files})
_expect_equal("C++17 selection" "${_selected}"
    "A.cxx11.tests.cpp;Z.cxx11.tests.cpp;A.cxx17.tests.cpp")
lumex_test_standards_select(_selected base64 20 ${_files})
_expect_equal("C++20 selection" "${_selected}"
    "A.cxx11.tests.cpp;Z.cxx11.tests.cpp;A.cxx17.tests.cpp;B.cxx20.tests.cpp")
# A standard without files of its own inherits the lower ones.
lumex_test_standards_select(_selected base64 20 A.cxx11.tests.cpp)
_expect_equal("C++20 selection without own files" "${_selected}" "A.cxx11.tests.cpp")

foreach(_case
        "outside_scheme|outside the scheme"
        "standard_of_other_module|outside the scheme"
        "no_lowest_file|has no test source"
        "unknown_module|has no module 'no_such_module'"
        "unknown_variant|has no variant 'lock_based' of 'base64'"
        "descending|not strictly ascending"
        "unknown_standard|names C\\+\\+15"
        "duplicate_module|is declared twice"
        "variant_outside_module|is not a standard of 'base64'")
    string(REPLACE "|" ";" _case "${_case}")
    list(GET _case 0 _mode)
    list(GET _case 1 _pattern)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DLUMEX_SOURCE_DIR=${LUMEX_SOURCE_DIR}
            -DLUMEX_STANDARD_SUITES_EXPECT_FAIL=${_mode}
            -P ${CMAKE_CURRENT_LIST_FILE}
        RESULT_VARIABLE _rv
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err)
    # CMake wraps long messages: compare with single spaces.
    string(REGEX REPLACE "[ \t\r\n]+" " " _text "${_out}${_err}")
    if(_rv EQUAL 0 OR NOT _text MATCHES "${_pattern}")
        string(APPEND _errors
            "  the table check '${_mode}' did not fail with '${_pattern}' "
            "(exit ${_rv}):\n${_out}${_err}\n")
    endif()
endforeach()

# --- The wiring of the helper -----------------------------------------------

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("lumex/tests/CMakeLists.txt"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/LumexTestStandards.cmake\")")
set(_gtest "cmake/LumexGoogleTest.cmake")
_require_text("${_gtest}" "function(lumex_add_standard_suites component)")
_require_text("${_gtest}" "set(_target \"Lumex\${component}\${_name_part}Cxx\${_std}Tests\")")
_require_text("${_gtest}" "set(_suffix \"\${_suffix_part}.cxx\${_std}\")")
_require_text("${_gtest}" "TEST_SUFFIX \"\${_suffix}\"")
_require_text("${_gtest}" "_std GREATER LUMEX_TEST_STANDARDS_OPTIONAL_ABOVE")
_require_text("${_gtest}" "NOT \"cxx_std_\${_std}\" IN_LIST CMAKE_CXX_COMPILE_FEATURES")
_require_text("${_gtest}" "lumex_test_standards_select(_sources \"\${ARG_MODULE}\" \${_std} \${_found})")
_require_text("${_gtest}" "lumex_test_use_gtest(\${_target} CXX_STANDARD \${_std})")
_require_text("${_gtest}" "CONFIGURE_DEPENDS")

# --- Every test directory against the table ---------------------------------

get_filename_component(_tests "${LUMEX_SOURCE_DIR}/lumex/tests" ABSOLUTE)
file(GLOB_RECURSE _all_sources "${_tests}/*.tests.cpp")
set(_dirs "")
foreach(_source IN LISTS _all_sources)
    get_filename_component(_dir "${_source}" DIRECTORY)
    file(RELATIVE_PATH _rel "${_tests}" "${_dir}")
    if(_rel MATCHES "^(cmake|support)(/|$)")
        continue()
    endif()
    list(APPEND _dirs "${_dir}")
endforeach()
list(REMOVE_DUPLICATES _dirs)
list(SORT _dirs)

# Strips # comments (outside of quoted text is assumed: test CMakeLists do
# not put # in strings) and returns the text of <path>.
function(_read_code out_var path)
    file(STRINGS "${path}" _lines)
    set(_code "")
    foreach(_line IN LISTS _lines)
        string(REGEX REPLACE "#.*$" "" _line "${_line}")
        string(APPEND _code "${_line}\n")
    endforeach()
    set(${out_var} "${_code}" PARENT_SCOPE)
endfunction()

set(_seen_keys "")
foreach(_dir IN LISTS _dirs)
    file(RELATIVE_PATH _rel "${_tests}" "${_dir}")
    lumex_test_name_prefix(_prefix "${_dir}" "${_tests}")
    string(REGEX REPLACE "\\.$" "" _key "${_prefix}")
    list(APPEND _seen_keys "${_key}")
    if(NOT _key IN_LIST LUMEX_TEST_STANDARD_MODULES)
        string(APPEND _errors
            "  lumex/tests/${_rel}: no entry '${_key}' in LumexTestStandards.cmake\n")
        continue()
    endif()

    set(_list "${_dir}/CMakeLists.txt")
    if(NOT EXISTS "${_list}")
        string(APPEND _errors "  lumex/tests/${_rel}: no CMakeLists.txt\n")
        continue()
    endif()
    _read_code(_code "${_list}")
    string(REGEX MATCHALL "(^|[^A-Za-z0-9_])lumex_add_standard_suites[ \t]*\\("
        _calls "${_code}")
    list(LENGTH _calls _call_count)

    if(_key IN_LIST _transition)
        if(_call_count GREATER 0)
            string(APPEND _errors
                "  lumex/tests/${_rel}: converted, remove '${_key}' from the "
                "transition list of this case\n")
        endif()
        continue()
    endif()

    if(NOT _call_count EQUAL 1)
        string(APPEND _errors
            "  lumex/tests/${_rel}/CMakeLists.txt: ${_call_count} calls of "
            "lumex_add_standard_suites, expected one\n")
        continue()
    endif()
    foreach(_command add_executable add_test lumex_test_use_gtest
            lumex_gtest_discover_tests)
        if(_code MATCHES "(^|[^A-Za-z0-9_])${_command}[ \t]*\\(")
            string(APPEND _errors
                "  lumex/tests/${_rel}/CMakeLists.txt: calls ${_command}; "
                "lumex_add_standard_suites builds every suite\n")
        endif()
    endforeach()

    # The arguments of the call, up to its closing parenthesis.
    string(REGEX MATCH "lumex_add_standard_suites[ \t]*\\(.*$" _call "${_code}")
    string(FIND "${_call}" "(" _open)
    math(EXPR _pos "${_open} + 1")
    string(LENGTH "${_call}" _length)
    set(_depth 1)
    while(_depth GREATER 0 AND _pos LESS _length)
        string(SUBSTRING "${_call}" ${_pos} 1 _char)
        if(_char STREQUAL "(")
            math(EXPR _depth "${_depth} + 1")
        elseif(_char STREQUAL ")")
            math(EXPR _depth "${_depth} - 1")
        endif()
        math(EXPR _pos "${_pos} + 1")
    endwhile()
    math(EXPR _args_length "${_pos} - ${_open} - 2")
    math(EXPR _args_begin "${_open} + 1")
    string(SUBSTRING "${_call}" ${_args_begin} ${_args_length} _args)
    string(STRIP "${_args}" _args)
    string(REGEX REPLACE "[ \t\r\n]+" ";" _args "${_args}")

    list(FIND _args MODULE _module_at)
    set(_module "")
    if(_module_at GREATER -1)
        math(EXPR _module_at "${_module_at} + 1")
        list(LENGTH _args _arg_count)
        if(_module_at LESS _arg_count)
            list(GET _args ${_module_at} _module)
        endif()
    endif()
    if(NOT _module STREQUAL _key)
        string(APPEND _errors
            "  lumex/tests/${_rel}/CMakeLists.txt: MODULE '${_module}', "
            "expected '${_key}'\n")
    endif()

    set(_variants "")
    set(_next_is_variant FALSE)
    foreach(_arg IN LISTS _args)
        if(_next_is_variant)
            list(APPEND _variants "${_arg}")
            set(_next_is_variant FALSE)
        elseif(_arg STREQUAL "VARIANT")
            set(_next_is_variant TRUE)
        endif()
    endforeach()
    set(_declared ${LUMEX_TEST_STANDARD_VARIANTS_${_key}})
    list(SORT _variants)
    list(SORT _declared)
    if(NOT "${_variants}" STREQUAL "${_declared}")
        string(APPEND _errors
            "  lumex/tests/${_rel}/CMakeLists.txt: variants '${_variants}', "
            "the table declares '${_declared}'\n")
    endif()

    lumex_test_standards_get(_standards "${_key}")
    list(GET _standards 0 _lowest)
    file(GLOB _files RELATIVE "${_dir}" "${_dir}/*.tests.cpp")
    set(_has_lowest FALSE)
    foreach(_file IN LISTS _files)
        lumex_test_standards_of_file(_std "${_file}")
        if(_std STREQUAL "" OR NOT _std IN_LIST _standards)
            string(APPEND _errors
                "  lumex/tests/${_rel}/${_file}: not <Stem>.cxx<std>.tests.cpp "
                "with <std> one of ${_standards}\n")
        elseif(_std STREQUAL _lowest)
            set(_has_lowest TRUE)
        endif()
    endforeach()
    if(NOT _has_lowest)
        string(APPEND _errors
            "  lumex/tests/${_rel}: no test source of its lowest standard "
            "(<Stem>.cxx${_lowest}.tests.cpp)\n")
    endif()
endforeach()

foreach(_key IN LISTS LUMEX_TEST_STANDARD_MODULES)
    if(NOT _key IN_LIST _seen_keys)
        string(APPEND _errors
            "  LumexTestStandards.cmake: module '${_key}' has no test directory\n")
    endif()
endforeach()
foreach(_key IN LISTS _transition)
    if(NOT _key IN_LIST _seen_keys)
        string(APPEND _errors
            "  transition list: '${_key}' is not a test directory\n")
    endif()
endforeach()

if(_errors)
    message(FATAL_ERROR "One test suite per standard:\n${_errors}")
endif()
