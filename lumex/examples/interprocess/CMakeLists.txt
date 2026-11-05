# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesInterprocess LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexInterprocessExampleLock
  SOURCE example_interprocess_lock.cpp
  LINK lumex::interprocess
  REQUIRE_TARGET lumex::interprocess
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexInterprocessExampleLockHold
  SOURCE example_interprocess_lock_hold.cpp
  LINK lumex::interprocess Threads::Threads
  REQUIRE_TARGET lumex::interprocess
  CXX_STANDARD 11
)
# Waiting for a lock that another owner holds: a retry loop of the caller,
# with a limit. One process, a thread plays the other owner.
lumex_example_executable(
  NAME LumexInterprocessExampleLockRetry
  SOURCE example_interprocess_lock_retry.cpp
  LINK lumex::interprocess Threads::Threads
  REQUIRE_TARGET lumex::interprocess
  CXX_STANDARD 11
)
