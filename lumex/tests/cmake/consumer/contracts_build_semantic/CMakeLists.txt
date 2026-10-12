# Fixture of cmake.consumer_contracts_build_semantic_*: LumexLib is embedded
# with the cache variable LUMEX_CONTRACTS_SEMANTIC set to SEMANTIC; an
# executable that links lumex::contracts and does not define the macro
# LUMEX_CONTRACTS_SEMANTIC itself must get that semantic for
# LUMEX_CONTRACT_ASSERT (main.cpp checks the behavior), the target must carry
# the definition for its consumers, and a translation unit that defines the
# macro itself must win over it.

cmake_minimum_required(VERSION 3.16)
project(LumexContractsBuildSemantic LANGUAGES CXX)

foreach(_var LUMEX_SOURCE_DIR SEMANTIC)
    # Not if(NOT ${_var}): the word "ignore" is a false constant for if().
    if("${${_var}}" STREQUAL "")
        message(FATAL_ERROR "pass -D${_var}=...")
    endif()
endforeach()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT GENERATORS HAZARD_POINTER REFLECTION STRING STRING_VIEW TEMPORARY
        TIME UNICODE)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(_opt CONTRACTS MATH OPTIONAL SPAN UTILITY)
    set(LUMEX_BUILD_${_opt} ON CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_CONTRACTS_SEMANTIC "${SEMANTIC}" CACHE STRING "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

string(TOLOWER "${SEMANTIC}" _word)
get_target_property(_definitions lumex::contracts INTERFACE_COMPILE_DEFINITIONS)
if(NOT "LUMEX_CONTRACTS_BUILD_SEMANTIC=${_word}" IN_LIST _definitions)
    message(FATAL_ERROR
        "lumex::contracts does not pass LUMEX_CONTRACTS_BUILD_SEMANTIC=${_word}: ${_definitions}")
endif()

if(_word STREQUAL "ignore")
    set(_requested 1)
elseif(_word STREQUAL "observe")
    set(_requested 2)
elseif(_word STREQUAL "enforce")
    set(_requested 3)
elseif(_word STREQUAL "quick_enforce")
    set(_requested 4)
else()
    set(_requested 5)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(contracts_build_semantic_consumer main.cpp overridden.cpp)
target_link_libraries(contracts_build_semantic_consumer PRIVATE lumex::contracts)
target_compile_definitions(contracts_build_semantic_consumer PRIVATE
    EXPECTED_REQUESTED=${_requested})
