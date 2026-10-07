# create_release.sh --dry-run shows every command the script would run and
# changes nothing: no output directory, no build tree, no package. The case runs
# the script for real, with the host C++ compiler, and reads its output: the
# plan has one build per --std item, the package names carry the standard, a
# command is shown after a "$", and the colours are on only when asked for
# (--color always) or when a terminal is there (here none: the output is a pipe).

if(NOT CMAKE_HOST_UNIX)
    message(STATUS "Skipping release_script_dry_run: create_release.sh is a Linux script")
    return()
endif()
find_program(_bash bash)
find_program(_cxx NAMES g++ c++ clang++)
foreach(_tool make python3 tar gzip readelf)
    find_program(_${_tool} ${_tool})
    if(NOT _${_tool})
        message(STATUS "Skipping release_script_dry_run: no ${_tool}")
        return()
    endif()
endforeach()
if(NOT _bash OR NOT _cxx)
    message(STATUS "Skipping release_script_dry_run: no bash or no C++ compiler")
    return()
endif()
find_program(_ninja ninja)
# The script takes the generator of the environment when --use-ninja is absent.
unset(ENV{CMAKE_GENERATOR})

set(_script "${LUMEX_SOURCE_DIR}/create_release.sh")
set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/release_dry_run_out")
file(REMOVE_RECURSE "${_out_dir}")
string(ASCII 27 _esc)

# _run(<result prefix> <script arguments>...) sets <prefix>_CODE, <prefix>_OUT, <prefix>_ERR
function(_run prefix)
    execute_process(
        COMMAND ${_bash} ${_script} --compilers ${_cxx} --output-dir ${_out_dir} ${ARGN}
        RESULT_VARIABLE _code
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err)
    set(${prefix}_CODE "${_code}" PARENT_SCOPE)
    set(${prefix}_OUT "${_out}" PARENT_SCOPE)
    set(${prefix}_ERR "${_err}" PARENT_SCOPE)
endfunction()

function(_expect_text text needle what)
    string(FIND "${text}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${what}: no '${needle}' in:\n${text}")
    endif()
endfunction()

function(_expect_no_text text needle what)
    string(FIND "${text}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "${what}: unexpected '${needle}' in:\n${text}")
    endif()
endfunction()

# Two standards, one package format: two builds, a package for each.
_run(plain --dry-run --std 11,17 --formats tar.gz)
if(NOT plain_CODE EQUAL 0)
    message(FATAL_ERROR "dry run exited with ${plain_CODE}:\n${plain_OUT}\n${plain_ERR}")
endif()
_expect_text("${plain_OUT}" "[1/2] LumexLib-" "plain")
_expect_text("${plain_OUT}" "[2/2] LumexLib-" "plain")
_expect_text("${plain_OUT}" "_cxx11 (C++11)" "plain")
_expect_text("${plain_OUT}" "_cxx17 (C++17)" "plain")
_expect_text("${plain_OUT}" "\$ cmake \\\n" "plain")
_expect_text("${plain_OUT}" "-DLUMEX_WERROR=ON" "plain")
_expect_text("${plain_OUT}" "-DCMAKE_CXX_STANDARD=17" "plain")
_expect_text("${plain_OUT}" "--install" "plain")
# Without --use-ninja no generator is named, and make keeps going with -k.
_expect_no_text("${plain_OUT}" "-G Ninja" "plain")
_expect_text("${plain_OUT}" "-Otarget" "plain")
_expect_text("${plain_OUT}" "_cxx11.tar.gz (would be created)" "plain")
_expect_text("${plain_OUT}" "_cxx17.tar.gz (would be created)" "plain")
_expect_text("${plain_OUT}" "Dry run finished: 2 build(s) shown, 2 package(s) would be created" "plain")
_expect_no_text("${plain_OUT}" "${_esc}" "plain: a pipe is not coloured")
if(EXISTS "${_out_dir}")
    message(FATAL_ERROR "a dry run created ${_out_dir}")
endif()

# --color always puts the command in cyan, after a "$".
_run(colour --dry-run --std 11 --formats tar.gz --color always)
_expect_text("${colour_OUT}" "${_esc}[36m\$ cmake" "colour")
_expect_text("${colour_OUT}" "${_esc}[0m" "colour")

# --no-package: the builds are shown, no package is.
_run(nopkg --dry-run --std 11 --no-package --color never)
_expect_text("${nopkg_OUT}" "\$ cmake \\\n" "no-package")
_expect_text("${nopkg_OUT}" "Dry run finished: 1 build(s) shown." "no-package")
_expect_no_text("${nopkg_OUT}" "would be created" "no-package")
_expect_no_text("${nopkg_OUT}" "\$ tar" "no-package")

# --use-ninja adds -G Ninja, and Ninja keeps going with -k 0.
if(_ninja)
    _run(ninja --dry-run --std 11 --no-package --use-ninja --color never)
    if(NOT ninja_CODE EQUAL 0)
        message(FATAL_ERROR "--use-ninja dry run exited with ${ninja_CODE}:\n${ninja_OUT}\n${ninja_ERR}")
    endif()
    _expect_text("${ninja_OUT}" "-G Ninja" "use-ninja")
    _expect_text("${ninja_OUT}" "-k 0" "use-ninja")
    _expect_no_text("${ninja_OUT}" "-Otarget" "use-ninja")
endif()

# The number of jobs: -j N, -jN, --jobs N, --parallel N and the = forms all end
# in cmake --build --parallel N.
foreach(_form "-j;3" "-j4" "--jobs;5" "--jobs=6" "--parallel;7" "--parallel=8")
    string(REGEX REPLACE "^[^0-9]*" "" _count "${_form}")
    _run(jobs --dry-run --std 11 --no-package --color never ${_form})
    if(NOT jobs_CODE EQUAL 0)
        message(FATAL_ERROR "'${_form}' exited with ${jobs_CODE}:\n${jobs_OUT}\n${jobs_ERR}")
    endif()
    _expect_text("${jobs_OUT}" "--parallel ${_count}" "jobs ${_form}")
endforeach()
_run(bad_jobs --dry-run -j0)
if(bad_jobs_CODE EQUAL 0)
    message(FATAL_ERROR "-j0 was accepted")
endif()
_expect_text("${bad_jobs_ERR}" "must be a positive number" "-j0")

# A wrong --color is refused.
_run(bad --dry-run --color purple)
if(bad_CODE EQUAL 0)
    message(FATAL_ERROR "--color purple was accepted")
endif()
_expect_text("${bad_ERR}" "--color must be auto, always or never" "bad colour")
