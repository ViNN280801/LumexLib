# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexMathExample
  SOURCE example_math.cpp
  LINK lumex::math
  REQUIRE_TARGET lumex::math
  CXX_STANDARD 20
)
lumex_example_executable(
  NAME LumexMathExampleWorkflow
  SOURCE example_math_workflow.cpp
  LINK lumex::math
  REQUIRE_TARGET lumex::math
  CXX_STANDARD 20
)
