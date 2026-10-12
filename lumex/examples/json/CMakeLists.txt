# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
# lumex::json is header-only and needs nlohmann/json 3.x from the consumer:
# standalone builds find it with find_package(nlohmann_json).
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options;
# the vendored 3rdparty/nlohmann copy is used.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexJsonExample
  SOURCE example_json.cpp
  LINK lumex::json
  REQUIRE_TARGET lumex::json
)
lumex_example_executable(
  NAME LumexJsonExampleWorkflow
  SOURCE example_json_workflow.cpp
  LINK lumex::json
  REQUIRE_TARGET lumex::json
)

if(COMMAND lumex_setup_nlohmann_json)
  lumex_setup_nlohmann_json()
elseif(NOT TARGET nlohmann_json::nlohmann_json)
  find_package(nlohmann_json 3 REQUIRED)
endif()
target_link_libraries(LumexJsonExample PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(LumexJsonExampleWorkflow
  PRIVATE nlohmann_json::nlohmann_json)
