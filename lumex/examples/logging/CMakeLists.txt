# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesLogging LANGUAGES CXX)
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
