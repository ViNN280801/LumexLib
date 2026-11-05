# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesExpected LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexExpectedExample
  SOURCE example_expected.cpp
  LINK lumex::expected
  REQUIRE_TARGET lumex::expected
)
lumex_example_executable(
  NAME LumexExpectedExampleWorkflow
  SOURCE example_expected_workflow.cpp
  LINK lumex::expected
  REQUIRE_TARGET lumex::expected
)
lumex_example_executable(
  NAME LumexExpectedExampleQuickstart
  SOURCE example_expected_quickstart.cpp
  LINK lumex::expected
  REQUIRE_TARGET lumex::expected
)
lumex_example_executable(
  NAME LumexExpectedExampleStd
  SOURCE example_expected_std.cpp
  LINK lumex::expected
  REQUIRE_TARGET lumex::expected
)
