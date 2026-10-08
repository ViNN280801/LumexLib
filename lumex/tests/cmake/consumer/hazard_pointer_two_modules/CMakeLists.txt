# Fixture of cmake.consumer_hazard_pointer_two_modules (see main.cpp).
#
# Two shared libraries with hidden visibility and an executable, all linking
# the shared hazard pointer library: a reader in one library and a reclaiming
# thread in another must see the same hazard slots, which is why the engine is
# compiled into one library instead of living in the headers (a header-only
# domain would be one copy per DLL on Windows). The libraries and the
# executable run at C++11 while the hazard pointer library is C++11 as well;
# the point is the sharing, not the standard.

cmake_minimum_required(VERSION 3.16)
project(LumexHazardPointerTwoModules LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT GENERATORS REFLECTION STRING STRING_VIEW TEMPORARY TIME UNICODE)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_OPTIONAL ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_SPAN ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_HAZARD_POINTER ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

foreach(_lib IN ITEMS holder_module retiring_module)
    add_library(hp_${_lib} SHARED ${_lib}.cpp)
    target_include_directories(hp_${_lib} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(hp_${_lib} PRIVATE lumex::hazard_pointer)
    target_compile_definitions(hp_${_lib} PRIVATE HP_FIXTURE_BUILDING_${_lib})
endforeach()

add_executable(hazard_pointer_two_modules_consumer main.cpp)
target_include_directories(hazard_pointer_two_modules_consumer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(hazard_pointer_two_modules_consumer PRIVATE
    hp_holder_module hp_retiring_module lumex::hazard_pointer)
