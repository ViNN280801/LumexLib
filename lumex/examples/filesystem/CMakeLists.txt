# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexFilesystemExample
  SOURCE example_filesystem.cpp
  LINK lumex::filesystem
  REQUIRE_TARGET lumex::filesystem
)
lumex_example_executable(
  NAME LumexFilesystemExampleWorkflow
  SOURCE example_filesystem_workflow.cpp
  LINK lumex::filesystem
  REQUIRE_TARGET lumex::filesystem
)
