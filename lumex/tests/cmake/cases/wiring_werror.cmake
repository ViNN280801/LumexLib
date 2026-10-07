# LUMEX_WERROR: default OFF; when ON the library targets get -Werror (/WX on
# MSVC and clang-cl) from lumex_configure_target, after the warnings are
# switched on; targets that suppress all warnings (tests, examples) are left
# out. Do not flip the default to ON to silence the case - it is a product
# decision (the release script passes ON itself).

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _opts)
string(REGEX MATCH "option\\(LUMEX_WERROR[^\n]*\\)" _line "${_opts}")
if(NOT _line)
    message(FATAL_ERROR "cmake/LumexOptions.cmake has no option(LUMEX_WERROR ...)")
endif()
if(NOT _line MATCHES " OFF\\)$")
    message(FATAL_ERROR "LUMEX_WERROR default is not OFF: ${_line}")
endif()

# The switch sits in the branch that is not WARNINGS OFF, after
# configure_compiler_flags(... WARNINGS HIGH).
file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexBuild.cmake" _build)
string(FIND "${_build}" "configure_compiler_flags(\"\${target_name}\" WARNINGS HIGH)" _high)
string(FIND "${_build}" "if(LUMEX_WERROR)" _werror)
string(FIND "${_build}" "configure_compiler_flags(\"\${target_name}\" WARNINGS OFF)" _off)
if(_high EQUAL -1 OR _werror EQUAL -1 OR _off EQUAL -1)
    message(FATAL_ERROR "cmake/LumexBuild.cmake lost the WARNINGS HIGH / LUMEX_WERROR wiring")
endif()
if(_werror LESS _high)
    message(FATAL_ERROR "if(LUMEX_WERROR) must come after WARNINGS HIGH")
endif()
_require_text("cmake/LumexBuild.cmake" "/WX")
_require_text("cmake/LumexBuild.cmake" "-Werror")
_require_text("CMakeLists.txt" "LUMEX_WERROR")
