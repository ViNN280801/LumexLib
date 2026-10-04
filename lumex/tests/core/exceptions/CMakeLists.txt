# lumex/tests/core/exceptions/CMakeLists.txt

# HIGH flags and CMAKE_CXX_FLAGS both inject /EHsc. Replace it here so
# /EHa is not a late override (MSVC D9025) and C4535 stays gone.
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

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexExceptionsCxx11Tests  the .cxx11.tests.cpp files: LumexException,
#                              LumexExceptionHeader, LumexExceptionWrapper,
#                              WindowsSEHTranslator
#   LumexExceptionsCxx17Tests  + LumexException.cxx17.tests.cpp: the
#                              std::string_view constructor, an inline wrapper
#                              over the exported std::string one
#   LumexExceptionsCxx20Tests  the same files at C++20
# LumexExceptionTestFixtures.hpp holds the fixture the files of every standard
# share.
lumex_add_standard_suites(Exceptions
    MODULE exceptions
    LINK
        lumex::filesystem
        lumex::exceptions
        lumex::time
        lumex::utility
    SOURCES LumexExceptionTestFixtures.hpp
    # The C++11, C++17 and C++20 executables share bin/. TearDown of
    # LumexExceptionTest removes <exe dir>/crashes, and to_crash_report
    # writes only there. One lock keeps every discovered test of every
    # standard from deleting that directory under ctest -j.
    DISCOVER_ARGS
        PROPERTIES RESOURCE_LOCK lumex_exception_crash_dir
    TARGETS_VAR _exceptions_suites
)

foreach(_suite IN LISTS _exceptions_suites)
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
