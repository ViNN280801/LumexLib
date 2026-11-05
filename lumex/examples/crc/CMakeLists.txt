# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesCrc LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexCrcExample
  SOURCE example_crc.cpp
  LINK lumex::crc
  REQUIRE_TARGET lumex::crc
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexCrcExampleWorkflow
  SOURCE example_crc_workflow.cpp
  LINK lumex::crc
  REQUIRE_TARGET lumex::crc
  CXX_STANDARD 11
)
