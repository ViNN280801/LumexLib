# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesFraming LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexFramingExample
  SOURCE example_framing.cpp
  LINK lumex::framing
  REQUIRE_TARGET lumex::framing
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexFramingExamplePipeline
  SOURCE example_framing_pipeline.cpp
  LINK lumex::framing
  REQUIRE_TARGET lumex::framing
  CXX_STANDARD 11
)
# The binary codecs (COBS, length prefix, header with a CRC, Modbus RTU) and
# the stages of a pipeline, each on a made-up protocol.
lumex_example_executable(
  NAME LumexFramingExampleBinary
  SOURCE example_framing_binary.cpp
  LINK lumex::framing
  REQUIRE_TARGET lumex::framing
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexFramingExampleStages
  SOURCE example_framing_stages.cpp
  LINK lumex::framing
  REQUIRE_TARGET lumex::framing
  CXX_STANDARD 11
)
