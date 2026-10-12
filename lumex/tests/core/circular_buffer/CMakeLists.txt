# lumex/tests/core/circular_buffer/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexCircularBufferCxx11Tests  every CircularBuffer*.cxx11.tests.cpp
#   LumexCircularBufferCxx20Tests  the same files at C++20, where
#                                  CircularBuffer.hpp takes its C++20 branches
#                                  (<=> and concepts), plus the
#                                  CircularBuffer*.cxx20.tests.cpp files
#                                  (iterator concepts, ranges, <=>)
#
# CircularBufferBoost.cxx11.tests.cpp compares the container with
# boost::circular_buffer on random operations. It needs the Boost headers
# (header-only use): pass -DLUMEX_TEST_BOOST_INCLUDE_DIR=<dir that holds
# boost/circular_buffer.hpp>, for example /opt/boost-1.92.0/include. Without
# it the file compiles to one skipped test; every other suite builds without
# Boost.
set(LUMEX_TEST_BOOST_INCLUDE_DIR "" CACHE PATH
    "Directory that holds boost/circular_buffer.hpp (headers only) for the differential circular_buffer tests; empty: skipped")

set(_lumex_cb_boost_defs "")
if(LUMEX_TEST_BOOST_INCLUDE_DIR
   AND EXISTS "${LUMEX_TEST_BOOST_INCLUDE_DIR}/boost/circular_buffer.hpp")
    set(_lumex_cb_boost_defs DEFINITIONS LUMEX_TEST_HAS_BOOST_CIRCULAR_BUFFER)
endif()

lumex_add_standard_suites(CircularBuffer
    MODULE circular_buffer
    LINK
        lumex::circular_buffer
    SOURCES
        CircularBufferTestSupport.hpp
    ${_lumex_cb_boost_defs}
    TARGETS_VAR _lumex_cb_test_targets
)

if(_lumex_cb_boost_defs)
    foreach(_target IN LISTS _lumex_cb_test_targets)
        target_include_directories(${_target} SYSTEM PRIVATE
            "${LUMEX_TEST_BOOST_INCLUDE_DIR}")
    endforeach()
endif()
