# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexExceptionsExample
  SOURCE example_exceptions.cpp
  LINK lumex::exceptions
  REQUIRE_TARGET lumex::exceptions
)
lumex_example_executable(
  NAME LumexExceptionsExampleWorkflow
  SOURCE example_exceptions_workflow.cpp
  LINK lumex::exceptions
  REQUIRE_TARGET lumex::exceptions
)
