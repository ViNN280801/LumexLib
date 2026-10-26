# lumex/tests/core/framing/length/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexFramingLengthCxx<std>Tests runs LumexLengthPrefixCodec.cxx11.tests.cpp.
lumex_add_standard_suites(FramingLength
    MODULE framing
    LINK lumex::framing
    SOURCES ../LumexFramingTestSupport.hpp
)
