# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.
# Both examples are pinned to C++11: hazard pointers work from that standard.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexHazardPointerExample
  SOURCE example_hazard_pointer.cpp
  LINK lumex::hazard_pointer Threads::Threads
  REQUIRE_TARGET lumex::hazard_pointer
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexHazardPointerExampleWorkflow
  SOURCE example_hazard_pointer_workflow.cpp
  LINK lumex::hazard_pointer Threads::Threads
  REQUIRE_TARGET lumex::hazard_pointer
  CXX_STANDARD 11
)
