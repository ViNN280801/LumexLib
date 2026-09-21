# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexCircularBufferExample
  SOURCE example_circular_buffer.cpp
  LINK lumex::circular_buffer
  REQUIRE_TARGET lumex::circular_buffer
)
lumex_example_executable(
  NAME LumexCircularBufferExampleWorkflow
  SOURCE example_circular_buffer_workflow.cpp
  LINK lumex::circular_buffer
  REQUIRE_TARGET lumex::circular_buffer
)
