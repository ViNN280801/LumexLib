# Run by cmake.consumer_third_party_vs_generator (cl.exe trees only).
#
# The release script builds with a Visual Studio generator, whose ClCompile
# items default to /W1: a third-party target silenced with /w alone then
# makes cl.exe print "warning D9025: overriding '/W1' with '/w'", a warning
# line in the release log. Configures the third_party_boost_context fixture
# with VS_GENERATOR, builds the vendored Boost.Context and fails on any
# warning in the build output.
#
# Required: LUMEX_SOURCE_DIR, WORK_DIR, VS_GENERATOR.

foreach(_var LUMEX_SOURCE_DIR WORK_DIR VS_GENERATOR)
    if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
        message(FATAL_ERROR "third_party_vs_generator: ${_var} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/third_party_boost_context"
        -B "${WORK_DIR}" -G "${VS_GENERATOR}" -A x64
        "-DLUMEX_SOURCE_DIR=${LUMEX_SOURCE_DIR}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "third_party_vs_generator: configure failed (${_rc})\n${_out}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${WORK_DIR}" --config Release
            --target lumex_3rdparty_boost_context
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "third_party_vs_generator: build failed (${_rc})\n${_out}")
endif()

string(REGEX MATCHALL "[^\n]*[Ww]arning [A-Z]*[0-9]+[^\n]*" _warnings "${_out}")
if(_warnings)
    string(REPLACE ";" "\n" _warnings "${_warnings}")
    message(FATAL_ERROR
        "third_party_vs_generator: the vendored Boost.Context warns under "
        "${VS_GENERATOR}:\n${_warnings}")
endif()
message(STATUS "third_party_vs_generator: no warning under ${VS_GENERATOR}")
