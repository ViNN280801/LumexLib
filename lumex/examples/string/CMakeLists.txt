# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesString LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexStringExample
  SOURCE example_string.cpp
  LINK lumex::string
  REQUIRE_TARGET lumex::string
)
lumex_example_executable(
  NAME LumexStringExampleWorkflow
  SOURCE example_string_workflow.cpp
  LINK lumex::string
  REQUIRE_TARGET lumex::string
)
