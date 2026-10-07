# lumex/tests/applied/serial/CMakeLists.txt

# The tests follow the source tree (lumex/applied/serial): enumeration/, port/,
# probe/ and probe/detail/; resolver/ has no tests. serial has one standard in
# lumex/tests/LumexTestStandards.cmake, C++11.
#
# Discovery runs the executable at ctest time (DISCOVERY_MODE PRE_TEST, set by
# lumex_gtest_discover_tests), never from the link step: a POST_BUILD
# discovery of this suite once left cmake.exe blocked while ninja still held
# the tree (CreateFile on a Bluetooth COM port in an older probe). CMake
# before 3.18 has no DISCOVERY_MODE, so there the executable is registered as
# one CTest test (PLAIN_EXECUTABLE) and is not run at build time either. The
# directories below use _serial_registration.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
    set(_serial_registration DISCOVER_ARGS DISCOVERY_TIMEOUT 30)
else()
    set(_serial_registration PLAIN_EXECUTABLE)
endif()

add_subdirectory(enumeration)
add_subdirectory(port)
add_subdirectory(probe)
