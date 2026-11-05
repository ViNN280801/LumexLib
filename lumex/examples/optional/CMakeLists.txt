# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesOptional LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexOptionalExample
  SOURCE example_optional.cpp
  LINK lumex::optional
  REQUIRE_TARGET lumex::optional
)
lumex_example_executable(
  NAME LumexOptionalExampleWorkflow
  SOURCE example_optional_workflow.cpp
  LINK lumex::optional
  REQUIRE_TARGET lumex::optional
)
lumex_example_executable(
  NAME LumexOptionalExampleMonadic
  SOURCE example_optional_monadic.cpp
  LINK lumex::optional
  REQUIRE_TARGET lumex::optional
)
lumex_example_executable(
  NAME LumexOptionalExampleLiteral
  SOURCE example_optional_literal.cpp
  LINK lumex::optional
  REQUIRE_TARGET lumex::optional
)
# The conversions to and from std::optional exist from C++17.
lumex_example_executable(
  NAME LumexOptionalExampleStdConversion
  SOURCE example_optional_std_conversion.cpp
  LINK lumex::optional
  REQUIRE_TARGET lumex::optional
  CXX_STANDARD 17
)
