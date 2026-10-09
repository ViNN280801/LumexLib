# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.
# Both examples are pinned to C++11: the smart pointers work from that
# standard.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexSmartPtrExample
  SOURCE example_smart_ptr.cpp
  LINK lumex::smart_ptr Threads::Threads
  REQUIRE_TARGET lumex::smart_ptr
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexSmartPtrExampleWorkflow
  SOURCE example_smart_ptr_workflow.cpp
  LINK lumex::smart_ptr Threads::Threads
  REQUIRE_TARGET lumex::smart_ptr
  CXX_STANDARD 11
)
