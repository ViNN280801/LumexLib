# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexReflectionExample
  SOURCE example_reflection.cpp
  LINK lumex::reflection
  REQUIRE_TARGET lumex::reflection
)
lumex_example_executable(
  NAME LumexReflectionExampleWorkflow
  SOURCE example_reflection_workflow.cpp
  LINK lumex::reflection
  REQUIRE_TARGET lumex::reflection
)
