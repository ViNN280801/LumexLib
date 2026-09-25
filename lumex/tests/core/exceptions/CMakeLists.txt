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

set(TEST_EXECUTABLE_NAME LumexExceptionsTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexException.tests.cpp
    LumexExceptionWrapper.tests.cpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::filesystem
    lumex::exceptions
    lumex::time
    lumex::utility
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)

if(WIN32)
    target_compile_options(${TEST_EXECUTABLE_NAME} PRIVATE

        # Generate program database (PDB) debug information for source files
        # /Zi is needed for full debug symbols with MSVC.
        # /EHa is applied after HIGH flags in lumex_configure_target so it
        # stays last on the cl command line (C4535 / D9025).
        /Zi
    )
    target_link_options(${TEST_EXECUTABLE_NAME} PRIVATE

        # Linker flag to generate debug information (PDB file)
        /DEBUG
    )
elseif(UNIX AND NOT APPLE)
    # Ensure debug symbols are generated and exported for the test executable
    target_compile_options(${TEST_EXECUTABLE_NAME} PRIVATE -g)
    target_link_options(${TEST_EXECUTABLE_NAME} PRIVATE -rdynamic) # needed for backtrace_symbols to resolve names
endif()

lumex_gtest_discover_tests(${TEST_EXECUTABLE_NAME})
