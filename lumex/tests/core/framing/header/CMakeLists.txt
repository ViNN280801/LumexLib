# lumex/tests/core/framing/header/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexFramingHeaderCxx<std>Tests runs LumexHeaderCodec.cxx11.tests.cpp.
lumex_add_standard_suites(FramingHeader
    MODULE framing
    LINK lumex::framing
    SOURCES ../LumexFramingTestSupport.hpp
)
