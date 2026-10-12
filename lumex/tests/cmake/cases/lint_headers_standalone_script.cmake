# Scripts/CodeTools/check_headers_standalone.py (the lint.headers_standalone
# case): in fixtures/headers_standalone/bad/lumex it must report Bad.hpp (it
# spells std::size_t without <cstddef>) and only that header, and exit with 1
# under --check; in fixtures/headers_standalone/good/lumex it must accept all
# three headers (a header, an umbrella without an extension, and a copy of the
# logger config format header, which compiles only with its macro defined). A
# checker that accepts the bad file lets a header through that relies on a
# neighbour; one that skips the macro table fails the good fixture.

find_program(_python NAMES python3 python)
if(NOT _python)
    message(STATUS "Skipping lint_headers_standalone_script: no Python 3 interpreter")
    return()
endif()
if(DEFINED ENV{CXX} AND NOT "$ENV{CXX}" STREQUAL "")
    set(_cxx "$ENV{CXX}")
else()
    find_program(_cxx NAMES c++ g++ clang++)
endif()
if(NOT _cxx)
    message(STATUS "Skipping lint_headers_standalone_script: no C++ compiler")
    return()
endif()

set(_script "${LUMEX_SOURCE_DIR}/Scripts/CodeTools/check_headers_standalone.py")
set(_fixtures "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/fixtures/headers_standalone")

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/bad/lumex
        --compiler ${_cxx} --std 11 --jobs 2
    RESULT_VARIABLE _bad_code
    OUTPUT_VARIABLE _bad_out)
if(NOT _bad_code EQUAL 1)
    message(FATAL_ERROR "the checker returned ${_bad_code} for the bad fixture, expected 1:\n${_bad_out}")
endif()
if(NOT _bad_out MATCHES "lumex/Bad\\.hpp: does not compile alone")
    message(FATAL_ERROR "the checker did not report Bad.hpp:\n${_bad_out}")
endif()
if(_bad_out MATCHES "Fine\\.hpp: does not compile alone")
    message(FATAL_ERROR "the checker reported the header that compiles alone:\n${_bad_out}")
endif()
if(NOT _bad_out MATCHES "1 of 2 public header")
    message(FATAL_ERROR "the checker did not count 1 of 2 headers in the bad fixture:\n${_bad_out}")
endif()

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/good/lumex
        --compiler ${_cxx} --std 11 --jobs 2
    RESULT_VARIABLE _good_code
    OUTPUT_VARIABLE _good_out)
if(NOT _good_code EQUAL 0)
    message(FATAL_ERROR "the checker rejected the good fixture:\n${_good_out}")
endif()
if(NOT _good_out MATCHES "0 of 3 public header")
    message(FATAL_ERROR "the checker did not count the 3 headers of the good fixture:\n${_good_out}")
endif()

# Without --check a failing header is listed and the exit code stays 0.
execute_process(
    COMMAND ${_python} ${_script} --dir ${_fixtures}/bad/lumex
        --compiler ${_cxx} --std 11 --jobs 2
    RESULT_VARIABLE _list_code
    OUTPUT_VARIABLE _list_out)
if(NOT _list_code EQUAL 0 OR NOT _list_out MATCHES "Bad\\.hpp: does not compile alone")
    message(FATAL_ERROR "listing mode must report Bad.hpp and exit 0 (got ${_list_code}):\n${_list_out}")
endif()
