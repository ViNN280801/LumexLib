# lumex/tests/core/exceptions/CMakeLists.txt

# HIGH flags and CMAKE_CXX_FLAGS both inject /EHsc. Replace it here so
# /EHa is not a late override (MSVC D9025) and C4535 stays gone. The
# directories below inherit the flags.
if(MSVC)
    foreach(_lumex_eh_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS_MINSIZEREL)
        if(DEFINED ${_lumex_eh_var})
            string(REPLACE "/EHsc" "/EHa" ${_lumex_eh_var} "${${_lumex_eh_var}}")
            string(REPLACE "/EHs" "/EHa" ${_lumex_eh_var} "${${_lumex_eh_var}}")
        endif()
    endforeach()
endif()

# The libraries every exceptions suite links; the directories below use them.
set(LUMEX_EXCEPTIONS_TEST_LINK
    lumex::filesystem
    lumex::exceptions
    lumex::time
    lumex::utility
)

# The options every exceptions suite gets: debug symbols, and on Linux the
# symbols exported so that backtrace_symbols can name the frames.
# lumex_exceptions_test_options (<target>...) is called by each directory with
# the suites it built.
function(lumex_exceptions_test_options)
    foreach(_suite IN LISTS ARGN)
        if(WIN32)
            target_compile_options(${_suite} PRIVATE

                # Generate program database (PDB) debug information for source files
                # /Zi is needed for full debug symbols with MSVC.
                # /EHa is applied after HIGH flags in lumex_configure_target so it
                # stays last on the cl command line (C4535 / D9025).
                /Zi
            )
            target_link_options(${_suite} PRIVATE

                # Linker flag to generate debug information (PDB file)
                /DEBUG
            )
        elseif(UNIX AND NOT APPLE)
            # Ensure debug symbols are generated and exported for the test executable
            target_compile_options(${_suite} PRIVATE -g)
            target_link_options(${_suite} PRIVATE -rdynamic) # needed for backtrace_symbols to resolve names
        endif()
    endforeach()
endfunction()

# The tests follow the source tree (lumex/core/exceptions), one suite per
# standard of the module (lumex/tests/LumexTestStandards.cmake) in each
# directory:
#   this one      LumexExceptionWrapper.cxx11.tests.cpp: the wrapper beside the
#                 umbrella (LumexExceptionsCxx11Tests, ...Cxx17Tests,
#                 ...Cxx20Tests)
#   exception/    lumex_base_exception, the macros and the header
#   stacktrace/   lumex_stacktrace and lumex_stacktrace_entry
#   crash/        the SEH translator
# The executables of every directory share bin/. TearDown of
# LumexExceptionTest removes <exe dir>/crashes, and to_crash_report writes
# only there. One lock keeps every discovered test of every directory and
# standard from deleting that directory under ctest -j.
lumex_add_standard_suites(Exceptions
    MODULE exceptions
    LINK ${LUMEX_EXCEPTIONS_TEST_LINK}
    DISCOVER_ARGS
        PROPERTIES RESOURCE_LOCK lumex_exception_crash_dir
    TARGETS_VAR _exceptions_suites
)
lumex_exceptions_test_options(${_exceptions_suites})

add_subdirectory(crash)
add_subdirectory(exception)
add_subdirectory(stacktrace)
