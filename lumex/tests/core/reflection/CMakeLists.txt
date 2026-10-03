# lumex/tests/core/reflection/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexReflectionCxx11Tests  *.cxx11.tests.cpp: LUMEX_VARINFO, the
#                              reflected enums and the field count
#   LumexReflectionCxx14Tests  + LumexFieldReflection.cxx14.tests.cpp: indexed
#                              get (friend-auto loophole)
#   LumexReflectionCxx17Tests  the same files at C++17 (get through
#                              structured bindings)
#   LumexReflectionCxx20Tests  + LumexFieldReflection.cxx20.tests.cpp: names
#                              and to_json (pointer NTTP pretty names)
# LUMEX_DEFINE_REFLECTED_ENUM once failed to compile at C++20 (`constinit`
# companions) while the C++11 suite stayed green; every standard runs it.
#
# The LumexFieldReflection*.tests.cpp files need LUMEX_WITH_FIELD_REFLECTION
# and the in-tree nlohmann_json::nlohmann_json target from
# LumexNlohmannJson.cmake. The macro is defined on those sources only, so
# LumexVarInfo and LumexReflectedEnum still include the umbrella without
# field reflection, as a consumer without nlohmann does. Without the option
# or the target the field-reflection files are left out.
file(GLOB _field_reflection_sources RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/LumexFieldReflection.cxx*.tests.cpp")
set(_reflection_link lumex::reflection)
set(_reflection_exclude "")
if(LUMEX_WITH_FIELD_REFLECTION AND TARGET nlohmann_json::nlohmann_json)
    list(APPEND _reflection_link nlohmann_json::nlohmann_json)
    set_property(SOURCE ${_field_reflection_sources}
        APPEND PROPERTY COMPILE_DEFINITIONS LUMEX_WITH_FIELD_REFLECTION)
else()
    set(_reflection_exclude ${_field_reflection_sources})
    message(STATUS "Skipping the field-reflection tests of LumexReflection*Tests: requires LUMEX_WITH_FIELD_REFLECTION=ON and an available nlohmann_json::nlohmann_json target")
endif()

lumex_add_standard_suites(Reflection
    MODULE reflection
    LINK ${_reflection_link}
    SOURCES LumexFieldReflectionTestFixtures.hpp
    EXCLUDE ${_reflection_exclude}
)
