# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexStringViewExample
  SOURCE example_string_view.cpp
  LINK lumex::string_view
  REQUIRE_TARGET lumex::string_view
)
lumex_example_executable(
  NAME LumexStringViewExampleWorkflow
  SOURCE example_string_view_workflow.cpp
  LINK lumex::string_view
  REQUIRE_TARGET lumex::string_view
)
lumex_example_executable(
  NAME LumexStringViewExampleTextApi
  SOURCE example_string_view_text_api.cpp
  LINK lumex::string_view
  REQUIRE_TARGET lumex::string_view
  CXX_STANDARD 11
)
lumex_example_executable(
  NAME LumexStringViewExampleStdConversion
  SOURCE example_string_view_std_conversion.cpp
  LINK lumex::string_view
  REQUIRE_TARGET lumex::string_view
  CXX_STANDARD 17
)
lumex_example_executable(
  NAME LumexWStringViewExample
  SOURCE example_wstring_view.cpp
  LINK lumex::string_view
  REQUIRE_TARGET lumex::string_view
  CXX_STANDARD 11
)
