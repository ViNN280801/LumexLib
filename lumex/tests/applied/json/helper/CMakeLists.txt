# lumex/tests/applied/json/helper/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexJsonHelperCxx11Tests  LumexJsonHelper.cxx11.tests.cpp and
#                              LumexJsonHelperStringView.cxx11.tests.cpp
#                              (is_empty_value on lumex_string_view)
#   LumexJsonHelperCxx17Tests  + LumexJsonHelper.cxx17.tests.cpp
#                              (std::string_view)
#   LumexJsonHelperCxx20Tests  the same files: consumers such as PeakExpertWeb
#                              compile these headers at C++20
# LumexJsonHelperTestFixture.hpp holds the fixture of the helper tests; the
# files it writes carry the standard of the suite in their names, so the
# suites can run in parallel.
lumex_add_standard_suites(JsonHelper
    MODULE json
    LINK lumex::json nlohmann_json::nlohmann_json
    SOURCES LumexJsonHelperTestFixture.hpp
)
