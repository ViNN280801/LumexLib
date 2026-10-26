# lumex/tests/core/framing/cobs/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexFramingCobsCxx<std>Tests runs LumexCobsCodec.cxx11.tests.cpp.
lumex_add_standard_suites(FramingCobs
    MODULE framing
    LINK lumex::framing
    SOURCES ../LumexFramingTestSupport.hpp
)
