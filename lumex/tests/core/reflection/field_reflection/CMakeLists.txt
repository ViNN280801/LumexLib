# lumex/tests/core/reflection/field_reflection/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexReflectionFieldReflectionCxx11Tests  *.cxx11.tests.cpp: the field
#                                             count, and the names, indexed get
#                                             and to_json of aggregates
#                                             registered with
#                                             LUMEX_DEFINE_FIELD_NAMES
#                                             (LumexFieldNames*)
#   LumexReflectionFieldReflectionCxx14Tests  + LumexFieldReflection.cxx14 and
#                                             LumexFieldReflectionGetFirst.cxx14:
#                                             indexed get of an unregistered
#                                             aggregate (friend-auto loophole);
#                                             LumexFieldNames*.cxx14:
#                                             tuple_size_v, registered and
#                                             unregistered twins
#   LumexReflectionFieldReflectionCxx17Tests  the same files at C++17 (get
#                                             through structured bindings);
#                                             + LumexFieldNamesStdOptional.cxx17:
#                                             std::optional members
#   LumexReflectionFieldReflectionCxx20Tests  + LumexFieldReflection.cxx20:
#                                             names and to_json of an
#                                             unregistered aggregate (pointer
#                                             NTTP pretty names);
#                                             LumexFieldNamesAgreement.cxx20:
#                                             the registration wins over, and
#                                             agrees with, those names
# Every source of this directory tests field reflection, so the suites define
# LUMEX_WITH_FIELD_REFLECTION (the module's CMakeLists.txt adds this directory
# only when the option and the in-tree nlohmann_json::nlohmann_json target are
# there) and link nlohmann. LumexFieldReflectionTestFixtures.hpp holds the
# aggregates of the tests of the automatic path,
# LumexFieldReflectionRegisteredFixtures.hpp the registered ones. The
# negative compile checks of the registration (a missing or wrong one) are in
# lumex/tests/cmake/consumer/field_names_compile_checks.
lumex_add_standard_suites(ReflectionFieldReflection
    MODULE reflection
    LINK lumex::reflection nlohmann_json::nlohmann_json
    DEFINITIONS LUMEX_WITH_FIELD_REFLECTION
    SOURCES LumexFieldReflectionTestFixtures.hpp
            LumexFieldReflectionRegisteredFixtures.hpp
)
