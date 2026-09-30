# Consumer fixture for LumexCMake.consumer_standard_mismatch_*.
#
# The embedded Lumex modules are compiled at LIB_STD and this consumer at
# CONSUMER_STD. The exported functions of base64, XML and exceptions have one
# signature in every standard; the overloads that depend on the standard
# (std::string_view and std::span from C++17 / C++20, std::string const &
# below C++17) are inline wrappers in the headers. The consumer calls the
# overloads of its own standard, so it links whatever the pair is, and an
# exported function that regains a standard-dependent signature fails here.

cmake_minimum_required(VERSION 3.16)
project(LumexStandardMismatchConsumer LANGUAGES CXX)

foreach(_var LUMEX_SOURCE_DIR LIB_STD CONSUMER_STD)
    if(NOT ${_var})
        message(FATAL_ERROR "pass -D${_var}=...")
    endif()
endforeach()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML ON CACHE BOOL "" FORCE)
foreach(_opt
        CIRCULAR_BUFFER CRC EXPECTED FMT GENERATORS OPTIONAL REFLECTION
        STRING_VIEW TEMPORARY)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(_opt
        BASE64 ENVIRONMENT EXCEPTIONS FILESYSTEM MATH STRING TIME UTILITY)
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
    lumex::exceptions
    lumex::xml
)
set_target_properties(standard_mismatch_consumer PROPERTIES
    CXX_STANDARD ${CONSUMER_STD}
    CXX_STANDARD_REQUIRED ON
)
