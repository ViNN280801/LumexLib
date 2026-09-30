# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexLoggerExample
  SOURCE example_logger.cpp
  LINK lumex::logger
  REQUIRE_TARGET lumex::logger
  CXX_STANDARD 11
)
# The workflow starts std::thread workers itself: before glibc 2.34
# pthread_create lives in libpthread, which only Threads::Threads adds.
lumex_example_executable(
  NAME LumexLoggerExampleWorkflow
  SOURCE example_logger_workflow.cpp
  LINK lumex::logger Threads::Threads
  REQUIRE_TARGET lumex::logger
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexLoggerExampleMacros
  SOURCE example_logger_macros.cpp
  LINK lumex::logger
  REQUIRE_TARGET lumex::logger
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexLoggerExampleConfigFormats
  SOURCE example_logger_config_formats.cpp
  LINK lumex::logger
  REQUIRE_TARGET lumex::logger
  CXX_STANDARD 11
)
