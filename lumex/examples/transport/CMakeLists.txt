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
