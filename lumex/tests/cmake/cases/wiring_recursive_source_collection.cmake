# RecursiveSourceCollection: included from LumexBuild; smoke-collect once.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("cmake/LumexBuild.cmake"
    "include(utils/RecursiveSourceCollection)")

file(READ
    "${LUMEX_SOURCE_DIR}/CMakeRoutines/utils/RecursiveSourceCollection.cmake"
    _mod)
string(FIND "${_mod}" "function(collect_sources_recursive" _fn)
if(_fn EQUAL -1)
    message(FATAL_ERROR
        "RecursiveSourceCollection.cmake missing collect_sources_recursive")
endif()

# Functional smoke: include the module and collect from a tiny temp tree.
# Lumex production modules keep explicit source lists; this only proves the
# util is loadable and filters as documented.
include("${LUMEX_SOURCE_DIR}/CMakeRoutines/utils/RecursiveSourceCollection.cmake")

if(DEFINED ENV{TEMP} AND NOT "$ENV{TEMP}" STREQUAL "")
    set(_tmp "$ENV{TEMP}/lumex_rsc_smoke")
elseif(DEFINED ENV{TMPDIR} AND NOT "$ENV{TMPDIR}" STREQUAL "")
    set(_tmp "$ENV{TMPDIR}/lumex_rsc_smoke")
else()
    set(_tmp "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/.rsc_smoke_tmp")
endif()
file(TO_CMAKE_PATH "${_tmp}" _tmp)
file(REMOVE_RECURSE "${_tmp}")
file(MAKE_DIRECTORY "${_tmp}/src")
file(MAKE_DIRECTORY "${_tmp}/src/tests")
file(MAKE_DIRECTORY "${_tmp}/src/3rdparty")
file(WRITE "${_tmp}/src/a.cpp" "// a\n")
file(WRITE "${_tmp}/src/b.hpp" "// b\n")
file(WRITE "${_tmp}/src/tests/t.cpp" "// t\n")
file(WRITE "${_tmp}/src/3rdparty/x.cpp" "// x\n")

collect_sources_recursive("${_tmp}/src" _rsc_out
    INCLUDE_TESTS OFF
    INCLUDE_EXAMPLES OFF)

list(LENGTH _rsc_out _rsc_n)
if(NOT _rsc_n EQUAL 2)
    message(FATAL_ERROR
        "collect_sources_recursive expected 2 files (a.cpp, b.hpp), got ${_rsc_n}: ${_rsc_out}")
endif()

file(REMOVE_RECURSE "${_tmp}")
