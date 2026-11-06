# create_release.sh --dry-run shows every command the script would run and
# changes nothing: no output directory, no build tree, no package. The case runs
# the script for real, with the host C++ compiler, and reads its output: the
# plan has one build per --std item, the package names carry the standard, a
# command is shown after a "$", and the colours are on only when asked for
# (--color always) or when a terminal is there (here none: the output is a pipe).

# The script reads the glibc version (getconf GNU_LIBC_VERSION) and builds ELF
# packages: Linux only, not Windows, macOS, MSYS2 or Cygwin (CMAKE_HOST_UNIX is
# true on the last two).
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
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
        COMMAND ${_bash} ${_script} --compilers ${_cxx} --output-dir ${_out_dir}
                --doxygen ${_bash} ${ARGN}
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

# The documentation is part of the package unless --no-docs says otherwise
# (user, 2026-11-05). A dry run only checks that the Doxygen executable is
# there (the case passes the shell as a stand-in): the documentation is built
# once, before the builds, and every build installs that tree.
_run(docs --dry-run --std 11,17 --formats tar.gz --color never)
if(NOT docs_CODE EQUAL 0)
    message(FATAL_ERROR "dry run with the documentation exited with ${docs_CODE}:\n${docs_OUT}\n${docs_ERR}")
endif()
_expect_text("${docs_OUT}" "--target generate_documentation" "docs")
_expect_text("${docs_OUT}" "-DLUMEX_BUILD_DOCUMENTATION=ON" "docs")
_expect_text("${docs_OUT}" "-DLUMEX_DOCUMENTATION_PREBUILT_DIR=" "docs")
_expect_no_text("${docs_OUT}" "-DLUMEX_BUILD_DOCUMENTATION=OFF" "docs")
# Once, not once per build.
string(REGEX MATCHALL "--target generate_documentation" _doc_builds "${docs_OUT}")
list(LENGTH _doc_builds _doc_build_count)
if(NOT _doc_build_count EQUAL 1)
    message(FATAL_ERROR "the documentation is built ${_doc_build_count} times, not once:\n${docs_OUT}")
endif()

_run(no_docs --dry-run --std 11,17 --formats tar.gz --color never --no-docs)
if(NOT no_docs_CODE EQUAL 0)
    message(FATAL_ERROR "--no-docs dry run exited with ${no_docs_CODE}:\n${no_docs_OUT}\n${no_docs_ERR}")
endif()
_expect_text("${no_docs_OUT}" "-DLUMEX_BUILD_DOCUMENTATION=OFF" "no-docs")
_expect_no_text("${no_docs_OUT}" "generate_documentation" "no-docs")
_expect_no_text("${no_docs_OUT}" "-DLUMEX_DOCUMENTATION_PREBUILT_DIR=" "no-docs")

# --no-package builds nothing that is packaged: no documentation either.
_run(docs_no_package --dry-run --std 11 --no-package --color never)
_expect_no_text("${docs_no_package_OUT}" "generate_documentation" "docs --no-package")

# A Doxygen that is not there stops the run before the first build, with the
# way out in the message; --no-docs and --no-package do not need it.
execute_process(
    COMMAND ${_bash} ${_script} --compilers ${_cxx} --output-dir ${_out_dir}
            --dry-run --std 11 --formats tar.gz --doxygen ${_out_dir}/no/such/doxygen
    RESULT_VARIABLE missing_CODE OUTPUT_VARIABLE missing_OUT ERROR_VARIABLE missing_ERR)
if(missing_CODE EQUAL 0)
    message(FATAL_ERROR "a missing Doxygen was accepted:\n${missing_OUT}")
endif()
_expect_text("${missing_ERR}" "doxygen" "missing Doxygen")
_expect_text("${missing_ERR}" "pass --no-docs to build the packages without the documentation" "missing Doxygen")
_expect_no_text("${missing_OUT}" "\$ cmake" "missing Doxygen: a build was started")
execute_process(
    COMMAND ${_bash} ${_script} --compilers ${_cxx} --output-dir ${_out_dir}
            --dry-run --std 11 --formats tar.gz --no-docs --doxygen ${_out_dir}/no/such/doxygen
    RESULT_VARIABLE skipped_CODE OUTPUT_VARIABLE skipped_OUT ERROR_VARIABLE skipped_ERR)
