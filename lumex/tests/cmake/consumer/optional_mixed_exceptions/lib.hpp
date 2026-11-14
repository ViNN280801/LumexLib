// The functions of lib.cpp, built without exceptions.
#ifndef LUMEX_TESTS_CMAKE_OPTIONAL_MIXED_EXCEPTIONS_LIB_HPP
#define LUMEX_TESTS_CMAKE_OPTIONAL_MIXED_EXCEPTIONS_LIB_HPP

// value () of an empty optional<int>: 0 lvalue, 1 rvalue, 2 const lvalue,
// 3 const rvalue. None of them returns: std::terminate is called.
int lib_value_of_empty (int overload);

#endif
