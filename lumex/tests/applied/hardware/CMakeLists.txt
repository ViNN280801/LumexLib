# lumex/tests/applied/hardware/CMakeLists.txt

# hardware has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexHardwareCapabilitiesCxx11Tests builds every *.cxx11.tests.cpp of this
# directory.
lumex_add_standard_suites(HardwareCapabilities
    MODULE hardware
    LINK
        lumex::hardware
)
