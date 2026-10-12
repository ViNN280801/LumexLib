# A compiler outside the system paths (GCC or LLVM under /opt, built from
# source) keeps its C++ runtime where the loader does not look: the tests,
# examples and libraries of the build tree load the older libstdc++ / libc++
# of the system and stop (GLIBCXX_3.4.32 not found, undefined symbol
# std::__1::__hash_memory). Every configured target gets the compiler's
# runtime directories as BUILD_RPATH through CMakeRoutines
# (configure_optimization_level ... TOOLCHAIN_RUNTIME_RPATH ON), the nested consumer
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

# lumex_configure_target asks for the runtime directories through the
# TOOLCHAIN_RUNTIME_RPATH parameter of configure_optimization_level (which
# decides -stdlib=libc++ and then applies the RPATH as its last step), and not
# through a second call of its own.
file(READ "${LUMEX_SOURCE_DIR}/CMakeRoutines/optimizations/OptimizationLevelConfig.cmake" _level_module)
foreach(_needle "TOOLCHAIN_RUNTIME_RPATH"
                "configure_toolchain_runtime_rpath(\${target})")
    string(FIND "${_level_module}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR
            "CMakeRoutines OptimizationLevelConfig.cmake does not contain: ${_needle}")
    endif()
endforeach()
string(FIND "${_build}" "function(lumex_configure_target " _configure_pos)
string(FIND "${_build}" "function(lumex_configure_targets_in_dir " _end_pos)
if(_configure_pos EQUAL -1 OR _end_pos EQUAL -1 OR _end_pos LESS _configure_pos)
    message(FATAL_ERROR "cmake/LumexBuild.cmake: lumex_configure_target not found")
endif()
math(EXPR _len "${_end_pos} - ${_configure_pos}")
string(SUBSTRING "${_build}" ${_configure_pos} ${_len} _body)
string(REGEX MATCH
    "configure_optimization_level\\(\"\\$\\{target_name\\}\"[^)]*TOOLCHAIN_RUNTIME_RPATH ON\\)"
    _call "${_body}")
if(_call STREQUAL "")
    message(FATAL_ERROR
        "lumex_configure_target does not pass TOOLCHAIN_RUNTIME_RPATH ON to "
        "configure_optimization_level")
endif()
string(FIND "${_body}" "configure_toolchain_runtime_rpath(" _second_call_pos)
if(NOT _second_call_pos EQUAL -1)
    message(FATAL_ERROR
        "lumex_configure_target calls configure_toolchain_runtime_rpath itself: "
        "the TOOLCHAIN_RUNTIME_RPATH parameter does it")
endif()

# Build tree only: no raw linker option. A -Wl,-rpath on a target would be in
# the installed binary too, next to the package's own RPATH.
foreach(_raw "-Wl,-rpath" "LINKER:-rpath" "-rpath," "INSTALL_RPATH")
    string(FIND "${_build}" "${_raw}" _raw_pos)
    if(NOT _raw_pos EQUAL -1)
        message(FATAL_ERROR
            "cmake/LumexBuild.cmake sets '${_raw}': the toolchain runtime goes "
            "into BUILD_RPATH only, through configure_optimization_level TOOLCHAIN_RUNTIME_RPATH")
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
