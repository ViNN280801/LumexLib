# Dual vendored GoogleTest: 1.12.1 for C++11/14 tests, 1.18.0 for C++17+.
# Official add_subdirectory() cannot host both trees in one build - both
# export the same gtest / gtest_main target names. These targets compile
# gtest-all.cc from each tree under distinct names instead.
#
# Usage from a test CMakeLists.txt, after add_executable() and after
# target_link_libraries() for Lumex modules:
#   lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)

find_package(Threads REQUIRED)

# Resolve against this file, not CMAKE_SOURCE_DIR: LumexLib is often a
# submodule, and CMAKE_SOURCE_DIR then points at the consuming project.
get_filename_component(_lumex_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(LUMEX_GTEST_1_12_ROOT "${_lumex_root}/3rdparty/googletest-1.12.1")
set(LUMEX_GTEST_1_18_ROOT "${_lumex_root}/3rdparty/googletest-1.18.0")

function(lumex_add_vendored_gtest prefix source_root cxx_std)
  set(gtest_dir "${source_root}/googletest")
  if(NOT EXISTS "${gtest_dir}/src/gtest-all.cc")
    message(FATAL_ERROR "Vendored GoogleTest sources missing: ${gtest_dir}/src/gtest-all.cc")
  endif()

  set(lib_name "lumex_gtest_${prefix}")
  set(main_name "lumex_gtest_main_${prefix}")

  add_library(${lib_name} STATIC "${gtest_dir}/src/gtest-all.cc")
  add_library(${main_name} STATIC "${gtest_dir}/src/gtest_main.cc")

  foreach(tgt IN ITEMS ${lib_name} ${main_name})
    set_target_properties(${tgt} PROPERTIES
      CXX_STANDARD ${cxx_std}
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
      POSITION_INDEPENDENT_CODE ON
      INTERPROCEDURAL_OPTIMIZATION OFF
    )
    target_include_directories(${tgt}
      SYSTEM PUBLIC "${gtest_dir}/include"
      PRIVATE "${gtest_dir}"
    )
    target_link_libraries(${tgt} PUBLIC Threads::Threads)
    # Prefer CMakeRoutines WarningSuppression when LumexBuild included it.
    if(COMMAND suppress_warnings)
      suppress_warnings(${tgt})
    elseif(MSVC)
      target_compile_options(${tgt} PRIVATE /W0)
    else()
      target_compile_options(${tgt} PRIVATE -w)
    endif()
  endforeach()

  target_link_libraries(${main_name} PUBLIC ${lib_name})
endfunction()

lumex_add_vendored_gtest(1_12 "${LUMEX_GTEST_1_12_ROOT}" 11)
lumex_add_vendored_gtest(1_18 "${LUMEX_GTEST_1_18_ROOT}" 17)

add_library(lumex::gtest_cxx11 ALIAS lumex_gtest_1_12)
add_library(lumex::gtest_main_cxx11 ALIAS lumex_gtest_main_1_12)
add_library(lumex::gtest_cxx17 ALIAS lumex_gtest_1_18)
add_library(lumex::gtest_main_cxx17 ALIAS lumex_gtest_main_1_18)

# Pins the test target's language standard and links the matching gtest.
# CXX_STANDARD 11 or 14 -> GoogleTest 1.12.1
# CXX_STANDARD 17 or 20 -> GoogleTest 1.18.0
function(lumex_test_use_gtest target)
  cmake_parse_arguments(ARG "" "CXX_STANDARD" "" ${ARGN})
  if(NOT ARG_CXX_STANDARD)
    message(FATAL_ERROR "lumex_test_use_gtest(${target}): CXX_STANDARD is required (11, 14, 17, or 20)")
  endif()

  set_target_properties(${target} PROPERTIES
    CXX_STANDARD ${ARG_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    # Same directory as shared Lumex DLLs (bin/, not bin/$<CONFIG>).
    # gtest_discover_tests runs the exe at build time; a split output
    # directory yields Windows STATUS_DLL_NOT_FOUND (0xc0000135).
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )

  if(ARG_CXX_STANDARD LESS 17)
    target_link_libraries(${target} PRIVATE lumex::gtest_main_cxx11)
  else()
    target_link_libraries(${target} PRIVATE lumex::gtest_main_cxx17)
  endif()

  # TYPED_TEST_SUITE Cartesian products (SafeComparator mixed pairs,
  # FieldReflection arity 1-32) exceed the default COFF section limit
  # (MSVC C1128). clang-cl accepts the same /bigobj switch. MinGW gas
  # needs the equivalent assembler flag.
  if(MSVC)
    target_compile_options(${target} PRIVATE /bigobj)
    # /external:* is MSVC cl.exe only; clang-cl warns on anglebrackets.
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      target_compile_options(${target} PRIVATE
        /external:W0
        /external:anglebrackets)
    endif()
  elseif(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${target} PRIVATE -Wa,-mbig-obj)
  endif()

  # Stage the clang-cl ASan runtime DLL next to the test executable BEFORE
  # gtest_discover_tests registers its POST_BUILD discovery command. POST_BUILD
  # commands run in registration order, and every test CMakeLists calls
  # lumex_test_use_gtest immediately before gtest_discover_tests, so the copy
  # lands first and the discovery run can load clang_rt.asan_dynamic-*.dll.
  if(COMMAND stage_clang_sanitizer_runtime)
    stage_clang_sanitizer_runtime(${target}
      ADDRESS ${LUMEX_USE_ASAN}
      UNDEFINED ${LUMEX_USE_UBSAN}
      THREAD ${LUMEX_USE_TSAN})
  endif()

  # Tests are not the shipped surface, so silence every warning for the test
  # translation unit (CMakeRoutines `suppress_warnings_for_sources`). This
  # helper runs in the test's own directory, which is what CMake requires:
  # source-file properties are scoped to the directory that sets them, so the
  # same call from the root CMakeLists would not reach this target.
  if(COMMAND suppress_warnings_for_sources)
    suppress_warnings_for_sources(${target} MATCH ".*" ALL)
  endif()
  set_property(TARGET ${target} PROPERTY LUMEX_SUPPRESS_ALL_WARNINGS TRUE)
endfunction()
