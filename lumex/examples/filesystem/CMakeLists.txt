# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesFilesystem LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexFilesystemExample
  SOURCE example_filesystem.cpp
  LINK lumex::filesystem
  REQUIRE_TARGET lumex::filesystem
)
lumex_example_executable(
  NAME LumexFilesystemExampleWorkflow
  SOURCE example_filesystem_workflow.cpp
  LINK lumex::filesystem
  REQUIRE_TARGET lumex::filesystem
)
