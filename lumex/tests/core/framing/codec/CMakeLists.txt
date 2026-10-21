# lumex/tests/core/framing/codec/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexFramingCodecCxx<std>Tests runs LumexFramingErrc.cxx11.tests.cpp (the
# error codes and their category) and LumexFramingCodec.cxx11.tests.cpp (the
# sink forms and the bounded frame buffer) at every standard.
lumex_add_standard_suites(FramingCodec
    MODULE framing
    LINK lumex::framing
)
