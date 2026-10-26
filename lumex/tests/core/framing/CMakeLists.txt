# lumex/tests/core/framing/CMakeLists.txt

# The tests follow the source tree (lumex/core/framing): one directory per codec
# directory, codec/ for the error codes and the shared helpers. This directory
# keeps the tests of the umbrella: LumexFraming.cxx11.tests.cpp, the properties
# every codec keeps (LumexFramingCxx<std>Tests). Each directory has one suite
# per standard of the module (lumex/tests/LumexTestStandards.cmake).
lumex_add_standard_suites(Framing
    MODULE framing
    LINK lumex::framing
    SOURCES LumexFramingTestSupport.hpp
)

add_subdirectory(cobs)
add_subdirectory(codec)
add_subdirectory(delimiter)
add_subdirectory(header)
add_subdirectory(length)
add_subdirectory(modbus_rtu)
add_subdirectory(slip)
add_subdirectory(stage)
