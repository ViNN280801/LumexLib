# lumex/tests/core/math/CMakeLists.txt

# LumexMath works from C++11 on: the same source at C++11, C++17 and C++20.
# The C++20 suite also covers std::views (non-const-iterable filter views,
# sentinel ends).
foreach(_math_suite IN ITEMS
        "LumexMathTests;11;"
        "LumexMathCxx17Tests;17;.cxx17"
        "LumexMathCxx20Tests;20;.cxx20")
    list(GET _math_suite 0 _math_name)
    list(GET _math_suite 1 _math_std)
    list(GET _math_suite 2 _math_suffix)

    add_executable(${_math_name} LumexMath.tests.cpp)

    set_target_properties(${_math_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )

    target_link_libraries(${_math_name} PRIVATE lumex::math)

    lumex_test_use_gtest(${_math_name} CXX_STANDARD ${_math_std})
    if(_math_suffix)
        lumex_gtest_discover_tests(${_math_name} TEST_SUFFIX "${_math_suffix}")
    else()
        lumex_gtest_discover_tests(${_math_name})
    endif()
endforeach()
