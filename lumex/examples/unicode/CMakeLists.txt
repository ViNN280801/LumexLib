# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexUnicodeExample
  SOURCE example_unicode.cpp
  LINK lumex::unicode
  REQUIRE_TARGET lumex::unicode
)
lumex_example_executable(
  NAME LumexUnicodeExampleWorkflow
  SOURCE example_unicode_workflow.cpp
  LINK lumex::unicode
  REQUIRE_TARGET lumex::unicode
)
