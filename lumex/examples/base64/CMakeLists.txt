# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexBase64Example
  SOURCE example_base64.cpp
  LINK lumex::base64
  REQUIRE_TARGET lumex::base64
)
lumex_example_executable(
  NAME LumexBase64ExampleWorkflow
  SOURCE example_base64_workflow.cpp
  LINK lumex::base64
  REQUIRE_TARGET lumex::base64
)
