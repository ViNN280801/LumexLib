# lumex/tests/core/reflection/field_reflection/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexReflectionFieldReflectionCxx11Tests  *.cxx11.tests.cpp: the field
#                                             count
#   LumexReflectionFieldReflectionCxx14Tests  + LumexFieldReflection.cxx14 and
#                                             LumexFieldReflectionGetFirst.cxx14:
#                                             indexed get (friend-auto
#                                             loophole)
#   LumexReflectionFieldReflectionCxx17Tests  the same files at C++17 (get
#                                             through structured bindings)
#   LumexReflectionFieldReflectionCxx20Tests  + LumexFieldReflection.cxx20:
#                                             names and to_json (pointer NTTP
#                                             pretty names)
# Every source of this directory tests field reflection, so the suites define
# LUMEX_WITH_FIELD_REFLECTION (the module's CMakeLists.txt adds this directory
# only when the option and the in-tree nlohmann_json::nlohmann_json target are
# there) and link nlohmann. LumexFieldReflectionTestFixtures.hpp holds the
# aggregates of the tests.
lumex_add_standard_suites(ReflectionFieldReflection
    MODULE reflection
    LINK lumex::reflection nlohmann_json::nlohmann_json
    DEFINITIONS LUMEX_WITH_FIELD_REFLECTION
    SOURCES LumexFieldReflectionTestFixtures.hpp
)
