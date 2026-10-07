# lumex/tests/applied/json/schema/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexJsonSchemaCxx11Tests builds LumexJsonSchema.cxx11.tests.cpp (the
# traverser and the exception), and the Cxx17 and Cxx20 suites run the same
# file: the schema walker takes its std::ranges branch at C++20.
# LumexJsonSchemaTestFixtures.hpp of the module's directory holds the schema
# and the documents shared with validation/ and normalization/.
lumex_add_standard_suites(JsonSchema
    MODULE json
    LINK lumex::json nlohmann_json::nlohmann_json
    SOURCES ../LumexJsonSchemaTestFixtures.hpp
)
