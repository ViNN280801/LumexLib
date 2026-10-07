# One test suite per C++ standard (lumex/tests/LumexTestStandards.cmake) and
# one test directory per source directory: every test directory against the
# table of standards and against the source tree.
#
# - Every directory under lumex/tests with *.tests.cpp files (other than
#   cmake/ and support/) belongs to a table entry, the longest module key its
#   CTest prefix begins with (core/math/ops -> math.ops. -> math), and every
#   entry has a test directory of its own or below it.
# - The test tree follows the source tree: lumex/tests/<path> with tests has a
#   lumex/<path> directory, so the CTest prefix of a test names the source
#   directory it tests (math.ops., utility.traits., json.schema.).
# - A test directory calls lumex_add_standard_suites exactly once, with
#   MODULE <the key of its entry>, calls none of add_executable, add_test,
#   lumex_test_use_gtest and lumex_gtest_discover_tests itself, names every
#   test source <Stem>.cxx<std>.tests.cpp with <std> a standard of its entry,
#   and passes VARIANT <name> for exactly the variants of its entry. Some
#   directory of the module has a file of the module's lowest standard; a
#   directory itself has suites from its lowest file on only.
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
# A directory has no suite below its lowest file: the selection is empty, the
# helper builds nothing for that standard.
lumex_test_standards_select(_selected base64 11 A.cxx17.tests.cpp)
_expect_equal("C++11 selection of a C++17 file" "${_selected}" "")
lumex_test_standards_select(_selected base64 17 A.cxx17.tests.cpp)
_expect_equal("C++17 selection of a C++17 file" "${_selected}" "A.cxx17.tests.cpp")
lumex_test_standards_select(_selected base64 20 A.cxx17.tests.cpp)
_expect_equal("C++20 selection of a C++17 file" "${_selected}" "A.cxx17.tests.cpp")

foreach(_case
        "outside_scheme|outside the scheme"
        "standard_of_other_module|outside the scheme"
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
# The module of a directory begins the directory's own CTest prefix, a suite
# without a test source is skipped, and a directory that builds none fails.
_require_text("${_gtest}" "lumex_test_name(_directory_prefix \"\")")
_require_text("${_gtest}" "if(NOT _directory_head STREQUAL \"\${ARG_MODULE}.\")")
_require_text("${_gtest}" "Skipping \${_target}: no test source of C++\${_std} or lower in")
_require_text("${_gtest}" "if(NOT _targets AND _unsupported EQUAL 0)")
_require_text("${_gtest}" "no *.tests.cpp in")

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

# The module of a directory: the longest key of the table that its CTest
# prefix begins with (math.ops. -> math), or empty.
function(_module_of_prefix out_var prefix)
    set(_best "")
    foreach(_candidate IN LISTS LUMEX_TEST_STANDARD_MODULES)
        string(LENGTH "${_candidate}." _length)
        string(SUBSTRING "${prefix}" 0 ${_length} _head)
        string(LENGTH "${_best}" _best_length)
        if(_head STREQUAL "${_candidate}." AND _length GREATER _best_length)
            set(_best "${_candidate}")
        endif()
    endforeach()
    set(${out_var} "${_best}" PARENT_SCOPE)
endfunction()

# The module of every test directory, for the lowest-standard check.
set(_module_lowest_files "")
set(_seen_keys "")
foreach(_dir IN LISTS _dirs)
    file(RELATIVE_PATH _rel "${_tests}" "${_dir}")
    lumex_test_name_prefix(_prefix "${_dir}" "${_tests}")
    _module_of_prefix(_key "${_prefix}")
    if(_key STREQUAL "")
        string(REGEX REPLACE "\\.$" "" _own "${_prefix}")
        string(APPEND _errors
            "  lumex/tests/${_rel}: no module of '${_own}' in "
            "LumexTestStandards.cmake\n")
        continue()
    endif()
    list(APPEND _seen_keys "${_key}")

    # The test tree follows the source tree.
    if(NOT IS_DIRECTORY "${LUMEX_SOURCE_DIR}/lumex/${_rel}")
        string(APPEND _errors
            "  lumex/tests/${_rel}: there is no lumex/${_rel}; a test directory "
            "has the path of the source directory it tests\n")
    endif()

    # The parent directory adds it (a directory nobody adds registers no test).
    get_filename_component(_name "${_dir}" NAME)
    get_filename_component(_parent "${_dir}" DIRECTORY)
    if(NOT EXISTS "${_parent}/CMakeLists.txt")
        string(APPEND _errors "  lumex/tests/${_rel}: its parent has no CMakeLists.txt\n")
    else()
        _read_code(_parent_code "${_parent}/CMakeLists.txt")
        if(NOT _parent_code MATCHES
           "(add_subdirectory[ \t]*\\(|lumex_add_subdirectory_if[ \t]*\\([A-Za-z0-9_]+[ \t]+)${_name}[ \t]*\\)")
            string(APPEND _errors
                "  lumex/tests/${_rel}: the CMakeLists.txt of its parent does "
                "not add_subdirectory (${_name})\n")
        endif()
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
    foreach(_file IN LISTS _files)
        lumex_test_standards_of_file(_std "${_file}")
        if(_std STREQUAL "" OR NOT _std IN_LIST _standards)
            string(APPEND _errors
                "  lumex/tests/${_rel}/${_file}: not <Stem>.cxx<std>.tests.cpp "
                "with <std> one of ${_standards}\n")
        elseif(_std STREQUAL _lowest)
            list(APPEND _module_lowest_files "${_key}")
        endif()
    endforeach()
endforeach()

foreach(_key IN LISTS LUMEX_TEST_STANDARD_MODULES)
    if(NOT _key IN_LIST _seen_keys)
        string(APPEND _errors
            "  LumexTestStandards.cmake: module '${_key}' has no test directory\n")
    elseif(NOT _key IN_LIST _module_lowest_files)
        lumex_test_standards_get(_standards "${_key}")
        list(GET _standards 0 _lowest)
        string(APPEND _errors
            "  module '${_key}': no test directory has a test source of its "
            "lowest standard (<Stem>.cxx${_lowest}.tests.cpp)\n")
    endif()
endforeach()

if(_errors)
    message(FATAL_ERROR "One test suite per standard:\n${_errors}")
endif()
