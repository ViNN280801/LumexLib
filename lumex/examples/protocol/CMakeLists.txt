# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
# The Markdown export needs only lumex::protocol. The JSON export of the
# second example also needs nlohmann/json 3.x from the consumer: standalone
# builds find it with find_package(nlohmann_json), and the example prints the
# JSON document only when the library was built with the option
# LUMEX_PROTOCOL_WITH_JSON (the macro of the same name reaches the example
# through lumex::protocol).
#
# The examples of the dialogue (client, simulator, reconnect) also need
# lumex::transport, with its protocol, framing and testing parts, and are built
# when that target exists: standalone, when the installed package has it.
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options;
# the vendored 3rdparty/nlohmann copy is used.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexProtocolExampleIndex
  SOURCE example_protocol_index.cpp
  LINK lumex::protocol
  REQUIRE_TARGET lumex::protocol
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexProtocolExampleExport
  SOURCE example_protocol_export.cpp
  LINK lumex::protocol
  REQUIRE_TARGET lumex::protocol
  CXX_STANDARD 11
)

if(COMMAND lumex_setup_nlohmann_json)
  if(LUMEX_PROTOCOL_WITH_JSON)
    lumex_setup_nlohmann_json()
    target_link_libraries(LumexProtocolExampleExport
      PRIVATE nlohmann_json::nlohmann_json)
  endif()
else()
  find_package(nlohmann_json 3 QUIET)
  if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(LumexProtocolExampleExport
      PRIVATE nlohmann_json::nlohmann_json)
  endif()
endif()

# Both examples finish at once; a hung one is a failure, not a wait.
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND LUMEX_BUILD_TESTS)
  foreach(_example LumexProtocolExampleIndex LumexProtocolExampleExport)
    lumex_example_test_name(_ctest_name ${_example})
    set_tests_properties(${_ctest_name} PROPERTIES TIMEOUT 60)
  endforeach()
endif()

# The tutorials: the fields and the layouts, and the parameter registries, on
# invented devices. They talk to nothing.
lumex_example_executable(
  NAME LumexProtocolExampleFields
  SOURCE example_protocol_fields.cpp
  LINK lumex::protocol
  REQUIRE_TARGET lumex::protocol
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
lumex_example_executable(
  NAME LumexProtocolExampleRegistry
  SOURCE example_protocol_registry.cpp
  LINK lumex::protocol
  REQUIRE_TARGET lumex::protocol
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)

# The dialogue examples talk to a simulated device through lumex::transport
# (its protocol, framing and testing parts: LUMEX_TRANSPORT_WITH_PROTOCOL and
# LUMEX_TRANSPORT_WITH_TESTING reach the examples through the target). They
# run anywhere: the device is a simulator in the program, reached over a
# socket of the loopback interface or over mock transports, and they say so
# when the library was built without those parts.
if(TARGET lumex::transport)
  lumex_example_executable(
    NAME LumexProtocolExampleClient
    SOURCE example_protocol_client.cpp
    LINK lumex::protocol lumex::transport Threads::Threads
    REQUIRE_TARGET lumex::transport
    CXX_STANDARD 11
    TEST_TIMEOUT 60
  )
  # A session that reconnects under a client, and a search for devices.
  lumex_example_executable(
    NAME LumexProtocolExampleReconnect
    SOURCE example_protocol_reconnect.cpp
    LINK lumex::protocol lumex::transport Threads::Threads
    REQUIRE_TARGET lumex::transport
    CXX_STANDARD 11
    TEST_TIMEOUT 60
  )
  lumex_example_executable(
    NAME LumexProtocolExampleSimulator
    SOURCE example_protocol_simulator.cpp
    LINK lumex::protocol lumex::transport
    REQUIRE_TARGET lumex::transport
    CXX_STANDARD 11
    TEST_TIMEOUT 60
  )
endif()
