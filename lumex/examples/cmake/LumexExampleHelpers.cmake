# Shared helpers for lumex/examples/<component>/CMakeLists.txt.
#
# Dual mode:
# - In-tree (add_subdirectory from lumex/examples/): link lumex::* aliases
#   already created by the library build.
# - Standalone (this dir is CMAKE_SOURCE_DIR): find_package(LumexLib) after
#   install, then link the same lumex::* names.
# When LUMEX_BUILD_TESTS is ON, each in-tree example is also an add_test
# (exit 0 required). Binaries land in ${CMAKE_BINARY_DIR}/bin next to
# shared Lumex DLLs.
#
# Usage in a component CMakeLists.txt:
#   include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
#   lumex_example_executable(
#     NAME LumexBase64Example
#     SOURCE example_base64.cpp
#     LINK lumex::base64
#     REQUIRE_TARGET lumex::base64
#     [CXX_STANDARD 14]
#   )

function(lumex_example_executable)
  set(options)
  set(oneValueArgs NAME SOURCE REQUIRE_TARGET CXX_STANDARD)
  set(multiValueArgs LINK)
  cmake_parse_arguments(LEX "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT LEX_NAME OR NOT LEX_SOURCE OR NOT LEX_LINK)
    message(FATAL_ERROR "lumex_example_executable requires NAME, SOURCE, and LINK")
  endif()
  if(NOT LEX_REQUIRE_TARGET)
    list(GET LEX_LINK 0 LEX_REQUIRE_TARGET)
  endif()

  if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.16)
    project(${LEX_NAME} LANGUAGES CXX)
    find_package(LumexLib REQUIRED)
    if(NOT TARGET ${LEX_REQUIRE_TARGET})
      message(FATAL_ERROR
        "${LEX_REQUIRE_TARGET} not found. Install LumexLib with the matching "
        "LUMEX_BUILD_* option enabled, then set LumexLib_DIR to the package "
        "config directory.")
    endif()
  endif()

  add_executable(${LEX_NAME} ${LEX_SOURCE})
  target_link_libraries(${LEX_NAME} PRIVATE ${LEX_LINK})

  if(LEX_CXX_STANDARD)
    set_target_properties(${LEX_NAME} PROPERTIES
      CXX_STANDARD ${LEX_CXX_STANDARD}
      CXX_STANDARD_REQUIRED ON
    )
  endif()

  if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Same directory as shared Lumex DLLs. CTest runs the exe from here so
    # Windows STATUS_DLL_NOT_FOUND does not fire, matching lumex_test_use_gtest.
    set_target_properties(${LEX_NAME} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    set_property(GLOBAL APPEND PROPERTY LUMEX_EXAMPLE_TARGETS ${LEX_NAME})
    if(LUMEX_BUILD_TESTS)
      add_test(NAME ${LEX_NAME} COMMAND ${LEX_NAME})
      set_tests_properties(${LEX_NAME} PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${LEX_NAME}>"
        LABELS "example"
      )
    endif()
  else()
    install(TARGETS ${LEX_NAME} DESTINATION bin)
  endif()

  # Examples are not the shipped surface, so silence every warning for the
  # example translation unit (CMakeRoutines `suppress_warnings_for_sources`).
  # Called from the example's own directory, which is what CMake requires:
  # source-file properties are scoped to the directory that sets them.
  if(COMMAND suppress_warnings_for_sources)
    suppress_warnings_for_sources(${LEX_NAME} MATCH ".*" ALL)
  endif()
  set_property(TARGET ${LEX_NAME} PROPERTY LUMEX_SUPPRESS_ALL_WARNINGS TRUE)
endfunction()
