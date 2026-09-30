# lumex/tests/core/base64/CMakeLists.txt

set(LUMEX_BASE64_TEST_SOURCES
    LumexBase64Decoder.tests.cpp
    LumexBase64Encoder.tests.cpp
    LumexBase64Validator.tests.cpp
)

# C++11 runs the std::string wrappers, C++20 the std::string_view (C++17)
# and std::span (C++20) wrappers. Both link the same exported pointer and
# size functions, whatever standard the library itself was built with.
foreach(_base64_suite IN ITEMS
        "LumexBase64Tests;11;"
        "LumexBase64Cxx20Tests;20;.cxx20")
    list(GET _base64_suite 0 _base64_name)
    list(GET _base64_suite 1 _base64_std)
    list(GET _base64_suite 2 _base64_suffix)

    add_executable(${_base64_name} ${LUMEX_BASE64_TEST_SOURCES})

    set_target_properties(${_base64_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )

    target_link_libraries(
        ${_base64_name}
        PRIVATE
        lumex::base64
    )

    lumex_test_use_gtest(${_base64_name} CXX_STANDARD ${_base64_std})
    if(_base64_suffix)
        lumex_gtest_discover_tests(${_base64_name} TEST_SUFFIX "${_base64_suffix}")
    else()
        lumex_gtest_discover_tests(${_base64_name})
    endif()
endforeach()
