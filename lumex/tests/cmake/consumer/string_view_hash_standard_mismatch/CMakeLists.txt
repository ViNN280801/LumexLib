# Fixture of cmake.consumer_string_view_hash_standard_mismatch.
#
# A library built at C++11 and a program built at C++17 (and at C++20 when the
# compiler has it) must give the same std::hash value for the same text, so
# that a set filled by one is searched by the other. The producer translation
# unit is compiled at C++11, main.cpp at the other standards, and the
# executable compares the values and looks the keys up in the producer's set.
# The views are header-only: only the include directory of the checkout is
# needed.

cmake_minimum_required(VERSION 3.16)
project(LumexStringViewHashStandardMismatch LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

add_library(hash_producer STATIC producer.cpp)
target_include_directories(hash_producer PUBLIC "${LUMEX_SOURCE_DIR}")
set_target_properties(hash_producer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON)

# The executable the runner starts is named <case>_consumer: C++17. The C++20
# one is only built (when the compiler has it): the values do not depend on
# the standard of the program beyond C++11 vs later, which this pair covers.
add_executable(string_view_hash_standard_mismatch_consumer main.cpp)
target_link_libraries(string_view_hash_standard_mismatch_consumer PRIVATE hash_producer)
set_target_properties(string_view_hash_standard_mismatch_consumer PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    add_executable(string_view_hash_standard_mismatch_cxx20 main.cpp)
    target_link_libraries(string_view_hash_standard_mismatch_cxx20 PRIVATE hash_producer)
    set_target_properties(string_view_hash_standard_mismatch_cxx20 PROPERTIES
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endif()
