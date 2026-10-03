# lumex/tests/applied/json/CMakeLists.txt
#
# lumex::json is header-only and compiles against the consumer's nlohmann;
# the tests use the vendored 3rdparty/nlohmann copy. One suite per standard
# of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexJsonCxx11Tests  every *.cxx11.tests.cpp
#   LumexJsonCxx17Tests  + LumexJsonHelper.cxx17.tests.cpp (std::string_view)
#   LumexJsonCxx20Tests  the same sources: consumers such as PeakExpertWeb
#                        compile these headers at C++20, and the schema walker
#                        takes its std::ranges branch there
# LumexJsonHelperTestFixture.hpp holds the fixture of the helper tests; the
# files it writes carry the standard of the suite in their names, so the
# suites can run in parallel.

lumex_setup_nlohmann_json()

lumex_add_standard_suites(Json
    MODULE json
    LINK lumex::json nlohmann_json::nlohmann_json
    SOURCES LumexJsonHelperTestFixture.hpp
)
