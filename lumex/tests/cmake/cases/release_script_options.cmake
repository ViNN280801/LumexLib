# create_release.sh builds the four compilers of the release machine without a
# warning: -Werror through LUMEX_WERROR (on by default, --no-werror turns it
# off), --no-package for a build that only checks the warnings, the logs of
# <output-dir>/warns/, and the MinGW cross build with its zip. A missing string
# here is a script that silently drops one of them.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("create_release.sh" "--no-werror) WERROR=0")
_require_text("create_release.sh" "--no-package) NO_PACKAGE=1")
_require_text("create_release.sh" "-DLUMEX_WERROR=ON")
_require_text("create_release.sh" "WARN_DIR=\"\${OUTPUT_DIR}/warns\"")
_require_text("create_release.sh" "_warn.log")
_require_text("create_release.sh" "_err.log")
_require_text("create_release.sh" "extract_diagnostics.py")
_require_text("create_release.sh" "-- -k 0")
_require_text("create_release.sh" "export LC_ALL=C")
_require_text("create_release.sh" "Thread model:")
_require_text("create_release.sh" "write_mingw_toolchain")
_require_text("create_release.sh" "stage_mingw_runtime")
_require_text("create_release.sh" "_win_x64_")
_require_text("create_release.sh" "zip -qrX")
_require_text("create_release.sh" "x86_64-w64-mingw32-g++-posix")
# package and package_source run CPack: not a library build.
_require_text("create_release.sh" "|package|package_source|")
_require_text("Scripts/ReleaseTools/extract_diagnostics.py" "-Werror")
# The C++ standard is in the package name and --std / -Std take a list.
_require_text("create_release.sh" "_cxx\${stdtag}")
_require_text("create_release.sh" "STD_ITEMS")
_require_text("create_release.sh" "std_tag_for")
_require_text("create_release.ps1" "_cxx${stdTag}")
_require_text("create_release.ps1" "$StdList")
_require_text("create_release.ps1" "[string[]] $Std")
