# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesBase64 LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexBase64Example
  SOURCE example_base64.cpp
  LINK lumex::base64
  REQUIRE_TARGET lumex::base64
)
lumex_example_executable(
  NAME LumexBase64ExampleWorkflow
  SOURCE example_base64_workflow.cpp
  LINK lumex::base64
  REQUIRE_TARGET lumex::base64
)
