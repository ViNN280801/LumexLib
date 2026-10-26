# lumex/tests/core/framing/stage/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexFramingStageCxx<std>Tests runs LumexFramingStages.cxx11.tests.cpp (each
# stage alone) and LumexFramingPipeline.cxx11.tests.cpp (the pipeline of a
# text device and the pipeline mechanics).
lumex_add_standard_suites(FramingStage
    MODULE framing
    LINK lumex::framing
    SOURCES ../LumexFramingTestSupport.hpp
)
