# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexTransportExampleSync
  SOURCE example_transport_sync.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
lumex_example_executable(
  NAME LumexTransportExampleAsync
  SOURCE example_transport_async.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
# The framed and the HID examples show a message when the library was built
# without the framing module or the HID transport (LUMEX_TRANSPORT_WITH_FRAMING,
# LUMEX_TRANSPORT_WITH_HID of lumex::transport): they run anywhere.
lumex_example_executable(
  NAME LumexTransportExampleFramed
  SOURCE example_transport_framed.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
lumex_example_executable(
  NAME LumexTransportExampleHid
  SOURCE example_transport_hid.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
  # Only the count in the CTest log: the full listing names the devices of
  # the machine, run the example by hand for it.
  TEST_ARGS --count
)
# The session examples reconnect against loopback servers started by the
# example itself: no hardware, no network beyond 127.0.0.1.
lumex_example_executable(
  NAME LumexTransportExampleSession
  SOURCE example_transport_session.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
lumex_example_executable(
  NAME LumexTransportExampleHotplug
  SOURCE example_transport_hotplug.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
# The datagram example frames datagrams only when the framing module is
# built (LUMEX_TRANSPORT_WITH_FRAMING of lumex::transport); it says so
# otherwise.
lumex_example_executable(
  NAME LumexTransportExampleDatagram
  SOURCE example_transport_datagram.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
# The statistics and cancellation examples drive the mock transports of
# lumex::transport (LUMEX_TRANSPORT_WITH_TESTING) so that every count is
# exact; they say so when the library was built without them.
lumex_example_executable(
  NAME LumexTransportExampleStatistics
  SOURCE example_transport_statistics.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
lumex_example_executable(
  NAME LumexTransportExampleCancellation
  SOURCE example_transport_cancellation.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
# C++20 coroutines (asio::use_awaitable, co_spawn, awaitable operators). The
# example builds as C++20 everywhere; where the toolchain has no coroutines
# (GCC 8 with -std=c++2a, MinGW 8.3) it prints that and exits 0.
lumex_example_executable(
  NAME LumexTransportExampleCoroutinesCxx20
  SOURCE example_transport_coroutines.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 20
  TEST_TIMEOUT 60
)
# The log example routes the transport log into the LumexLogger file, so it
# writes a trigger file and the log next to the executable, as the logger's
# own re-read tests do: the same RESOURCE_LOCK runs them one at a time under
# ctest -j. It shows only part 1 when the library was built without the
# logger module.
lumex_example_executable(
  NAME LumexTransportExampleLog
  SOURCE example_transport_log.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
if(LUMEX_BUILD_TESTS AND NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  lumex_example_test_name(_lumex_transport_log_test LumexTransportExampleLog)
  set_tests_properties(${_lumex_transport_log_test}
    PROPERTIES RESOURCE_LOCK lumex_logger_trigger_file)
endif()
# The errors example opens only a port that does not exist, a temporary file
# whose mode forbids the open (POSIX, not as root) and a loopback port nobody
# listens on.
lumex_example_executable(
  NAME LumexTransportExampleErrors
  SOURCE example_transport_errors.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
# A workflow like a real device client: a reconnecting session, a line codec
# of lumex::framing, the counters and the log (the framing module is optional
# for lumex::transport, the example says so without it).
lumex_example_executable(
  NAME LumexTransportExampleWorkflow
  SOURCE example_transport_workflow.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
  TEST_TIMEOUT 60
)
