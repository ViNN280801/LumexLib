# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.
# Both examples are pinned to C++11: span works from that standard.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexSpanExample
  SOURCE example_span.cpp
  LINK lumex::span
  REQUIRE_TARGET lumex::span
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexSpanExampleWorkflow
  SOURCE example_span_workflow.cpp
  LINK lumex::span
  REQUIRE_TARGET lumex::span
  CXX_STANDARD 11
)
