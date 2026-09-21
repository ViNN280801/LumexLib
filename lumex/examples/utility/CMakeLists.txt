# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexUtilityExample
  SOURCE example_utility.cpp
  LINK lumex::utility
  REQUIRE_TARGET lumex::utility
)
lumex_example_executable(
  NAME LumexUtilityExampleWorkflow
  SOURCE example_utility_workflow.cpp
  LINK lumex::utility
  REQUIRE_TARGET lumex::utility
)
