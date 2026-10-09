// The reason for optional_aligned_storage.cpp: std::aligned_storage is the
// thing C++23 deprecates. cmake.hygiene_compile_checks compiles this unit at
// C++23 under -Wdeprecated-declarations -Werror and expects it to be rejected
// as deprecated. If the standard library of the toolchain does not deprecate
// it (a compiler with C++23 but an older library), the optional check proves
// nothing there and is skipped with a message.

#include <type_traits>

std::aligned_storage<4, 4>::type baseline_storage;
