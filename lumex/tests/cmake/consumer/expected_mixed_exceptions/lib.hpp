// The functions of lib.cpp, built without exceptions.
#ifndef LUMEX_TESTS_CMAKE_EXPECTED_MIXED_EXCEPTIONS_LIB_HPP
#define LUMEX_TESTS_CMAKE_EXPECTED_MIXED_EXCEPTIONS_LIB_HPP

// value () of an expected that holds an error: 0 lvalue, 1 rvalue, 2 const
// lvalue, 3 const rvalue, 4 void const lvalue, 5 void rvalue. None of them
// returns: std::terminate is called.
int lib_value_of_error (int overload);

#endif
