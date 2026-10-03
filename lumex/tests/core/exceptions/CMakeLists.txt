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

# C++11 is the floor of the module; C++20 also runs the std::string_view
# constructor (C++17), an inline wrapper over the exported std::string one.
foreach(_exceptions_suite IN ITEMS
        "LumexExceptionsTests;11;"
        "LumexExceptionsCxx20Tests;20;.cxx20")
    list(GET _exceptions_suite 0 _exceptions_name)
    list(GET _exceptions_suite 1 _exceptions_std)
    list(GET _exceptions_suite 2 _exceptions_suffix)

    add_executable(
        ${_exceptions_name}
        LumexException.tests.cpp
        LumexExceptionHeader.tests.cpp
        LumexExceptionWrapper.tests.cpp
        WindowsSEHTranslator.tests.cpp
    )

    set_target_properties(${_exceptions_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )

    target_link_libraries(
        ${_exceptions_name}
        PRIVATE
        lumex::filesystem
        lumex::exceptions
        lumex::time
        lumex::utility
    )

    lumex_test_use_gtest(${_exceptions_name} CXX_STANDARD ${_exceptions_std})

    if(WIN32)
        target_compile_options(${_exceptions_name} PRIVATE

            # Generate program database (PDB) debug information for source files
            # /Zi is needed for full debug symbols with MSVC.
            # /EHa is applied after HIGH flags in lumex_configure_target so it
            # stays last on the cl command line (C4535 / D9025).
            /Zi
        )
        target_link_options(${_exceptions_name} PRIVATE

            # Linker flag to generate debug information (PDB file)
            /DEBUG
        )
    elseif(UNIX AND NOT APPLE)
        # Ensure debug symbols are generated and exported for the test executable
        target_compile_options(${_exceptions_name} PRIVATE -g)
        target_link_options(${_exceptions_name} PRIVATE -rdynamic) # needed for backtrace_symbols to resolve names
    endif()

    if(_exceptions_suffix)
        lumex_gtest_discover_tests(${_exceptions_name} TEST_SUFFIX "${_exceptions_suffix}")
    else()
        lumex_gtest_discover_tests(${_exceptions_name})
    endif()
endforeach()
