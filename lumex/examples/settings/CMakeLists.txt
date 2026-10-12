# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

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
