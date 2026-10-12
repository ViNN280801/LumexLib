# MsvcVcvarsConfig must run in the root CMakeLists before project().

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("CMakeLists.txt" "include(core/MsvcVcvarsConfig)")
_require_text("CMakeLists.txt" "configure_msvc_vcvars()")
_require_text("CMakeLists.txt" "project(LumexLib")
_require_text("CMakeLists.txt" "MsvcVcvarsConfig: skipped")

file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)
string(FIND "${_root}" "include(core/MsvcVcvarsConfig)" _inc)
string(FIND "${_root}" "configure_msvc_vcvars()" _call)
string(FIND "${_root}" "project(LumexLib" _proj)
if(_inc EQUAL -1 OR _call EQUAL -1 OR _proj EQUAL -1)
    message(FATAL_ERROR "MsvcVcvarsConfig wiring is incomplete")
endif()
if(_inc GREATER _call OR _call GREATER _proj)
    message(FATAL_ERROR
        "configure_msvc_vcvars() must sit after include(core/MsvcVcvarsConfig) "
        "and before project(LumexLib)")
endif()
