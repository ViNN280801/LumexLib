# lumex/tests/core/framing/slip/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexFramingSlipCxx<std>Tests runs LumexSlipCodec.cxx11.tests.cpp.
lumex_add_standard_suites(FramingSlip
    MODULE framing
    LINK lumex::framing
    SOURCES ../LumexFramingTestSupport.hpp
)
