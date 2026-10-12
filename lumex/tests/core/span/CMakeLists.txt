# lumex/tests/core/span/CMakeLists.txt
#
# span is a header-only C++11 implementation of std::span with the later
# additions (const_iterator, at). The tests
# follow the source tree (lumex/core/span):
#   view/       the class, its traits and the byte views (LumexSpan*)
#   this one    LumexSpanGlobalNames.cxx11.tests.cpp, the test of the module
#               umbrella: the module declares nothing at global scope
# Every directory builds one suite per standard of the module
# (lumex/tests/LumexTestStandards.cmake), each from the files of its standard
# and the lower ones: LumexSpanCxx11Tests, LumexSpanCxx17Tests (deduction
# guides, std::byte) and LumexSpanCxx20Tests (std::span interoperability,
# ranges concepts, the differential tests against std::span), and the same
# three named LumexSpanView... in view/. CTest names carry the prefix of the
# directory ("span.", "span.view.") and the suffix of the suite (.cxx11, ...).
lumex_add_standard_suites(Span
    MODULE span
    LINK lumex::span
    VARIANT checks_on DEFINITIONS LUMEX_SPAN_CHECKS=1 LUMEX_TEST_SPAN_EXPECT_CHECKS=1
    VARIANT checks_off DEFINITIONS LUMEX_SPAN_CHECKS=0 LUMEX_TEST_SPAN_EXPECT_CHECKS=0
)

add_subdirectory(view)