if(NOT skipped_CODE EQUAL 0)
    message(FATAL_ERROR "--no-docs still asked for Doxygen:\n${skipped_OUT}\n${skipped_ERR}")
endif()

# A compiler without C++23 (GCC 8, MinGW 8.3: neither -std=c++23 nor
# -std=c++2b) has its C++23 build skipped and listed, not the run failed. The
# compiler is a wrapper of the host one that refuses both flags.
find_program(_cc NAMES gcc cc clang)
if(_cc)
    set(_fake_src "${CMAKE_CURRENT_BINARY_DIR}/release_dry_run_fake_src")
    set(_fake "${CMAKE_CURRENT_BINARY_DIR}/release_dry_run_fake")
    file(REMOVE_RECURSE "${_fake_src}" "${_fake}")
    foreach(_pair "g++|${_cxx}" "gcc|${_cc}")
        string(REPLACE "|" ";" _pair "${_pair}")
        list(GET _pair 0 _name)
        list(GET _pair 1 _real)
        file(WRITE "${_fake_src}/${_name}"
            "#!/bin/sh\n"
            "for a in \"$@\"; do\n"
            "  case \"$a\" in -std=c++23|-std=c++2b) echo \"unrecognized $a\" >&2; exit 1 ;; esac\n"
            "done\n"
            "exec \"${_real}\" \"$@\"\n")
        file(COPY "${_fake_src}/${_name}" DESTINATION "${_fake}"
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    endforeach()
    execute_process(
        COMMAND ${_bash} ${_script} --compilers "${_fake}/g++" --output-dir ${_out_dir}
                --dry-run --std 17,23 --no-package --color never
        RESULT_VARIABLE skip_CODE OUTPUT_VARIABLE skip_OUT ERROR_VARIABLE skip_ERR)
    if(NOT skip_CODE EQUAL 0)
        message(FATAL_ERROR "--std 17,23 without C++23 exited with ${skip_CODE}:\n${skip_OUT}\n${skip_ERR}")
    endif()
    _expect_text("${skip_OUT}" "skipped:" "no C++23")
    _expect_text("${skip_OUT}" "C++23 (the compiler has no such standard" "no C++23")
    _expect_text("${skip_OUT}" "Dry run finished: 1 build(s) shown." "no C++23")
    _expect_no_text("${skip_OUT}" "-DCMAKE_CXX_STANDARD=23" "no C++23")
    execute_process(
        COMMAND ${_bash} ${_script} --compilers "${_fake}/g++" --output-dir ${_out_dir}
                --dry-run --std 23 --no-package --color never
        RESULT_VARIABLE only_CODE OUTPUT_VARIABLE only_OUT ERROR_VARIABLE only_ERR)
    if(only_CODE EQUAL 0)
        message(FATAL_ERROR "--std 23 with no C++23 compiler was accepted:\n${only_OUT}")
    endif()
    _expect_text("${only_ERR}" "none of the compilers knows the standards of --std 23" "only C++23")
endif()

# A wrong --color is refused.
_run(bad --dry-run --color purple)
if(bad_CODE EQUAL 0)
    message(FATAL_ERROR "--color purple was accepted")
endif()
_expect_text("${bad_ERR}" "--color must be auto, always or never" "bad colour")

# A relative --doxygen path is made absolute (the builds run in other
# directories): the command shown names the full path.
set(_doxy_dir "${CMAKE_CURRENT_BINARY_DIR}/release_dry_run_doxygen")
file(REMOVE_RECURSE "${_doxy_dir}")
file(WRITE "${_doxy_dir}/src/fake_doxygen" "#!/bin/sh\nexit 0\n")
file(COPY "${_doxy_dir}/src/fake_doxygen" DESTINATION "${_doxy_dir}"
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
execute_process(
    COMMAND ${_bash} ${_script} --compilers ${_cxx} --output-dir ${_out_dir}
            --dry-run --std 11 --formats tar.gz --color never --doxygen ./fake_doxygen
    WORKING_DIRECTORY "${_doxy_dir}"
    RESULT_VARIABLE rel_CODE OUTPUT_VARIABLE rel_OUT ERROR_VARIABLE rel_ERR)
if(NOT rel_CODE EQUAL 0)
    message(FATAL_ERROR "a relative --doxygen exited with ${rel_CODE}:\n${rel_OUT}\n${rel_ERR}")
endif()
_expect_text("${rel_OUT}" "-DDOXYGEN_EXECUTABLE=${_doxy_dir}/fake_doxygen" "relative --doxygen")

# --tag of a release older than the documentation in the package (its CMake
# files have no LUMEX_DOCUMENTATION_PREBUILT_DIR): the run warns and builds
# without share/doc, and needs no Doxygen. A newer tag does need it. The tags
# are in a repository of its own, with a copy of the script.
find_program(_git git)
if(_git)
    set(_tag_repo "${CMAKE_CURRENT_BINARY_DIR}/release_dry_run_tags")
    file(REMOVE_RECURSE "${_tag_repo}")
    file(MAKE_DIRECTORY "${_tag_repo}/cmake" "${_tag_repo}/Scripts/ReleaseTools")
    file(COPY "${_script}" DESTINATION "${_tag_repo}")
    file(COPY "${LUMEX_SOURCE_DIR}/Scripts/ReleaseTools/extract_diagnostics.py"
         DESTINATION "${_tag_repo}/Scripts/ReleaseTools")
    file(WRITE "${_tag_repo}/CMakeLists.txt" "project(LumexLib VERSION 1.0.0.0)\n")
    function(_tag_git)
        execute_process(
            COMMAND ${_git} -c user.email=test@example.com -c user.name=test ${ARGN}
            WORKING_DIRECTORY "${_tag_repo}"
            RESULT_VARIABLE _code OUTPUT_QUIET ERROR_VARIABLE _msg)
        if(NOT _code EQUAL 0)
            message(FATAL_ERROR "git ${ARGN} failed: ${_msg}")
        endif()
    endfunction()
    _tag_git(init -q .)
    file(WRITE "${_tag_repo}/cmake/LumexOptions.cmake" "# no documentation option\n")
    _tag_git(add -A)
    _tag_git(commit -q -m old)
    _tag_git(tag v-without-docs)
    file(WRITE "${_tag_repo}/cmake/LumexOptions.cmake" "set(LUMEX_DOCUMENTATION_PREBUILT_DIR \"\")\n")
    _tag_git(add -A)
    _tag_git(commit -q -m new)
    _tag_git(tag v-with-docs)
    set(_tag_out "${CMAKE_CURRENT_BINARY_DIR}/release_dry_run_tags_out")
    foreach(_pair "old_tag|v-without-docs" "new_tag|v-with-docs")
        string(REPLACE "|" ";" _pair "${_pair}")
        list(GET _pair 0 _id)
        list(GET _pair 1 _tag)
        execute_process(
            COMMAND ${_bash} ${_tag_repo}/create_release.sh --compilers ${_cxx} --output-dir ${_tag_out}
                    --dry-run --std 11 --formats tar.gz --color never --tag ${_tag}
                    --doxygen ${_out_dir}/no/such/doxygen
            RESULT_VARIABLE ${_id}_CODE OUTPUT_VARIABLE ${_id}_OUT ERROR_VARIABLE ${_id}_ERR)
    endforeach()
    if(NOT old_tag_CODE EQUAL 0)
        message(FATAL_ERROR "an old tag without Doxygen exited with ${old_tag_CODE}:\n${old_tag_OUT}\n${old_tag_ERR}")
    endif()
    _expect_text("${old_tag_ERR}" "cannot install a finished documentation" "old tag")
    _expect_no_text("${old_tag_OUT}" "generate_documentation" "old tag")
    _expect_text("${old_tag_OUT}" "-DLUMEX_BUILD_DOCUMENTATION=OFF" "old tag")
    if(new_tag_CODE EQUAL 0)
        message(FATAL_ERROR "a tag with the documentation option was built without Doxygen:\n${new_tag_OUT}")
    endif()
    _expect_text("${new_tag_ERR}" "pass --no-docs to build the packages without the documentation" "new tag")
endif()
