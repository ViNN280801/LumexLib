# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesGenerators LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexGeneratorsExample
  SOURCE example_generators.cpp
  LINK lumex::number_generator
  REQUIRE_TARGET lumex::number_generator
)
lumex_example_executable(
  NAME LumexGeneratorsExampleWorkflow
  SOURCE example_generators_workflow.cpp
  LINK lumex::number_generator
  REQUIRE_TARGET lumex::number_generator
)
