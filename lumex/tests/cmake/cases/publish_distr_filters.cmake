# PublishDistr copies Lumex shared libs and drops Tests / Example artifacts.

string(RANDOM LENGTH 8 _tok)
set(_root "$ENV{TEMP}/lumex_distr_${_tok}")
set(_bin "${_root}/bin")
set(_distr "${_root}/DistrRelease")
file(MAKE_DIRECTORY "${_bin}")

foreach(_name
        LumexCore_utility.dll
        LumexFakeTests.dll
        LumexXmlExample1.dll
        LumexUtilityTests.exe
        LumexXmlExample1.exe)
    file(WRITE "${_bin}/${_name}" "stub")
endforeach()

execute_process(
    COMMAND ${CMAKE_COMMAND}
        -Dbin_dir=${_bin}
        -Ddistr_dir=${_distr}
        -P "${LUMEX_SOURCE_DIR}/cmake/PublishDistr.cmake"
    RESULT_VARIABLE _rv
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(NOT _rv EQUAL 0)
    file(REMOVE_RECURSE "${_root}")
    message(FATAL_ERROR
        "PublishDistr.cmake failed (${_rv})\n${_out}${_err}")
endif()

if(NOT EXISTS "${_distr}/LumexCore_utility.dll")
    file(REMOVE_RECURSE "${_root}")
    message(FATAL_ERROR
        "PublishDistr did not copy LumexCore_utility.dll\n${_out}${_err}")
endif()

foreach(_forbidden
        LumexFakeTests.dll
        LumexXmlExample1.dll
        LumexUtilityTests.exe
        LumexXmlExample1.exe)
    if(EXISTS "${_distr}/${_forbidden}")
        file(REMOVE_RECURSE "${_root}")
        message(FATAL_ERROR
            "PublishDistr copied forbidden artifact ${_forbidden}")
    endif()
endforeach()

file(REMOVE_RECURSE "${_root}")
