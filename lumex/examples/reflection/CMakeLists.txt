# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.
#
# The first two examples build from lumex::reflection alone. The field
# reflection example below also needs nlohmann/json (field_reflection/ includes
# <nlohmann/json.hpp>, and the umbrella only with LUMEX_WITH_FIELD_REFLECTION):
# in-tree it is built under the option with the vendored 3rdparty/nlohmann
# copy; standalone after install it needs find_package(nlohmann_json 3) to
# succeed, and is skipped otherwise.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexReflectionExample
  SOURCE example_reflection.cpp
  LINK lumex::reflection
  REQUIRE_TARGET lumex::reflection
)
lumex_example_executable(
  NAME LumexReflectionExampleWorkflow
  SOURCE example_reflection_workflow.cpp
  LINK lumex::reflection
  REQUIRE_TARGET lumex::reflection
)

# Field reflection example, once at C++11 (registered aggregates) and once at
# C++20 (also the names the compiler supplies).
set(_lumex_field_example_ready FALSE)
if(DEFINED LUMEX_WITH_FIELD_REFLECTION)
  # In-tree: under the option, with the vendored copy of nlohmann.
  if(LUMEX_WITH_FIELD_REFLECTION)
    lumex_setup_nlohmann_json()
    set(_lumex_field_example_ready TRUE)
  endif()
else()
  # Standalone after install: the consumer's own nlohmann.
  find_package(nlohmann_json 3 QUIET)
  if(TARGET nlohmann_json::nlohmann_json)
    set(_lumex_field_example_ready TRUE)
  endif()
endif()

if(_lumex_field_example_ready)
  lumex_example_executable(
    NAME LumexFieldReflectionExample
    SOURCE example_field_reflection.cpp
    LINK lumex::reflection nlohmann_json::nlohmann_json
    REQUIRE_TARGET lumex::reflection
    CXX_STANDARD 11
  )
  lumex_example_executable(
    NAME LumexFieldReflectionExampleCxx20
    SOURCE example_field_reflection.cpp
    LINK lumex::reflection nlohmann_json::nlohmann_json
    REQUIRE_TARGET lumex::reflection
    CXX_STANDARD 20
  )
  target_compile_definitions(LumexFieldReflectionExample
    PRIVATE LUMEX_WITH_FIELD_REFLECTION)
  target_compile_definitions(LumexFieldReflectionExampleCxx20
    PRIVATE LUMEX_WITH_FIELD_REFLECTION)
else()
  message(STATUS "Skipping the field reflection example (needs "
    "LUMEX_WITH_FIELD_REFLECTION=ON in-tree, or nlohmann_json when standalone)")
endif()
