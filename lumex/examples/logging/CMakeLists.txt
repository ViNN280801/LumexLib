# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexLoggingExample
  SOURCE example_logging.cpp
  LINK lumex::logging
  REQUIRE_TARGET lumex::logging
)
lumex_example_executable(
  NAME LumexLoggingExampleWorkflow
  SOURCE example_logging_workflow.cpp
  LINK lumex::logging
  REQUIRE_TARGET lumex::logging
)
