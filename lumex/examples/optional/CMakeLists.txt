# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexOptionalExample
  SOURCE example_optional.cpp
  LINK lumex::optional
  REQUIRE_TARGET lumex::optional
)
lumex_example_executable(
  NAME LumexOptionalExampleWorkflow
  SOURCE example_optional_workflow.cpp
  LINK lumex::optional
  REQUIRE_TARGET lumex::optional
)
