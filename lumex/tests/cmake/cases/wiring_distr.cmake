# Distr publish wiring: shared libs only, no in-build ctest, no test binaries.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_forbid_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "${path} still mentions ${needle}")
    endif()
endfunction()

_require_text("CMakeLists.txt" "lumex_add_publish_distr_target")
_require_text("cmake/LumexModules.cmake" "lumex_add_publish_distr_target")
_require_text("cmake/LumexModules.cmake" "publish_distr ALL")
_require_text("cmake/LumexModules.cmake" "LUMEX_SHARED_LIBRARY_CANDIDATES")
_require_text("cmake/PublishDistr.cmake" "Never copies test or example binaries")
_require_text("cmake/PublishDistr.cmake" "Tests")
_require_text("cmake/PublishDistr.cmake" "Example")
_require_text("cmake/PublishDistr.cmake" "copy_runtime_script")

# The staged compiler runtime must be the one the compiler links, not the
# ldconfig match: with GCC 13.2 in /opt on Astra Linux SE 1.7 the ldconfig
# match is the system libstdc++ 6.0.25, which lacks GLIBCXX_3.4.30.
_require_text("cmake/LumexModules.cmake" "-Dcxx_compiler=\${CMAKE_CXX_COMPILER}")
file(READ "${LUMEX_SOURCE_DIR}/cmake/PublishDistr.cmake" _publish)
string(FIND "${_publish}" "include(\"\${copy_runtime_script}\")" _include_pos)
string(FIND "${_publish}" "-print-file-name=\${_runtime}" _print_pos)
if(_include_pos EQUAL -1 OR _print_pos EQUAL -1
   OR _print_pos LESS _include_pos)
    message(FATAL_ERROR
        "cmake/PublishDistr.cmake must replace the staged runtime with the "
        "compiler's own file (-print-file-name) after CopyRuntimeDependencies")
endif()
foreach(_runtime libstdc++.so.6 libgcc_s.so.1 libc++.so.1 libc++abi.so.1
                 libunwind.so.1)
    string(FIND "${_publish}" "${_runtime}" _runtime_pos)
    if(_runtime_pos EQUAL -1)
        message(FATAL_ERROR
            "cmake/PublishDistr.cmake does not re-stage ${_runtime}")
    endif()
endforeach()

_forbid_text("CMakeLists.txt" "run_all_tests ALL")
_forbid_text("CMakeLists.txt" "lumex_add_run_all_tests_target")
_forbid_text("cmake/LumexModules.cmake" "lumex_add_run_all_tests_target")
_forbid_text("cmake/LumexModules.cmake" "run_all_tests ALL")
_forbid_text("cmake/PublishDistr.cmake" "Lumex*Tests")

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake" _mod)
foreach(_tgt
        LumexCore_atomic LumexCore_base64 LumexCore_circular_buffer LumexCore_crc
        LumexCore_environment LumexCore_exceptions LumexCore_expected
        LumexCore_filesystem LumexCore_number_generator LumexCore_math
        LumexCore_optional LumexCore_reflection LumexCore_string LumexCore_fmt
        LumexCore_string_view LumexCore_temporary LumexCore_time
        LumexCore_unicode LumexCore_utility
        LumexApplied_hardware LumexApplied_json LumexApplied_logger
        LumexApplied_logging
        LumexApplied_resource_monitor LumexApplied_serial LumexApplied_settings
        LumexXml)
    string(FIND "${_mod}" "${_tgt}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR
            "LUMEX_SHARED_LIBRARY_CANDIDATES omits ${_tgt}")
    endif()
endforeach()
