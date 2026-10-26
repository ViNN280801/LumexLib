# lumex/tests/core/framing/delimiter/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexFramingDelimiterCxx<std>Tests runs LumexDelimiterCodec.cxx11.tests.cpp.
lumex_add_standard_suites(FramingDelimiter
    MODULE framing
    LINK lumex::framing
    SOURCES ../LumexFramingTestSupport.hpp
)
