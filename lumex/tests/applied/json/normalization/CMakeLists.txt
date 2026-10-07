# lumex/tests/applied/json/normalization/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexJsonNormalizationCxx11Tests builds
# LumexJsonSchemaNormalizer.cxx11.tests.cpp, and the Cxx17 and Cxx20 suites
# run the same file. LumexJsonSchemaTestFixtures.hpp of the module's
# directory holds the schema and the documents shared with schema/ and
# validation/.
lumex_add_standard_suites(JsonNormalization
    MODULE json
    LINK lumex::json nlohmann_json::nlohmann_json
    SOURCES ../LumexJsonSchemaTestFixtures.hpp
)
