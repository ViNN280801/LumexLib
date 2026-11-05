# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

cmake_minimum_required(VERSION 3.16)
# A project of its own: standalone after install, a subproject in the tree.
project(LumexExamplesSettings LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexSettingsExample
  SOURCE example_settings.cpp
  LINK lumex::settings
  REQUIRE_TARGET lumex::settings
)
lumex_example_executable(
  NAME LumexSettingsExampleWorkflow
  SOURCE example_settings_workflow.cpp
  LINK lumex::settings
  REQUIRE_TARGET lumex::settings
)
lumex_example_executable(
  NAME LumexSettingsExampleXml
  SOURCE example_settings_xml.cpp
  LINK lumex::settings
  REQUIRE_TARGET lumex::settings
)
lumex_example_executable(
  NAME LumexSettingsExampleJson
  SOURCE example_settings_json.cpp
  LINK lumex::settings
  REQUIRE_TARGET lumex::settings
)

# example_settings, example_settings_xml and example_settings_json write the
# same files in the working directory (lumex_settings_example.xml and .json),
# so ctest -j must not run them at the same time.
if(LUMEX_BUILD_TESTS AND NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  foreach(_example LumexSettingsExample LumexSettingsExampleXml LumexSettingsExampleJson)
    lumex_example_test_name(_ctest_name ${_example})
    set_tests_properties(${_ctest_name}
      PROPERTIES RESOURCE_LOCK lumex_settings_example_files)
  endforeach()
endif()
