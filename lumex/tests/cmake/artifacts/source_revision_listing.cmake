# Run by cmake.source_revision_listing (a tree with a shared LumexCore_utility
# and Python 3).
#
# Scripts/ReleaseTools/generate_git_source_revision.py, as publish_distr and
# the install hook run it (cmake/LumexSourceRevision.cmake), on a library of
# this very tree: the listing names the LumexLib commit and the CMakeRoutines
# submodule, and for the library its symbol keys with "match" when the
# symbols beside it are its own (the .pdb on Windows, the split .debug on
# ELF). Then the symbols of another library are put in their place under the
# same name: the listing must say "another build", so a stale symbols folder
# is never reported as good.
#
# Required: PYTHON, LUMEX_SOURCE_DIR, WORK_DIR, LIBRARY (the library file),
# SYMBOLS (its .pdb or .debug, may not exist), OTHER_SYMBOLS (the same kind of
# file of another library, may not exist).

foreach(_var PYTHON LUMEX_SOURCE_DIR WORK_DIR LIBRARY)
    if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
        message(FATAL_ERROR "source_revision_listing: ${_var} is required")
    endif()
endforeach()

include("${LUMEX_SOURCE_DIR}/cmake/LumexSourceRevision.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/bin" "${WORK_DIR}/sym")
file(COPY "${LIBRARY}" DESTINATION "${WORK_DIR}/bin")
get_filename_component(_library_name "${LIBRARY}" NAME)
set(_have_symbols FALSE)
if(SYMBOLS AND EXISTS "${SYMBOLS}")
    file(COPY "${SYMBOLS}" DESTINATION "${WORK_DIR}/sym")
    get_filename_component(_symbols_name "${SYMBOLS}" NAME)
    set(_have_symbols TRUE)
endif()

function(_listing out_var)
    set(_rev "${WORK_DIR}/LumexLib-source-revision.rev")
    file(REMOVE "${_rev}")
    lumex_write_source_revision("${PYTHON}" "${LUMEX_SOURCE_DIR}" "${_rev}"
        "${WORK_DIR}/bin" "${WORK_DIR}/sym")
    if(NOT EXISTS "${_rev}")
        message(FATAL_ERROR "source_revision_listing: ${_rev} was not written")
    endif()
    file(READ "${_rev}" _text)
    set(${out_var} "${_text}" PARENT_SCOPE)
endfunction()

_listing(_text)
if(NOT _text MATCHES "\nsuperproject [0-9a-f]+")
    message(FATAL_ERROR "source_revision_listing: no LumexLib commit:\n${_text}")
endif()
if(NOT _text MATCHES "[0-9a-f]+ CMakeRoutines")
    # Seen twice on Windows right after a reconfigure (the test took 15 s),
    # never reproduced by hand: say what git answers now.
    string(TIMESTAMP _git_start "%s")
    execute_process(
        COMMAND git -C "${LUMEX_SOURCE_DIR}" submodule status --recursive
        RESULT_VARIABLE _git_rc
        OUTPUT_VARIABLE _git_out
        ERROR_VARIABLE _git_err)
    string(TIMESTAMP _git_end "%s")
    math(EXPR _git_seconds "${_git_end} - ${_git_start}")
    message(FATAL_ERROR "source_revision_listing: no CMakeRoutines submodule:\n${_text}\n"
        "git submodule status now: exit ${_git_rc} in ${_git_seconds} s\n${_git_out}\n${_git_err}")
endif()
if(NOT _text MATCHES "\n${_library_name}\n  size [0-9]+ bytes, sha256 [0-9a-f]+")
    message(FATAL_ERROR "source_revision_listing: ${_library_name} is not listed:\n${_text}")
endif()

if(NOT _have_symbols)
    message(STATUS "source_revision_listing: ${_library_name} listed; no symbols in this tree to match")
    return()
endif()
if(NOT _text MATCHES "(GUID and age|Build ID) match")
    message(FATAL_ERROR
        "source_revision_listing: the symbols of ${_library_name} do not match:\n${_text}")
endif()

if(NOT OTHER_SYMBOLS OR NOT EXISTS "${OTHER_SYMBOLS}")
    message(STATUS "source_revision_listing: matched; no other library to swap in")
    return()
endif()
file(REMOVE "${WORK_DIR}/sym/${_symbols_name}")
configure_file("${OTHER_SYMBOLS}" "${WORK_DIR}/sym/${_symbols_name}" COPYONLY)
_listing(_text)
if(NOT _text MATCHES "in the symbols folder but it is another build")
    message(FATAL_ERROR
        "source_revision_listing: foreign symbols named ${_symbols_name} were not told apart:\n${_text}")
endif()
message(STATUS "source_revision_listing: matched, and foreign symbols told apart")
