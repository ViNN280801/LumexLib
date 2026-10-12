# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexTimeExample
  SOURCE example_time.cpp
  LINK lumex::time
  REQUIRE_TARGET lumex::time
)
lumex_example_executable(
  NAME LumexTimeExampleWorkflow
  SOURCE example_time_workflow.cpp
  LINK lumex::time
  REQUIRE_TARGET lumex::time
)
