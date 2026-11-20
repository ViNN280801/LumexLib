# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.
# Both examples are pinned to C++11: the units catalog works from that
# standard.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesUnits LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexUnitsExample
  SOURCE example_units.cpp
  LINK lumex::units
  REQUIRE_TARGET lumex::units
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexUnitsExampleWorkflow
  SOURCE example_units_workflow.cpp
  LINK lumex::units
  REQUIRE_TARGET lumex::units
  CXX_STANDARD 11
)
