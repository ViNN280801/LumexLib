# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesSerial LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexSerialExample
  SOURCE example_serial.cpp
  LINK lumex::serial
  REQUIRE_TARGET lumex::serial
)
lumex_example_executable(
  NAME LumexSerialExampleWorkflow
  SOURCE example_serial_workflow.cpp
  LINK lumex::serial
  REQUIRE_TARGET lumex::serial
)
