# lumex/tests/applied/serial/CMakeLists.txt

# serial has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexSerialPortCxx11Tests builds every *.cxx11.tests.cpp of this directory.
#
# Discovery runs the executable at ctest time (DISCOVERY_MODE PRE_TEST, set by
# lumex_gtest_discover_tests), never from the link step: a POST_BUILD
# discovery of this suite once left cmake.exe blocked while ninja still held
# the tree (CreateFile on a Bluetooth COM port in an older probe). CMake
# before 3.18 has no DISCOVERY_MODE, so there the executable is registered as
# one CTest test (PLAIN_EXECUTABLE) and is not run at build time either.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
    set(_serial_registration DISCOVER_ARGS DISCOVERY_TIMEOUT 30)
else()
    set(_serial_registration PLAIN_EXECUTABLE)
endif()

lumex_add_standard_suites(SerialPort
    MODULE serial
    LINK
        lumex::serial
    ${_serial_registration}
)
