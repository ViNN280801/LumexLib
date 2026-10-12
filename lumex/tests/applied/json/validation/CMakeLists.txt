# lumex/tests/applied/json/validation/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexJsonValidationCxx11Tests builds LumexJsonSchemaValidator.cxx11.tests.cpp,
# and the Cxx17 and Cxx20 suites run the same file.
# LumexJsonSchemaTestFixtures.hpp of the module's directory holds the schema
# and the documents shared with schema/ and normalization/.
lumex_add_standard_suites(JsonValidation
    MODULE json
    LINK lumex::json nlohmann_json::nlohmann_json
    SOURCES ../LumexJsonSchemaTestFixtures.hpp
)
