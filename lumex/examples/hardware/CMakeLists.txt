# Standalone after install: configure this directory with LumexLib_DIR set.
# In-tree: gated by LUMEX_BUILD_APPLIED and LUMEX_BUILD_HARDWARE.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesHardware LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexHardwareExampleCPUVectorizationCapabilities
  SOURCE example_cpu_vectorization_capabilities.cpp
  LINK lumex::hardware
  REQUIRE_TARGET lumex::hardware
)
lumex_example_executable(
  NAME LumexHardwareExampleHardwareCapabilities
  SOURCE example_hardware_capabilities.cpp
  LINK lumex::hardware
  REQUIRE_TARGET lumex::hardware
)
lumex_example_executable(
  NAME LumexHardwareExampleWorkflow
  SOURCE example_hardware_workflow.cpp
  LINK lumex::hardware
  REQUIRE_TARGET lumex::hardware
)
