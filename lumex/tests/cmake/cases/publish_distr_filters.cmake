# PublishDistr copies Lumex shared libs and drops Tests / Example artifacts.

string(RANDOM LENGTH 8 _tok)
# TEMP exists only on Windows; without the fallbacks the tree went to /.
if(NOT "$ENV{TEMP}" STREQUAL "")
    set(_scratch "$ENV{TEMP}")
elseif(NOT "$ENV{TMPDIR}" STREQUAL "")
    set(_scratch "$ENV{TMPDIR}")
else()
    set(_scratch "/tmp")
endif()
set(_root "${_scratch}/lumex_distr_${_tok}")
set(_bin "${_root}/bin")
set(_distr "${_root}/DistrRelease")
file(MAKE_DIRECTORY "${_bin}")

# PublishDistr globs the host's shared-library names: Lumex*.dll on Windows,
# libLumex*.so* elsewhere. Executables have no extension outside Windows.
if(CMAKE_HOST_WIN32)
    set(_kept LumexCore_utility.dll)
    set(_dropped
        LumexFakeTests.dll
        LumexXmlExample1.dll
        LumexUtilityTests.exe
        LumexXmlExample1.exe)
else()
    set(_kept libLumexCore_utility.so)
    set(_dropped
        libLumexFakeTests.so
        libLumexXmlExample1.so
        LumexUtilityTests
        LumexXmlExample1)
endif()

foreach(_name IN LISTS _kept _dropped)
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

if(NOT EXISTS "${_distr}/${_kept}")
    file(REMOVE_RECURSE "${_root}")
    message(FATAL_ERROR
        "PublishDistr did not copy ${_kept}\n${_out}${_err}")
endif()

foreach(_forbidden IN LISTS _dropped)
    if(EXISTS "${_distr}/${_forbidden}")
        file(REMOVE_RECURSE "${_root}")
        message(FATAL_ERROR
            "PublishDistr copied forbidden artifact ${_forbidden}")
    endif()
endforeach()

file(REMOVE_RECURSE "${_root}")
