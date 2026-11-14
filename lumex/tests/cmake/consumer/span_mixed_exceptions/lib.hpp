// The functions of lib.cpp, built without exceptions.
#ifndef LUMEX_TESTS_CMAKE_SPAN_MIXED_EXCEPTIONS_LIB_HPP
#define LUMEX_TESTS_CMAKE_SPAN_MIXED_EXCEPTIONS_LIB_HPP

// at () of 3 elements: 0 span<int> index 3, 1 span<int> index 99, 2
// span<int const> index 3, 3 span<int, 3> index 3. None of them returns:
// std::terminate is called.
int lib_at_out_of_range (int overload);

#endif
