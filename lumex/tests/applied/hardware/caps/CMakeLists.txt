# lumex/tests/applied/hardware/caps/CMakeLists.txt

# hardware has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexHardwareCapsCxx11Tests builds every *.cxx11.tests.cpp of this
# directory.
lumex_add_standard_suites(HardwareCaps
    MODULE hardware
    LINK
        lumex::hardware
)
