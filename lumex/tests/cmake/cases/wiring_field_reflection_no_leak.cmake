# LUMEX_WITH_FIELD_REFLECTION must not leak to consumers of lumex::reflection.
#
# The umbrella lumex/core/reflection/LumexReflection includes
# LumexFieldReflection.hpp (and therefore <nlohmann/json.hpp>) whenever the
# macro is defined, while lumex::reflection deliberately carries no nlohmann
# include directory. An INTERFACE definition therefore broke every umbrella
# consumer without its own nlohmann (LumexReflectionTests, the reflection
# examples) as soon as the option was ON. The field-reflection suites opt in
# themselves: PRIVATE definition plus the in-tree nlohmann target.

file(READ "${LUMEX_SOURCE_DIR}/lumex/core/reflection/CMakeLists.txt" _module)
string(REGEX MATCH
    "target_compile_definitions[ \t\r\n]*\\([^)]*INTERFACE[^)]*LUMEX_WITH_FIELD_REFLECTION"
    _leak "${_module}")
if(_leak)
    message(FATAL_ERROR
        "lumex/core/reflection/CMakeLists.txt exports LUMEX_WITH_FIELD_REFLECTION "
        "as an INTERFACE definition; umbrella consumers without nlohmann break")
endif()
string(REGEX MATCH
    "target_compile_definitions[ \t\r\n]*\\([^)]*PUBLIC[^)]*LUMEX_WITH_FIELD_REFLECTION"
    _leak_public "${_module}")
if(_leak_public)
    message(FATAL_ERROR
        "lumex/core/reflection/CMakeLists.txt exports LUMEX_WITH_FIELD_REFLECTION "
        "as a PUBLIC definition; umbrella consumers without nlohmann break")
endif()
string(REGEX MATCH
    "target_link_libraries[ \t\r\n]*\\([^)]*INTERFACE[^)]*nlohmann"
    _nlohmann_leak "${_module}")
if(_nlohmann_leak)
    message(FATAL_ERROR
        "lumex/core/reflection/CMakeLists.txt links nlohmann INTERFACE; it would "
        "shadow the consumer's own nlohmann/json.hpp")
endif()

file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/core/reflection/CMakeLists.txt"
    _tests)
string(REGEX MATCH
    "target_compile_definitions[ \t\r\n]*\\([^)]*PRIVATE[^)]*LUMEX_WITH_FIELD_REFLECTION"
    _opt_in "${_tests}")
if(NOT _opt_in)
    message(FATAL_ERROR
        "field-reflection test suites must define LUMEX_WITH_FIELD_REFLECTION "
        "PRIVATE themselves")
endif()
string(FIND "${_tests}" "nlohmann_json::nlohmann_json" _links_nlohmann)
if(_links_nlohmann EQUAL -1)
    message(FATAL_ERROR
        "field-reflection test suites must link nlohmann_json::nlohmann_json")
endif()

# The plain reflection suite and the reflection examples must build from
# lumex::reflection alone, without nlohmann.
string(REGEX MATCH
    "add_executable[ \t\r\n]*\\([ \t\r\n]*\\\${TEST_EXECUTABLE_NAME}[^)]*\\)[^#]*target_link_libraries[ \t\r\n]*\\([ \t\r\n]*\\\${TEST_EXECUTABLE_NAME}[^)]*nlohmann"
    _plain_links_nlohmann "${_tests}")
if(_plain_links_nlohmann)
    message(FATAL_ERROR
        "LumexReflectionTests links nlohmann; it must prove the umbrella builds "
        "without it")
endif()
file(READ "${LUMEX_SOURCE_DIR}/lumex/examples/reflection/CMakeLists.txt"
    _examples)
string(FIND "${_examples}" "nlohmann" _examples_nlohmann)
if(NOT _examples_nlohmann EQUAL -1)
    message(FATAL_ERROR
        "reflection examples link nlohmann; they must build from "
        "lumex::reflection alone")
endif()
