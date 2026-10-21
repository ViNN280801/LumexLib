# lumex/tests/core/framing/modbus_rtu/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexFramingModbusRtuCxx<std>Tests runs LumexModbusRtuCodec.cxx11.tests.cpp.
lumex_add_standard_suites(FramingModbusRtu
    MODULE framing
    LINK lumex::framing
    SOURCES ../LumexFramingTestSupport.hpp
)
