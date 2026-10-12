# lumex/tests/applied/json/diagnostics/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexJsonDiagnosticsCxx11Tests builds LumexJsonDiagnostics.cxx11.tests.cpp,
# and the Cxx17 and Cxx20 suites run the same file: consumers such as
# PeakExpertWeb compile these headers at C++20.
lumex_add_standard_suites(JsonDiagnostics
    MODULE json
    LINK lumex::json nlohmann_json::nlohmann_json
)
