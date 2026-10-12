# Consumer fixture for cmake.consumer_standard_mismatch_*.
#
# The embedded Lumex modules are compiled at LIB_STD and this consumer at
# CONSUMER_STD. The exported functions of base64, XML and exceptions have one
# signature in every standard; the overloads that depend on the standard
# (std::string_view from C++17 and the lumex_string_view below, std::span from
# C++20) are inline wrappers in the headers. The consumer calls the overloads
# of its own standard, so it links whatever the pair is, and an exported
# function that regains a standard-dependent signature fails here.

cmake_minimum_required(VERSION 3.16)
project(LumexStandardMismatchConsumer LANGUAGES CXX)

foreach(_var LUMEX_SOURCE_DIR LIB_STD CONSUMER_STD)
    if(NOT ${_var})
        message(FATAL_ERROR "pass -D${_var}=...")
    endif()
endforeach()

# The pair of this fixture needs a library built at C++11 (LIB_STD=11) and a
# consumer at C++11 (CONSUMER_STD=11): MSVC has no C++11 mode - /std:c++14 is
# the lowest it accepts - so the scenario cannot be built there. The C++14
# library leg would be a different fixture (the exported signatures of the
# modules are the same in C++14 and C++11, so the check would have to be
# reworded), and this fixture reports itself as skipped instead.
if(MSVC)
    message("LUMEX_CONSUMER_SKIP: MSVC has no C++11 mode for the library and "
            "the consumer leg of this check")
    return()
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML ON CACHE BOOL "" FORCE)
foreach(_opt
        CIRCULAR_BUFFER EXPECTED FMT GENERATORS REFLECTION TEMPORARY)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(_opt
        BASE64 CRC ENVIRONMENT EXCEPTIONS FILESYSTEM MATH OPTIONAL SPAN STRING
        STRING_VIEW TIME UTILITY)
    set(LUMEX_BUILD_${_opt} ON CACHE BOOL "" FORCE)
endforeach()

# Every Lumex target created below without a standard of its own takes this
# one, which is how a parent project picks the standard of the library.
set(CMAKE_CXX_STANDARD ${LIB_STD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

foreach(_target LumexCore_base64 LumexCore_exceptions LumexXml)
    get_target_property(_std ${_target} CXX_STANDARD)
    if(NOT "${_std}" STREQUAL "${LIB_STD}")
        message(FATAL_ERROR
            "${_target} builds at C++${_std}, expected C++${LIB_STD}")
    endif()
endforeach()

add_executable(standard_mismatch_consumer main.cpp)
target_link_libraries(standard_mismatch_consumer PRIVATE
    lumex::base64
    lumex::crc
    lumex::exceptions
    lumex::xml
)
set_target_properties(standard_mismatch_consumer PROPERTIES
    CXX_STANDARD ${CONSUMER_STD}
    CXX_STANDARD_REQUIRED ON
)
