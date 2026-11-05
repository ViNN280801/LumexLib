# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesCircularBuffer LANGUAGES CXX)
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
lumex_example_executable(
  NAME LumexCircularBufferExampleQueue
  SOURCE example_circular_buffer_queue.cpp
  LINK lumex::circular_buffer
  REQUIRE_TARGET lumex::circular_buffer
)
lumex_example_executable(
  NAME LumexCircularBufferExampleChecked
  SOURCE example_circular_buffer_checked.cpp
  LINK lumex::circular_buffer
  REQUIRE_TARGET lumex::circular_buffer
)
lumex_example_executable(
  NAME LumexCircularBufferExampleAllocator
  SOURCE example_circular_buffer_allocator.cpp
  LINK lumex::circular_buffer
  REQUIRE_TARGET lumex::circular_buffer
)
lumex_example_executable(
  NAME LumexCircularBufferExampleEditing
  SOURCE example_circular_buffer_editing.cpp
  LINK lumex::circular_buffer
  REQUIRE_TARGET lumex::circular_buffer
)
