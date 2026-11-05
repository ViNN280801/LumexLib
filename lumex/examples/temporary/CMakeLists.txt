# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesTemporary LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexTemporaryExample
  SOURCE example_temporary.cpp
  LINK lumex::temporary
  REQUIRE_TARGET lumex::temporary
)
lumex_example_executable(
  NAME LumexTemporaryExampleWorkflow
  SOURCE example_temporary_workflow.cpp
  LINK lumex::temporary
  REQUIRE_TARGET lumex::temporary
)
