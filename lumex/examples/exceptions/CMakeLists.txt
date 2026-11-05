# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesExceptions LANGUAGES CXX)
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
