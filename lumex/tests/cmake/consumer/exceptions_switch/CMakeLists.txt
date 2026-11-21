# Fixture of cmake.consumer_exceptions_switch_*.
#
# The one switch of the library, LUMEX_HAS_EXCEPTIONS (core/utility/macros/
# LumexKeywords.hpp), and the macro of each module that defaults to it
# (expected, optional, span, string_view; smart_ptr follows the compiler and is
# not changed by it). Exceptions stay on in every case; one executable per mode:
#   -DSWITCH_MODE=default            nothing defined: everything throws
#   -DSWITCH_MODE=shared_zero        LUMEX_HAS_EXCEPTIONS=0: every module
#                                    terminates, smart_ptr keeps its try blocks
#   -DSWITCH_MODE=override_expected  LUMEX_HAS_EXCEPTIONS=0 and
#                                    LUMEX_EXPECTED_HAS_EXCEPTIONS=1: only
#                                    expected still throws
#   -DSWITCH_MODE=override_optional  LUMEX_HAS_EXCEPTIONS=0 and
#                                    LUMEX_OPTIONAL_HAS_EXCEPTIONS=1: only
#                                    optional still throws
#   -DSWITCH_MODE=override_span      LUMEX_SPAN_HAS_EXCEPTIONS=0 alone: only span
#                                    terminates
#   -DSWITCH_MODE=override_string_view  LUMEX_HAS_EXCEPTIONS=0 and
#                                    LUMEX_STRING_VIEW_HAS_EXCEPTIONS=1: only the
#                                    string view still throws
# main.cpp checks the values of the macros and the behavior of one throwing
# function of each module (a child process for those that terminate), and, in
# every mode, that the try blocks that follow the compiler and not the switch
# are kept: the stream inserter of the string view sets the badbit of a stream
# whose buffer throws, and a shared_ptr whose control block cannot be allocated
# calls its deleter. The refusal of the value 1 without exceptions is the
# exceptions_switch_compile_checks fixture.

cmake_minimum_required(VERSION 3.16)
project(LumexExceptionsSwitch LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT SWITCH_MODE)
    set(SWITCH_MODE default)
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS FILESYSTEM
        FMT FRAMING PROTOCOL UNITS GENERATORS INTERPROCESS REFLECTION STRING
        TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(_opt EXPECTED MATH OPTIONAL SMART_PTR SPAN STRING_VIEW UTILITY)
    set(LUMEX_BUILD_${_opt} ON CACHE BOOL "" FORCE)
endforeach()

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

add_executable(exceptions_switch_consumer main.cpp)
target_link_libraries(exceptions_switch_consumer PRIVATE
    lumex::expected lumex::optional lumex::smart_ptr lumex::span
    lumex::string_view)
set_target_properties(exceptions_switch_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(SWITCH_MODE STREQUAL "default")
    target_compile_definitions(exceptions_switch_consumer PRIVATE SWITCH_DEFAULT=1)
elseif(SWITCH_MODE STREQUAL "shared_zero")
    target_compile_definitions(exceptions_switch_consumer PRIVATE
        SWITCH_SHARED_ZERO=1 LUMEX_HAS_EXCEPTIONS=0)
elseif(SWITCH_MODE STREQUAL "override_expected")
    target_compile_definitions(exceptions_switch_consumer PRIVATE
        SWITCH_OVERRIDE_EXPECTED=1 LUMEX_HAS_EXCEPTIONS=0
        LUMEX_EXPECTED_HAS_EXCEPTIONS=1)
elseif(SWITCH_MODE STREQUAL "override_optional")
    target_compile_definitions(exceptions_switch_consumer PRIVATE
        SWITCH_OVERRIDE_OPTIONAL=1 LUMEX_HAS_EXCEPTIONS=0
        LUMEX_OPTIONAL_HAS_EXCEPTIONS=1)
elseif(SWITCH_MODE STREQUAL "override_span")
    target_compile_definitions(exceptions_switch_consumer PRIVATE
        SWITCH_OVERRIDE_SPAN=1 LUMEX_SPAN_HAS_EXCEPTIONS=0)
elseif(SWITCH_MODE STREQUAL "override_string_view")
    target_compile_definitions(exceptions_switch_consumer PRIVATE
        SWITCH_OVERRIDE_STRING_VIEW=1 LUMEX_HAS_EXCEPTIONS=0
        LUMEX_STRING_VIEW_HAS_EXCEPTIONS=1)
else()
    message(FATAL_ERROR "SWITCH_MODE: default, shared_zero, override_expected, override_optional, override_span or override_string_view")
endif()
