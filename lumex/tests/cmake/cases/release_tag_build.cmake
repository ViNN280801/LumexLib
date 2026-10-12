# The -Tag / --tag build (release 1.0.3.1, the user's request): both release
# scripts can drive the build of a git tag's sources with the current script -
# a detached worktree of the tag under <output-dir>/.work, its CMakeRoutines
# submodule taken from the sibling checkout (offline: the file transport needs
# the explicit allow since Git 2.38.1), the worktree removed at the end. A
# missing string here is a script that silently builds the working tree while
# the user asked for a tag.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("create_release.ps1" "[string] $Tag       = '',")
_require_text("create_release.ps1" "refs/tags/$Tag^{commit}")
_require_text("create_release.ps1" "'worktree', 'add', '--detach'")
_require_text("create_release.ps1" "protocol.file.allow=always")
_require_text("create_release.ps1" "submodule.CMakeRoutines.url=")
_require_text("create_release.ps1" "'worktree', 'remove', '--force'")
_require_text("create_release.ps1" "# An interrupted earlier run can leave a worktree registration")
_require_text("create_release.ps1" "keepWorkRoot")
_require_text("create_release.sh" "TAG_ARG=\"\"")
_require_text("create_release.sh" "refs/tags/\${TAG_ARG}^{commit}")
_require_text("create_release.sh" "worktree add --detach")
_require_text("create_release.sh" "protocol.file.allow=always")
_require_text("create_release.sh" "submodule.CMakeRoutines.url=")
_require_text("create_release.sh" "worktree remove --force")
_require_text("create_release.sh" "# An interrupted earlier run can leave a worktree registration")
