# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.
# The walkthrough is pinned to C++11, the floor of the module; the workflow
# builds at the compiler's default standard.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexAtomicExample
  SOURCE example_atomic_smart_ptr.cpp
  LINK lumex::atomic Threads::Threads
  REQUIRE_TARGET lumex::atomic
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexAtomicExampleConfigReload
  SOURCE example_atomic_config_reload.cpp
  LINK lumex::atomic Threads::Threads
  REQUIRE_TARGET lumex::atomic
)
