# LUMEX_WITH_FIELD_REFLECTION must not leak to consumers of lumex::reflection.
#
# The umbrella lumex/core/reflection/LumexReflection includes
# LumexFieldReflection.hpp (and therefore <nlohmann/json.hpp>) whenever the
# macro is defined, while lumex::reflection deliberately carries no nlohmann
# include directory. An INTERFACE definition therefore broke every umbrella
# consumer without its own nlohmann (the reflection tests, the reflection
# examples) as soon as the option was ON. The field-reflection test suites
# opt in themselves: a definition on those suites plus the in-tree nlohmann
# target.

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

# The field reflection tests live in a directory of their own (they follow the
# source tree) and opt in themselves: the suites of that directory define the
# macro and link the in-tree nlohmann target. The directory is added only when
# the option and the target are there.
file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/core/reflection/field_reflection/CMakeLists.txt"
    _tests)
string(REGEX MATCH
    "(^|[^A-Z_])DEFINITIONS[ \t\r\n]+LUMEX_WITH_FIELD_REFLECTION"
    _opt_in "${_tests}")
if(NOT _opt_in)
    message(FATAL_ERROR
        "the field-reflection test suites must define LUMEX_WITH_FIELD_REFLECTION "
        "themselves (DEFINITIONS LUMEX_WITH_FIELD_REFLECTION)")
endif()
string(FIND "${_tests}" "nlohmann_json::nlohmann_json" _links_nlohmann)
if(_links_nlohmann EQUAL -1)
    message(FATAL_ERROR
        "field-reflection test suites must link nlohmann_json::nlohmann_json")
endif()

# The VarInfo and ReflectedEnum tests must include the umbrella without the
# macro, as a consumer without nlohmann does: their directories name neither the
# macro nor nlohmann, and the module's directory adds the field reflection
# directory under the option and the target only. The reflection examples must
# build from lumex::reflection alone, without nlohmann.
foreach(_directory reflected_enum var_info)
    file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/core/reflection/${_directory}/CMakeLists.txt"
        _plain)
    string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" _plain_code "${_plain}")
    foreach(_forbidden LUMEX_WITH_FIELD_REFLECTION nlohmann)
        string(FIND "${_plain_code}" "${_forbidden}" _found)
        if(NOT _found EQUAL -1)
            message(FATAL_ERROR
                "lumex/tests/core/reflection/${_directory}/CMakeLists.txt "
                "mentions ${_forbidden}; the tests of that directory include "
                "the umbrella without field reflection")
        endif()
    endforeach()
endforeach()
file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/core/reflection/CMakeLists.txt" _root)
string(FIND "${_root}"
    "if(LUMEX_WITH_FIELD_REFLECTION AND TARGET nlohmann_json::nlohmann_json)"
    _gated)
string(FIND "${_root}" "add_subdirectory(field_reflection)" _added)
if(_gated EQUAL -1 OR _added EQUAL -1 OR _added LESS _gated)
    message(FATAL_ERROR
        "the field_reflection test directory must be added under "
        "if(LUMEX_WITH_FIELD_REFLECTION AND TARGET nlohmann_json::nlohmann_json)")
endif()
file(READ "${LUMEX_SOURCE_DIR}/lumex/examples/reflection/CMakeLists.txt"
    _examples)
# Only the field reflection example may link nlohmann, behind the option (or,
# standalone, behind a successful find_package): the other reflection examples
# must build from lumex::reflection alone, so every mention of nlohmann stands
# after the first mention of LUMEX_WITH_FIELD_REFLECTION, and the two plain
# examples are declared before it.
string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" _examples_code "${_examples}")
string(FIND "${_examples_code}" "nlohmann" _examples_nlohmann)
string(FIND "${_examples_code}" "LUMEX_WITH_FIELD_REFLECTION" _examples_gate)
string(FIND "${_examples_code}" "LumexReflectionExampleWorkflow" _examples_plain)
if(NOT _examples_nlohmann EQUAL -1)
    if(_examples_gate EQUAL -1 OR _examples_nlohmann LESS _examples_gate)
        message(FATAL_ERROR
            "reflection examples link nlohmann outside the field reflection "
            "example; the other examples must build from lumex::reflection "
            "alone")
    endif()
    if(_examples_plain EQUAL -1 OR _examples_plain GREATER _examples_gate)
        message(FATAL_ERROR
            "the plain reflection examples must be declared before the "
            "gate of the field reflection example")
    endif()
endif()
