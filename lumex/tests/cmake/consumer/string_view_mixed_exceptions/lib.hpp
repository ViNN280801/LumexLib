// The functions of lib.cpp, built without exceptions.
#ifndef LUMEX_TESTS_CMAKE_STRING_VIEW_MIXED_EXCEPTIONS_LIB_HPP
#define LUMEX_TESTS_CMAKE_STRING_VIEW_MIXED_EXCEPTIONS_LIB_HPP

#include <ostream>

#include "lumex/core/string_view/LumexStringView"

char lib_first (lumex::core::string_view::view::lumex_string_view view);
void lib_print (std::ostream &out,
                lumex::core::string_view::view::lumex_string_view view);

#endif
