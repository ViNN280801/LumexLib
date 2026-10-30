# The framed adapters of the transport follow the logger adapter's wiring: the
# transport's CMakeLists compiles lumex/applied/transport/framed only when the
# framing module is effective, links lumex::framing and defines
# LUMEX_TRANSPORT_WITH_FRAMING only then, leaves the headers out of the
# install otherwise; the umbrella header includes them only under the macro;
# the tests of the adapters are added only with the framing module; and the
# conan component names the framing module and the macro.

file(READ "${LUMEX_SOURCE_DIR}/lumex/applied/transport/CMakeLists.txt" _cmake)

if(NOT _cmake MATCHES "lumex_effective_on\\(_transport_with_framing LUMEX_BUILD_FRAMING\\)")
    message(FATAL_ERROR "the transport does not ask whether the framing module is effective")
endif()
if(NOT _cmake MATCHES "if\\(NOT _transport_with_framing\\)[ \t\r\n]*list\\(FILTER SOURCES EXCLUDE REGEX \"\\^framed/\"\\)")
    message(FATAL_ERROR "the transport does not leave framed/ out of its sources without framing")
endif()
if(NOT _cmake MATCHES "if\\(_transport_with_framing\\)[ \t\r\n]*target_link_libraries\\(\\$\\{LUMEX_TRANSPORT_NAME\\} PUBLIC lumex::framing\\)[ \t\r\n]*target_compile_definitions\\(\\$\\{LUMEX_TRANSPORT_NAME\\} PUBLIC[ \t\r\n]*LUMEX_TRANSPORT_WITH_FRAMING=1\\)")
    message(FATAL_ERROR "the transport links lumex::framing and defines LUMEX_TRANSPORT_WITH_FRAMING outside if(_transport_with_framing)")
endif()
if(NOT _cmake MATCHES "if\\(NOT _transport_with_framing\\)[ \t\r\n]*list\\(APPEND _transport_install_excludes PATTERN \"framed\" EXCLUDE\\)")
    message(FATAL_ERROR "the install does not leave the framed headers out without framing")
endif()
# No other mention of the framing module's target: no unconditional link.
string(REGEX MATCHALL "lumex::framing" _mentions "${_cmake}")
list(LENGTH _mentions _mention_count)
if(NOT _mention_count EQUAL 1)
    message(FATAL_ERROR "lumex::framing is named ${_mention_count} times in the transport's CMakeLists, expected only the conditional link")
endif()

file(READ "${LUMEX_SOURCE_DIR}/lumex/applied/transport/LumexTransport" _umbrella)
if(NOT _umbrella MATCHES "#if defined\\(LUMEX_TRANSPORT_WITH_FRAMING\\)[ \t\r\n]*#include \"framed/LumexFramedDatagram.hpp\"[ \t\r\n]*#include \"framed/LumexFramedStatistics.hpp\"[ \t\r\n]*#include \"framed/LumexFramedStream.hpp\"[ \t\r\n]*#include \"framed/LumexFramedTypes.hpp\"[ \t\r\n]*#endif")
    message(FATAL_ERROR "the umbrella does not include the framed headers under LUMEX_TRANSPORT_WITH_FRAMING")
endif()
string(REGEX MATCHALL "#include \"framed/" _framed_includes "${_umbrella}")
list(LENGTH _framed_includes _framed_include_count)
if(NOT _framed_include_count EQUAL 4)
    message(FATAL_ERROR "the umbrella has ${_framed_include_count} framed includes, expected the 4 under the macro")
endif()

file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/applied/transport/CMakeLists.txt" _tests)
if(NOT _tests MATCHES "lumex_effective_on\\(_transport_tests_with_framing LUMEX_BUILD_FRAMING\\)[ \t\r\n]*if\\(_transport_tests_with_framing\\)[ \t\r\n]*add_subdirectory\\(framed\\)")
    message(FATAL_ERROR "the tests of the framed adapters are not added only with the framing module")
endif()

file(READ "${LUMEX_SOURCE_DIR}/conanfile.py" _conan)
if(NOT _conan MATCHES "\"LUMEX_TRANSPORT_WITH_FRAMING=1\"")
    message(FATAL_ERROR "the conan component does not define LUMEX_TRANSPORT_WITH_FRAMING")
endif()
if(NOT _conan MATCHES "\"applied_transport\", \"transport\", \\[\"LumexApplied_transport\"\\],[ \t\r\n]*\\[[^]]*\"core_framing\"")
    message(FATAL_ERROR "the conan component of the transport does not require core_framing")
endif()

file(GLOB_RECURSE _sources "${LUMEX_SOURCE_DIR}/lumex/applied/transport/framed/*.cpp")
if(NOT _sources)
    message(FATAL_ERROR "the framed adapters have no compiled source")
endif()
