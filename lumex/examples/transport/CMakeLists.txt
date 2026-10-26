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
)
lumex_example_executable(
  NAME LumexTransportExampleAsync
  SOURCE example_transport_async.cpp
  LINK lumex::transport Threads::Threads
  REQUIRE_TARGET lumex::transport
  CXX_STANDARD 11
)
