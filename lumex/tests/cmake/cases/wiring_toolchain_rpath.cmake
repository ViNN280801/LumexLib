# A compiler outside the system paths (GCC or LLVM under /opt, built from
# source) keeps its C++ runtime where the loader does not look: the tests,
# examples and libraries of the build tree load the older libstdc++ / libc++
# of the system and stop (GLIBCXX_3.4.32 not found, undefined symbol
# std::__1::__hash_memory). Every configured target gets the compiler's
# runtime directories as BUILD_RPATH through CMakeRoutines, the nested consumer
# builds get them as CMAKE_BUILD_RPATH and run without LD_LIBRARY_PATH, and
# the install tree is not touched (its RPATH is the package's own).

foreach(_file
        "${LUMEX_SOURCE_DIR}/CMakeRoutines/deployment/ToolchainRuntimeRpath.cmake")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR
            "the CMakeRoutines submodule has no deployment/ToolchainRuntimeRpath.cmake: update it")
    endif()
endforeach()
file(READ "${LUMEX_SOURCE_DIR}/CMakeRoutines/deployment/ToolchainRuntimeRpath.cmake" _module)
foreach(_needle "function(configure_toolchain_runtime_rpath target)"
                "function(get_toolchain_runtime_directories out_var)"
                "BUILD_RPATH")
    string(FIND "${_module}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR
            "CMakeRoutines ToolchainRuntimeRpath.cmake does not contain: ${_needle}")
    endif()
endforeach()

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexBuild.cmake" _build)
string(FIND "${_build}" "include(deployment/ToolchainRuntimeRpath)" _include_pos)
if(_include_pos EQUAL -1)
    message(FATAL_ERROR
        "cmake/LumexBuild.cmake does not include deployment/ToolchainRuntimeRpath")
endif()

# lumex_configure_target applies the runtime directories to the target, after
# configure_optimization_level (which decides -stdlib=libc++).
string(FIND "${_build}" "function(lumex_configure_target " _configure_pos)
string(FIND "${_build}" "function(lumex_configure_targets_in_dir " _end_pos)
if(_configure_pos EQUAL -1 OR _end_pos EQUAL -1 OR _end_pos LESS _configure_pos)
    message(FATAL_ERROR "cmake/LumexBuild.cmake: lumex_configure_target not found")
endif()
math(EXPR _len "${_end_pos} - ${_configure_pos}")
string(SUBSTRING "${_build}" ${_configure_pos} ${_len} _body)
string(FIND "${_body}" "configure_optimization_level(" _level_pos)
string(FIND "${_body}" "configure_toolchain_runtime_rpath(\"\${target_name}\")" _rpath_pos)
if(_rpath_pos EQUAL -1)
    message(FATAL_ERROR
        "lumex_configure_target does not call configure_toolchain_runtime_rpath")
endif()
if(_level_pos EQUAL -1 OR _rpath_pos LESS _level_pos)
    message(FATAL_ERROR
        "configure_toolchain_runtime_rpath must run after configure_optimization_level "
        "(the -stdlib= choice is on the target by then)")
endif()

# Build tree only: no raw linker option. A -Wl,-rpath on a target would be in
# the installed binary too, next to the package's own RPATH.
foreach(_raw "-Wl,-rpath" "LINKER:-rpath" "-rpath," "INSTALL_RPATH")
    string(FIND "${_build}" "${_raw}" _raw_pos)
    if(NOT _raw_pos EQUAL -1)
        message(FATAL_ERROR
            "cmake/LumexBuild.cmake sets '${_raw}': the toolchain runtime goes "
            "into BUILD_RPATH only, through configure_toolchain_runtime_rpath")
    endif()
endforeach()

# The consumer cases: the directories are computed once, handed to the runner
# as BUILD_RPATH, forwarded to the nested configure as CMAKE_BUILD_RPATH, and
# the fixture starts without the caller's LD_LIBRARY_PATH.
file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/CMakeLists.txt" _cases)
foreach(_needle
        "get_toolchain_runtime_directories(_LUMEX_CONSUMER_RPATH STDLIB libstdc++ libc++)"
        "\"-DBUILD_RPATH=\${_LUMEX_CONSUMER_RPATH}\"")
    string(FIND "${_cases}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR
            "lumex/tests/cmake/CMakeLists.txt does not contain: ${_needle}")
    endif()
endforeach()
file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/run_consumer.cmake" _runner)
foreach(_needle
        "-DCMAKE_BUILD_RPATH=\${_build_rpath}"
        "-E env --unset=LD_LIBRARY_PATH")
    string(FIND "${_runner}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "run_consumer.cmake does not contain: ${_needle}")
    endif()
endforeach()
