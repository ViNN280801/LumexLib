# lumex/tests/applied/json/CMakeLists.txt
#
# lumex::json is header-only and compiles against the consumer's nlohmann;
# the tests use the vendored 3rdparty/nlohmann copy. The tests follow the
# source tree (lumex/applied/json), one directory per directory of the module:
# diagnostics, helper, normalization, schema and validation. Each has one
# suite per standard of the module (lumex/tests/LumexTestStandards.cmake).
# LumexJsonSchemaTestFixtures.hpp holds the schema and the documents the
# schema, validation and normalization tests share.

lumex_setup_nlohmann_json()

add_subdirectory(diagnostics)
add_subdirectory(helper)
add_subdirectory(normalization)
add_subdirectory(schema)
add_subdirectory(validation)
