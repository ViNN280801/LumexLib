# The C++ standard library under Clang is chosen by the top-level project
# (CMakeRoutines configure_optimization_level CXX_STDLIB). LUMEX_CLANG_STDLIB
# defaults to AUTO when LumexLib is the top-level project and to DEFAULT (no
# -stdlib: the parent's library) when a parent embeds it, is validated,
# reaches every configured target through LumexBuild, is set by the Conan
# recipe from compiler.libcxx, and the CMakeRoutines submodule accepts it.

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _opts)
string(REGEX MATCH
    "if\\(LUMEX_IS_TOP_LEVEL\\)[ \n]*set\\(_lumex_clang_stdlib_default AUTO\\)[ \n]*else\\(\\)[ \n]*set\\(_lumex_clang_stdlib_default DEFAULT\\)[ \n]*endif\\(\\)[ \n]*set\\(LUMEX_CLANG_STDLIB \"\\$\\{_lumex_clang_stdlib_default\\}\" CACHE STRING"
    _decl "${_opts}")
if(_decl STREQUAL "")
    message(FATAL_ERROR
        "cmake/LumexOptions.cmake: LUMEX_CLANG_STDLIB must be a cache STRING "
        "that defaults to AUTO at top level and to DEFAULT when embedded")
endif()
foreach(_needle
        "set_property(CACHE LUMEX_CLANG_STDLIB PROPERTY STRINGS AUTO LIBCXX DEFAULT)"
        "Invalid LUMEX_CLANG_STDLIB=")
    string(FIND "${_opts}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "cmake/LumexOptions.cmake does not contain: ${_needle}")
    endif()
endforeach()

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexBuild.cmake" _build)
string(REGEX MATCH
    "configure_optimization_level\\(\"\\$\\{target_name\\}\"[^)]*CXX_STDLIB \"\\$\\{LUMEX_CLANG_STDLIB\\}\"[^)]*\\)"
    _call "${_build}")
if(_call STREQUAL "")
    message(FATAL_ERROR
        "cmake/LumexBuild.cmake does not pass LUMEX_CLANG_STDLIB to "
        "configure_optimization_level as CXX_STDLIB")
endif()

file(READ "${LUMEX_SOURCE_DIR}/conanfile.py" _conan)
foreach(_needle
        "compiler.libcxx"
        "tc.cache_variables[\"LUMEX_CLANG_STDLIB\"] = \"LIBCXX\""
        "tc.cache_variables[\"LUMEX_CLANG_STDLIB\"] = \"DEFAULT\"")
    string(FIND "${_conan}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "conanfile.py does not contain: ${_needle}")
    endif()
endforeach()

file(READ "${LUMEX_SOURCE_DIR}/CMakeRoutines/optimizations/OptimizationLevelConfig.cmake"
    _routine)
if(NOT _routine MATCHES "set\\(oneValueArgs[^)]*CXX_STDLIB")
    message(FATAL_ERROR
        "the CMakeRoutines submodule does not support CXX_STDLIB: update it")
endif()

# Consumer cases build embedded LumexLib like the tree that runs them: the
# runner forwards that tree's LUMEX_CLANG_STDLIB and CMAKE_CXX_FLAGS (without
# them an embedded Clang build falls back to DEFAULT, which on Astra Linux
# is GCC 8's libstdc++ and cannot build the C++20 fixtures).
file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/CMakeLists.txt" _cases)
foreach(_needle "-DCLANG_STDLIB=\${LUMEX_CLANG_STDLIB}"
                "\"-DCXX_FLAGS=\${CMAKE_CXX_FLAGS}\"")
    string(FIND "${_cases}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR
            "lumex/tests/cmake/CMakeLists.txt does not pass ${_needle} to the consumer runner")
    endif()
endforeach()
file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/run_consumer.cmake" _runner)
foreach(_needle "-DLUMEX_CLANG_STDLIB=\${CLANG_STDLIB}"
                "-DCMAKE_CXX_FLAGS=\${CXX_FLAGS}")
    string(FIND "${_runner}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR
            "run_consumer.cmake does not forward ${_needle} to the nested configure")
    endif()
endforeach()
