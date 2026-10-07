# lumex/tests/applied/serial/port/CMakeLists.txt

# serial has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexSerialPortCxx11Tests builds every *.cxx11.tests.cpp of this directory.
# _serial_registration comes from the CMakeLists.txt of the module.
lumex_add_standard_suites(SerialPort
    MODULE serial
    LINK
        lumex::serial
    ${_serial_registration}
)