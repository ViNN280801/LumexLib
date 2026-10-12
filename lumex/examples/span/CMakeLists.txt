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
lumex_example_executable(
  NAME LumexSpanExampleIterators
  SOURCE example_span_iterators.cpp
  LINK lumex::span
  REQUIRE_TARGET lumex::span
  CXX_STANDARD 11
)
# The conversions to and from std::span exist from C++20; the source prints a
# note and returns 0 where <span> is missing (GCC 8 at -std=c++2a).
lumex_example_executable(
  NAME LumexSpanExampleStdConversion
  SOURCE example_span_std_conversion.cpp
  LINK lumex::span
  REQUIRE_TARGET lumex::span
  CXX_STANDARD 20
)
lumex_example_executable(
  NAME LumexSpanExampleBasic
  SOURCE example_span_basic.cpp
  LINK lumex::span
  REQUIRE_TARGET lumex::span
  CXX_STANDARD 11
)
