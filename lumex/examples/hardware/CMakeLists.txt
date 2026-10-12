# Standalone after install: configure this directory with LumexLib_DIR set.
# In-tree: gated by LUMEX_BUILD_APPLIED and LUMEX_BUILD_HARDWARE.

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
