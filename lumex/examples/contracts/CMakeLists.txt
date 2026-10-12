# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.
# Both examples are pinned to C++11: contract assertions work from that standard.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexContractsExample
  SOURCE example_contracts.cpp
  LINK lumex::contracts
  REQUIRE_TARGET lumex::contracts
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexContractsExampleWorkflow
  SOURCE example_contracts_workflow.cpp
  LINK lumex::contracts
  REQUIRE_TARGET lumex::contracts
  CXX_STANDARD 11
)
