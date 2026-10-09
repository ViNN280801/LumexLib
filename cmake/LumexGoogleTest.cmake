# Dual vendored GoogleTest: 1.12.1 for C++11/14 tests, 1.18.0 for C++17+.
# Official add_subdirectory() cannot host both trees in one build - both
# export the same gtest / gtest_main target names. These targets compile
# gtest-all.cc from each tree under distinct names instead.
#
# Usage from a test CMakeLists.txt: lumex_add_standard_suites (at the end of
# this file) builds one suite per C++ standard of the module from
# lumex/tests/LumexTestStandards.cmake. A hand-made executable calls, after
# add_executable() and after target_link_libraries() for Lumex modules:
#   lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)

find_package(Threads REQUIRED)

# lumex_test_name: CTest names derived from the registering directory.
include("${CMAKE_CURRENT_LIST_DIR}/LumexTestNames.cmake")

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
# CXX_STANDARD 17 and above -> GoogleTest 1.18.0
function(lumex_test_use_gtest target)
  cmake_parse_arguments(ARG "" "CXX_STANDARD" "" ${ARGN})
  if(NOT ARG_CXX_STANDARD)
    message(FATAL_ERROR "lumex_test_use_gtest(${target}): CXX_STANDARD is required (11, 14, 17, 20, 23 or 26)")
  endif()

  set_target_properties(${target} PROPERTIES
    CXX_STANDARD ${ARG_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    # Same directory as shared Lumex DLLs (bin/, not bin/$<CONFIG>).
    # A split output directory yields Windows STATUS_DLL_NOT_FOUND
    # (0xc0000135) when ctest later runs the exe.
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )

  # Incremental link opens the existing exe to patch it. POST_BUILD
  # discovery (and a parallel rebuild) keeps that image mapped, and
  # link.exe then fails with LNK1104 on the output exe.
  if(MSVC)
    target_link_options(${target} PRIVATE "/INCREMENTAL:NO")
  endif()

  if(ARG_CXX_STANDARD LESS 17)
    target_link_libraries(${target} PRIVATE lumex::gtest_main_cxx11)
  else()
    target_link_libraries(${target} PRIVATE lumex::gtest_main_cxx17)
  endif()

  # TYPED_TEST_SUITE Cartesian products (safe_comparator mixed pairs,
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

  # Stage the clang-cl ASan runtime DLL next to the test executable at
  # build time. lumex_gtest_discover_tests uses DISCOVERY_MODE PRE_TEST, so
  # the exe is not launched from the link recipe; ctest still needs the DLL
  # beside it. POST_BUILD commands run in registration order, and every test
  # CMakeLists calls lumex_test_use_gtest before lumex_gtest_discover_tests.
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

# Register tests without executing the binary from the link recipe.
# DISCOVERY_MODE PRE_TEST runs --gtest_list_tests at ctest time. The old
# POST_BUILD mode left the exe mapped, and the next MSVC link failed with
# LNK1104 (cannot open the output exe) under /INCREMENTAL.
#
# CTest names are <prefix><Suite>.<Test><suffix>. The prefix comes from the
# calling directory (lumex_test_name, cmake/LumexTestNames.cmake): crc. for
# lumex/tests/core/crc. An explicit TEST_PREFIX replaces it; TEST_SUFFIX and
# every other argument go to gtest_discover_tests unchanged.
function(lumex_gtest_discover_tests target)
  cmake_parse_arguments(ARG "" "TEST_PREFIX" "" ${ARGN})
  set(_override "")
  if(DEFINED ARG_TEST_PREFIX OR "TEST_PREFIX" IN_LIST ARG_KEYWORDS_MISSING_VALUES)
    set(_override TEST_PREFIX "${ARG_TEST_PREFIX}")
  endif()
  lumex_test_name(_prefix "" ${_override})
  gtest_discover_tests(${target}
    TEST_PREFIX "${_prefix}"
    ${ARG_UNPARSED_ARGUMENTS}
    DISCOVERY_MODE PRE_TEST)
endfunction()

# lumex_add_standard_suites(<Component> MODULE <key>
#     [LINK <item>...] [SOURCES <file>...] [DEFINITIONS <definition>...]
#     [EXCLUDE <file>...] [DISCOVER_ARGS <argument>...] [PLAIN_EXECUTABLE]
#     [TARGETS_VAR <out_var>] [SOAK_FILTER <gtest filter>]
#     [VARIANT <name> [DEFINITIONS <definition>...]]...)
#
# The common DEFINITIONS go before the first VARIANT: after VARIANT <name>,
# DEFINITIONS belongs to that variant. Every other keyword ends the group.
#
# One test suite (one executable) per C++ standard of the test module <key>,
# as listed in lumex/tests/LumexTestStandards.cmake, which must be included
# before (lumex/tests/CMakeLists.txt does). That file also explains the
# scheme and how to convert a module; in short:
#
# - The test tree follows the source tree: a test directory is lumex/tests/
#   plus the path of a directory of lumex/ (core/math/ops tests
#   lumex/core/math/ops), and every directory with tests calls this function
#   once. MODULE <key> is the module the directory belongs to, the CTest
#   prefix of the module's own test directory without the trailing dot
#   (math, utility, generators.number_generator); the directory itself may be
#   that directory or any directory below it, and its CTest prefix is the
#   directory's own (math.ops.).
# - The test sources are every <Stem>.cxx<std>.tests.cpp of the calling
#   directory, whatever the stem, so a directory with several components
#   still gets one executable per standard. The suite of a standard compiles
#   the files of that standard and of every lower standard of the module.
#   A directory has no suite of a standard below its lowest file: tests that
#   need C++20 get suites from C++20 on, as they did in the module's
#   directory. A *.tests.cpp outside the scheme, or of a standard the module
#   does not list, is an error. EXCLUDE leaves named scheme files out of every
#   suite (for sources that need an optional dependency).
# - Executables are Lumex<Component>Cxx<std>Tests; <Component> is given
#   without the Lumex prefix and is unique in the tree (a module and the
#   directory below it: MathOps, Base64Encode). CTest names get the
#   directory's prefix (lumex_test_name) and the suffix .cxx<std>, at every
#   standard.
# - A standard above LUMEX_TEST_STANDARDS_OPTIONAL_ABOVE is built only when
#   CMAKE_CXX_COMPILE_FEATURES has cxx_std_<std>; otherwise it is skipped
#   with a STATUS message.
# - VARIANT <name> adds suites on the same sources at the standards the table
#   declares for that variant, with DEFINITIONS on top of the common ones:
#   Lumex<Component><Name in CamelCase>Cxx<std>Tests, CTest suffix
#   .<name>.cxx<std>. Every variant of the table must be passed.
# - LINK goes to target_link_libraries PRIVATE, SOURCES (helper headers, a
#   support .cpp) into every suite, DISCOVER_ARGS to
#   lumex_gtest_discover_tests (TEST_SUFFIX is the helper's own).
# - PLAIN_EXECUTABLE is for a suite with its own main () and no GoogleTest
#   test list: each executable is one CTest test named
#   <prefix>Lumex<Component>Tests<suffix>.
# - SOAK_FILTER <gtest filter> names the long soak tests of the directory
#   (for example *Soak*). They are left out of the ordinary registration
#   (TEST_FILTER, CMake 3.22 and later; on an older CMake they stay and skip
#   themselves unless LUMEX_TEST_SOAK is set in the environment). When the
#   tree is configured with -DLUMEX_BUILD_SOAK_TESTS=ON they are registered
#   once more under the CTest label soak, with LUMEX_TEST_SOAK=1 in their
#   environment and the name <directory prefix>soak.<Suite>.<Test><suffix>:
#   `ctest -L soak`. LUMEX_TEST_SOAK_SECONDS sets their length.
# - TARGETS_VAR receives the names of the created targets, for options the
#   helper does not cover (compile or link flags of one module). Add sources
#   through SOURCES, not afterwards: lumex_test_use_gtest silences warnings
#   for the sources the target has when it runs.
#
# One call per directory.
function(lumex_add_standard_suites component)
  if(NOT COMMAND lumex_test_standards_select
     OR NOT DEFINED LUMEX_TEST_STANDARD_MODULES)
    message(FATAL_ERROR
      "lumex_add_standard_suites: include lumex/tests/LumexTestStandards.cmake first")
  endif()
  if(NOT component MATCHES "^[A-Z][A-Za-z0-9]*$" OR component MATCHES "^Lumex")
    message(FATAL_ERROR
      "lumex_add_standard_suites: '${component}' must be a CamelCase "
      "component name without the Lumex prefix (Base64, not LumexBase64)")
  endif()

  get_property(_previous DIRECTORY PROPERTY LUMEX_STANDARD_SUITES_COMPONENT)
  if(_previous)
    message(FATAL_ERROR
      "lumex_add_standard_suites(${component}): this directory already "
      "called it for ${_previous}; one call per directory builds one "
      "executable per standard")
  endif()
  set_property(DIRECTORY PROPERTY LUMEX_STANDARD_SUITES_COMPONENT "${component}")

  # Split the arguments into the common ones and one group per VARIANT. A
  # group is VARIANT <name> [DEFINITIONS ...]; any other keyword of the
  # helper ends it, so the common arguments may also follow the variants
  # (except DEFINITIONS, which inside a group belongs to the variant).
  set(_common_keywords MODULE LINK SOURCES EXCLUDE DISCOVER_ARGS
    PLAIN_EXECUTABLE TARGETS_VAR SOAK_FILTER)
  set(_group _main)
  set(_args__main "")
  set(_variants "")
  set(_after_variant FALSE)
  foreach(_arg IN LISTS ARGN)
    if(_arg STREQUAL "VARIANT")
      set(_after_variant TRUE)
      continue()
    endif()
    if(_arg IN_LIST _common_keywords)
      set(_group _main)
    endif()
    if(_after_variant)
      if(NOT _arg MATCHES "^[a-z][a-z0-9]*(_[a-z0-9]+)*$")
        message(FATAL_ERROR
          "lumex_add_standard_suites(${component}): VARIANT needs a "
          "snake_case name, got '${_arg}'")
      endif()
      if(_arg IN_LIST _variants)
        message(FATAL_ERROR
          "lumex_add_standard_suites(${component}): VARIANT ${_arg} is given twice")
      endif()
      list(APPEND _variants "${_arg}")
      set(_group "${_arg}")
      set(_args_${_group} "")
      set(_after_variant FALSE)
      continue()
    endif()
    list(APPEND _args_${_group} "${_arg}")
  endforeach()
  if(_after_variant)
    message(FATAL_ERROR
      "lumex_add_standard_suites(${component}): VARIANT without a name")
  endif()

  cmake_parse_arguments(ARG "PLAIN_EXECUTABLE" "MODULE;TARGETS_VAR;SOAK_FILTER"
    "LINK;SOURCES;DEFINITIONS;EXCLUDE;DISCOVER_ARGS" ${_args__main})
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "lumex_add_standard_suites(${component}): unexpected arguments: "
      "${ARG_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT ARG_MODULE)
    message(FATAL_ERROR
      "lumex_add_standard_suites(${component}): MODULE <key> is required")
  endif()
  # The calling directory is the module's own test directory or one below it.
  lumex_test_name(_directory_prefix "")
  string(LENGTH "${ARG_MODULE}." _module_prefix_length)
  string(SUBSTRING "${_directory_prefix}" 0 ${_module_prefix_length}
    _directory_head)
  if(NOT _directory_head STREQUAL "${ARG_MODULE}.")
    message(FATAL_ERROR
      "lumex_add_standard_suites(${component}): MODULE ${ARG_MODULE} is not "
      "the module of ${CMAKE_CURRENT_SOURCE_DIR}: its CTest prefix is "
      "'${_directory_prefix}', which must begin with '${ARG_MODULE}.'")
  endif()
  if("TEST_SUFFIX" IN_LIST ARG_DISCOVER_ARGS)
    message(FATAL_ERROR
      "lumex_add_standard_suites(${component}): TEST_SUFFIX is set by the "
      "helper (.cxx<std>), do not pass it in DISCOVER_ARGS")
  endif()
  if(ARG_PLAIN_EXECUTABLE AND ARG_DISCOVER_ARGS)
    message(FATAL_ERROR
      "lumex_add_standard_suites(${component}): DISCOVER_ARGS has no effect "
      "with PLAIN_EXECUTABLE")
  endif()

  foreach(_variant IN LISTS _variants)
    cmake_parse_arguments(_VAR "" "" "DEFINITIONS" ${_args_${_variant}})
    if(_VAR_UNPARSED_ARGUMENTS)
      message(FATAL_ERROR
        "lumex_add_standard_suites(${component}): unexpected arguments of "
        "VARIANT ${_variant}: ${_VAR_UNPARSED_ARGUMENTS}")
    endif()
    set(_definitions_${_variant} ${_VAR_DEFINITIONS})
  endforeach()

  # The table decides the standards; check that the call and the table name
  # the same variants.
  lumex_test_standards_get(_module_standards "${ARG_MODULE}")
  set(_declared ${LUMEX_TEST_STANDARD_VARIANTS_${ARG_MODULE}})
  foreach(_variant IN LISTS _declared)
    if(NOT _variant IN_LIST _variants)
      message(FATAL_ERROR
        "lumex_add_standard_suites(${component}): the table declares variant "
        "'${_variant}' of '${ARG_MODULE}'; pass VARIANT ${_variant}")
    endif()
  endforeach()
  foreach(_variant IN LISTS _variants)
    lumex_test_standards_get(_variant_standards_${_variant} "${ARG_MODULE}"
      VARIANT "${_variant}")
  endforeach()

  file(GLOB _found RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.tests.cpp")
  if(NOT _found)
    message(FATAL_ERROR
      "lumex_add_standard_suites(${component}): no *.tests.cpp in "
      "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  foreach(_excluded IN LISTS ARG_EXCLUDE)
    if(NOT _excluded IN_LIST _found)
      message(FATAL_ERROR
        "lumex_add_standard_suites(${component}): EXCLUDE names "
        "'${_excluded}', which is not a *.tests.cpp of this directory")
    endif()
  endforeach()

  set(_targets "")
  set(_unsupported 0)
  foreach(_suite IN ITEMS _main ${_variants})
    if(_suite STREQUAL "_main")
      set(_standards ${_module_standards})
      set(_name_part "")
      set(_suffix_part "")
      set(_definitions ${ARG_DEFINITIONS})
    else()
      set(_standards ${_variant_standards_${_suite}})
      string(REPLACE "_" ";" _words "${_suite}")
      set(_name_part "")
      foreach(_word IN LISTS _words)
        string(SUBSTRING "${_word}" 0 1 _first)
        string(SUBSTRING "${_word}" 1 -1 _rest)
        string(TOUPPER "${_first}" _first)
        string(APPEND _name_part "${_first}${_rest}")
      endforeach()
      set(_suffix_part ".${_suite}")
      set(_definitions ${ARG_DEFINITIONS} ${_definitions_${_suite}})
    endif()

    foreach(_std IN LISTS _standards)
      set(_target "Lumex${component}${_name_part}Cxx${_std}Tests")
      if(_std GREATER LUMEX_TEST_STANDARDS_OPTIONAL_ABOVE
         AND NOT "cxx_std_${_std}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        message(STATUS
          "Skipping ${_target}: ${CMAKE_CXX_COMPILER_ID} "
          "${CMAKE_CXX_COMPILER_VERSION} has no C++${_std} "
          "(cxx_std_${_std} is not in CMAKE_CXX_COMPILE_FEATURES)")
        math(EXPR _unsupported "${_unsupported} + 1")
        continue()
      endif()

      # The files of this standard and of the lower ones; none, or none left
      # after EXCLUDE, means the directory has no suite of this standard.
      lumex_test_standards_select(_sources "${ARG_MODULE}" ${_std} ${_found})
      if(ARG_EXCLUDE AND _sources)
        list(REMOVE_ITEM _sources ${ARG_EXCLUDE})
      endif()
      if(NOT _sources)
        message(VERBOSE
          "Skipping ${_target}: no test source of C++${_std} or lower in "
          "${CMAKE_CURRENT_SOURCE_DIR}")
        continue()
      endif()

      add_executable(${_target} ${_sources} ${ARG_SOURCES})
      if(ARG_LINK)
        target_link_libraries(${_target} PRIVATE ${ARG_LINK})
      endif()
      if(_definitions)
        target_compile_definitions(${_target} PRIVATE ${_definitions})
      endif()
      lumex_test_use_gtest(${_target} CXX_STANDARD ${_std})

      set(_suffix "${_suffix_part}.cxx${_std}")
      if(ARG_PLAIN_EXECUTABLE)
        lumex_test_name(_ctest_name "Lumex${component}Tests${_suffix}")
        add_test(NAME ${_ctest_name} COMMAND ${_target})
      else()
        # The soak tests (SOAK_FILTER) leave the ordinary registration where
        # gtest_discover_tests can filter (CMake 3.22) and are registered
        # again under the label soak when LUMEX_BUILD_SOAK_TESTS is ON.
        set(_filter_args "")
        if(ARG_SOAK_FILTER AND NOT CMAKE_VERSION VERSION_LESS 3.22)
          set(_filter_args TEST_FILTER "-${ARG_SOAK_FILTER}")
        endif()
        lumex_gtest_discover_tests(${_target}
          TEST_SUFFIX "${_suffix}"
          ${_filter_args}
          ${ARG_DISCOVER_ARGS})
        if(ARG_SOAK_FILTER AND LUMEX_BUILD_SOAK_TESTS
           AND NOT CMAKE_VERSION VERSION_LESS 3.22)
          lumex_test_name(_soak_prefix "soak.")
          lumex_gtest_discover_tests(${_target}
            TEST_PREFIX "${_soak_prefix}"
            TEST_SUFFIX "${_suffix}"
            TEST_FILTER "${ARG_SOAK_FILTER}"
            TEST_LIST ${_target}_soak_tests
            PROPERTIES LABELS soak ENVIRONMENT LUMEX_TEST_SOAK=1 TIMEOUT 3600)
        endif()
      endif()
      list(APPEND _targets ${_target})
    endforeach()
  endforeach()

  if(NOT _targets AND _unsupported EQUAL 0)
    message(FATAL_ERROR
      "lumex_add_standard_suites(${component}): no suite was built in "
      "${CMAKE_CURRENT_SOURCE_DIR}; every test source is excluded or newer "
      "than every standard of the module")
  endif()

  if(ARG_TARGETS_VAR)
    set(${ARG_TARGETS_VAR} ${_targets} PARENT_SCOPE)
  endif()
endfunction()
