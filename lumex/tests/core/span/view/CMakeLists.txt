# lumex/tests/core/span/view/CMakeLists.txt
#
# The tests of lumex/core/span/view: span, the byte views, dynamic_extent,
# byte and the traits. One suite per standard of the module span in
# lumex/tests/LumexTestStandards.cmake (11, 17, 20); each builds the
# *.cxxNN.tests.cpp files of its standard and the lower ones.
lumex_add_standard_suites(SpanView
    MODULE span
    LINK lumex::span
    SOURCES LumexSpanTestSupport.hpp
)
