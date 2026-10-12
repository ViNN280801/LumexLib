# lumex/tests/core/fmt/CMakeLists.txt

# LumexFormat (lumex/core/fmt) has its own code path at several standards.
# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexFormatCxx11Tests  *.cxx11.tests.cpp: the floor (printf / strtod
#                          float fallback, run-time format string checks)
#   LumexFormatCxx14Tests  the C++11 files at C++14
#   LumexFormatCxx17Tests  + *.cxx17.tests.cpp: std::string_view arguments
#                          and format strings; floats through std::to_chars
#   LumexFormatCxx20Tests  + *.cxx20.tests.cpp: the consteval format string
#                          check and the cross-checks against std::format
#                          (LumexFormatStdCompare, the differential fuzzing
#                          of LumexFormatDifferential, ranges)
# LumexFormatTestHelpers.hpp holds the run-time helpers every file shares.
lumex_add_standard_suites(Format
    MODULE fmt
    LINK lumex::fmt lumex::string_view lumex::reflection
    SOURCES LumexFormatTestHelpers.hpp
)
