# Standalone after install:
#   cmake -S . -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
#   cmake --build build
#
# In-tree: gated by lumex/examples/CMakeLists.txt via LUMEX_BUILD_* options.
#
# Together the examples show every public function, class and specification
# option of lumex::fmt: fields and the specification mini-language
# (example_format), the output API (example_format_api), ranges and tuples
# (example_format_ranges), chrono (example_format_chrono), user types
# (example_format_custom) and a practical report (example_format_workflow).

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexFormatExample
  SOURCE example_format.cpp
  LINK lumex::fmt lumex::string_view
  REQUIRE_TARGET lumex::fmt
)
lumex_example_executable(
  NAME LumexFormatExampleApi
  SOURCE example_format_api.cpp
  LINK lumex::fmt
  REQUIRE_TARGET lumex::fmt
)
lumex_example_executable(
  NAME LumexFormatExampleRanges
  SOURCE example_format_ranges.cpp
  LINK lumex::fmt
  REQUIRE_TARGET lumex::fmt
)
lumex_example_executable(
  NAME LumexFormatExampleChrono
  SOURCE example_format_chrono.cpp
  LINK lumex::fmt
  REQUIRE_TARGET lumex::fmt
)
lumex_example_executable(
  NAME LumexFormatExampleCustom
  SOURCE example_format_custom.cpp
  LINK lumex::fmt lumex::reflection
  REQUIRE_TARGET lumex::fmt
)
lumex_example_executable(
  NAME LumexFormatExampleWorkflow
  SOURCE example_format_workflow.cpp
  LINK lumex::fmt
  REQUIRE_TARGET lumex::fmt
)

# In-tree with tests: every public feature of lumex::fmt must appear in these
# examples (Scripts/CodeTools/check_fmt_examples_coverage.py).
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND LUMEX_BUILD_TESTS)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME LumexFormatExamplesCoverage
      COMMAND ${Python3_EXECUTABLE}
        ${LumexLib_SOURCE_DIR}/Scripts/CodeTools/check_fmt_examples_coverage.py
    )
    set_tests_properties(LumexFormatExamplesCoverage PROPERTIES LABELS "example")
  endif()
endif()
