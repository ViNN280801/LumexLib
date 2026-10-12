# CMakeRoutines builds Lumex targets with -stdlib=libc++ on Clang whenever
# libc++ is usable. The vendored GoogleTest libraries skip
# lumex_configure_target, so without an explicit hand-over they stay on the
# compiler's default library (libstdc++ on Linux) while the tests use libc++,
# and every test fails to link on the differently mangled std::string. The
# choice is recorded per configured target and mirrored onto the four
# GoogleTest libraries.

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexBuild.cmake" _build)

string(FIND "${_build}" "function(lumex_configure_target " _configure_pos)
string(FIND "${_build}" "function(lumex_configure_all_compiled_targets)"
    _all_pos)
if(_configure_pos EQUAL -1 OR _all_pos EQUAL -1)
    message(FATAL_ERROR "cmake/LumexBuild.cmake: configure functions not found")
endif()

string(FIND "${_build}"
    "set_property(GLOBAL PROPERTY LUMEX_CXX_STDLIB_OPTION \"-stdlib=libc++\")"
    _record_pos)
if(_record_pos EQUAL -1 OR _record_pos LESS _configure_pos
   OR (_all_pos GREATER _configure_pos AND _record_pos GREATER _all_pos))
    message(FATAL_ERROR
        "lumex_configure_target does not record the -stdlib=libc++ choice")
endif()

string(SUBSTRING "${_build}" ${_all_pos} -1 _all_body)
string(FIND "${_all_body}" "get_property(_lumex_stdlib GLOBAL PROPERTY LUMEX_CXX_STDLIB_OPTION)"
    _read_pos)
if(_read_pos EQUAL -1)
    message(FATAL_ERROR
        "lumex_configure_all_compiled_targets does not read the recorded "
        "C++ standard library choice")
endif()
string(SUBSTRING "${_all_body}" ${_read_pos} -1 _mirror)
foreach(_gtest lumex_gtest_1_12 lumex_gtest_main_1_12
               lumex_gtest_1_18 lumex_gtest_main_1_18)
    string(FIND "${_mirror}" "${_gtest}" _gtest_pos)
    if(_gtest_pos EQUAL -1)
        message(FATAL_ERROR
            "${_gtest} does not receive the tests' C++ standard library")
    endif()
endforeach()
if(NOT _mirror MATCHES "target_compile_options\\(\"\\$\\{_g\\}\" PRIVATE \\$\\{_lumex_stdlib\\}\\)")
    message(FATAL_ERROR
        "the recorded C++ standard library option is not applied to the "
        "GoogleTest libraries")
endif()
