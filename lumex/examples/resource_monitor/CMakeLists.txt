# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexResourceMonitorExample
  SOURCE example_resource_monitor.cpp
  LINK lumex::resource_monitor
  REQUIRE_TARGET lumex::resource_monitor
  CXX_STANDARD 17
)
lumex_example_executable(
  NAME LumexResourceMonitorExampleWorkflow
  SOURCE example_resource_monitor_workflow.cpp
  LINK lumex::resource_monitor
  REQUIRE_TARGET lumex::resource_monitor
  CXX_STANDARD 17
)
