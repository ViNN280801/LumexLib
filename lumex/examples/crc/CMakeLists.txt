# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexCrcExample
  SOURCE example_crc.cpp
  LINK lumex::crc
  REQUIRE_TARGET lumex::crc
  CXX_STANDARD 14
)
lumex_example_executable(
  NAME LumexCrcExampleWorkflow
  SOURCE example_crc_workflow.cpp
  LINK lumex::crc
  REQUIRE_TARGET lumex::crc
  CXX_STANDARD 14
)
