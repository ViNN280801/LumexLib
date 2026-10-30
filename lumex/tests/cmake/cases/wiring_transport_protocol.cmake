# The dialogue engine of the transport (protocol/: frame_link, dialogue_engine)
# follows the wiring of the framed adapters: the transport's CMakeLists
# compiles lumex/applied/transport/protocol only when both the protocol and
# the framing modules are effective, links lumex::protocol and defines
# LUMEX_TRANSPORT_WITH_PROTOCOL only then, and leaves its headers out of the
# install otherwise (the vocabulary of protocol/dsl stays with the protocol
# module alone); the umbrella header includes the dialogue umbrella only under
# the macro; the tests of the engine are added only with both modules; and
# the conan component names the protocol module and the macro.

file(READ "${LUMEX_SOURCE_DIR}/lumex/applied/transport/CMakeLists.txt" _cmake)

if(NOT _cmake MATCHES "lumex_effective_on\\(_transport_with_protocol LUMEX_BUILD_PROTOCOL\\)")
    message(FATAL_ERROR "the transport does not ask whether the protocol module is effective")
endif()
if(NOT _cmake MATCHES "if\\(_transport_with_protocol AND _transport_with_framing\\)[ \t\r\n]*set\\(_transport_with_dialogue TRUE\\)")
    message(FATAL_ERROR "the transport does not build its dialogue engine with the protocol and the framing modules only")
endif()
if(NOT _cmake MATCHES "if\\(NOT _transport_with_dialogue\\)[ \t\r\n]*list\\(FILTER SOURCES EXCLUDE REGEX \"/protocol/\"\\)")
    message(FATAL_ERROR "the transport does not leave protocol/ out of its sources without the engine")
endif()
if(NOT _cmake MATCHES "if\\(_transport_with_dialogue\\)[ \t\r\n]*target_link_libraries\\(\\$\\{LUMEX_TRANSPORT_NAME\\} PUBLIC lumex::protocol\\)[ \t\r\n]*target_compile_definitions\\(\\$\\{LUMEX_TRANSPORT_NAME\\} PUBLIC[ \t\r\n]*LUMEX_TRANSPORT_WITH_PROTOCOL=1\\)")
    message(FATAL_ERROR "the transport links lumex::protocol and defines LUMEX_TRANSPORT_WITH_PROTOCOL outside if(_transport_with_dialogue)")
endif()
if(NOT _cmake MATCHES "elseif\\(NOT _transport_with_dialogue\\)[ \t\r\n]*list\\(APPEND _transport_install_excludes[ \t\r\n]*PATTERN \"link\" EXCLUDE[ \t\r\n]*PATTERN \"dialogue\" EXCLUDE[ \t\r\n]*PATTERN \"LumexDialogueTypes.hpp\" EXCLUDE[ \t\r\n]*PATTERN \"LumexTransportProtocol.hpp\" EXCLUDE\\)")
    message(FATAL_ERROR "the install does not leave the engine's headers out without the framing module")
endif()
string(REGEX MATCHALL "lumex::protocol" _mentions "${_cmake}")
list(LENGTH _mentions _mention_count)
if(NOT _mention_count EQUAL 1)
    message(FATAL_ERROR "lumex::protocol is named ${_mention_count} times in the transport's CMakeLists, expected only the conditional link")
endif()

file(READ "${LUMEX_SOURCE_DIR}/lumex/applied/transport/LumexTransport" _umbrella)
if(NOT _umbrella MATCHES "#if defined\\(LUMEX_TRANSPORT_WITH_PROTOCOL\\)[ \t\r\n]*#include \"protocol/LumexTransportProtocol.hpp\"[ \t\r\n]*#endif")
    message(FATAL_ERROR "the umbrella does not include the dialogue umbrella under LUMEX_TRANSPORT_WITH_PROTOCOL")
endif()
string(REGEX MATCHALL "#include \"protocol/" _protocol_includes "${_umbrella}")
list(LENGTH _protocol_includes _protocol_include_count)
if(NOT _protocol_include_count EQUAL 1)
    message(FATAL_ERROR "the umbrella has ${_protocol_include_count} protocol includes, expected the one under the macro")
endif()

file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/applied/transport/protocol/CMakeLists.txt" _tests)
if(NOT _tests MATCHES "lumex_effective_on\\(_transport_tests_with_dialogue LUMEX_BUILD_FRAMING\\)[ \t\r\n]*if\\(_transport_tests_with_dialogue\\)[ \t\r\n]*add_subdirectory\\(dialogue\\)[ \t\r\n]*add_subdirectory\\(link\\)")
    message(FATAL_ERROR "the tests of the engine are not added only with the framing module")
endif()

file(READ "${LUMEX_SOURCE_DIR}/conanfile.py" _conan)
if(NOT _conan MATCHES "\"LUMEX_TRANSPORT_WITH_PROTOCOL=1\"")
    message(FATAL_ERROR "the conan component does not define LUMEX_TRANSPORT_WITH_PROTOCOL")
endif()
if(NOT _conan MATCHES "\"applied_transport\", \"transport\", \\[\"LumexApplied_transport\"\\],[ \t\r\n]*\\[[^]]*\"core_protocol\"")
    message(FATAL_ERROR "the conan component of the transport does not require core_protocol")
endif()

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _options)
if(NOT _options MATCHES "LUMEX_TRANSPORT_WITH_PROTOCOL=1")
    message(FATAL_ERROR "cmake/LumexOptions.cmake does not say how the dialogue engine is wired")
endif()

file(GLOB_RECURSE _sources "${LUMEX_SOURCE_DIR}/lumex/applied/transport/protocol/*.cpp")
if(NOT _sources)
    message(FATAL_ERROR "the dialogue engine has no compiled source")
endif()
