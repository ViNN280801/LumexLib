# WarningSuppression: included from LumexBuild; gtest uses suppress_warnings.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("cmake/LumexBuild.cmake" "include(utils/WarningSuppression)")
_require_text("cmake/LumexGoogleTest.cmake" "suppress_warnings")
_require_text("cmake/LumexGoogleTest.cmake" "COMMAND suppress_warnings")

# Module must expose the public API used by LumexGoogleTest.
file(READ
    "${LUMEX_SOURCE_DIR}/CMakeRoutines/utils/WarningSuppression.cmake" _mod)
string(FIND "${_mod}" "function(suppress_warnings" _fn)
if(_fn EQUAL -1)
    message(FATAL_ERROR
        "CMakeRoutines/utils/WarningSuppression.cmake missing suppress_warnings")
endif()
