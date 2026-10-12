# lumex/tests/core/reflection/CMakeLists.txt

# The tests follow the source tree (lumex/core/reflection): reflected_enum/
# and var_info/ include the umbrella without LUMEX_WITH_FIELD_REFLECTION, as a
# consumer without nlohmann does, and link lumex::reflection alone.
# LUMEX_DEFINE_REFLECTED_ENUM once failed to compile at C++20 (`constinit`
# companions) while the C++11 suite stayed green; every standard runs it.
add_subdirectory(reflected_enum)
add_subdirectory(var_info)

# field_reflection/ needs LUMEX_WITH_FIELD_REFLECTION and the in-tree
# nlohmann_json::nlohmann_json target from LumexNlohmannJson.cmake: its suites
# define the macro and link nlohmann themselves, in the directory of the
# field reflection tests only. Without the option or the target the directory
# is left out.
if(LUMEX_WITH_FIELD_REFLECTION AND TARGET nlohmann_json::nlohmann_json)
    add_subdirectory(field_reflection)
else()
    message(STATUS "Skipping the field-reflection tests of LumexReflectionFieldReflection*Tests: requires LUMEX_WITH_FIELD_REFLECTION=ON and an available nlohmann_json::nlohmann_json target")
endif()
