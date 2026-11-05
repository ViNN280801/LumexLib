# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesResourceMonitor LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexResourceMonitorExample
  SOURCE example_resource_monitor.cpp
  LINK lumex::resource_monitor
  REQUIRE_TARGET lumex::resource_monitor
)
lumex_example_executable(
  NAME LumexResourceMonitorExampleWorkflow
  SOURCE example_resource_monitor_workflow.cpp
  LINK lumex::resource_monitor
  REQUIRE_TARGET lumex::resource_monitor
)
