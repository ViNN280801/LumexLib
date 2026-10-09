# lumex/tests/LumexTestStandards.cmake
#
# The C++ standards every test module is built and run at: one test suite
# (one executable) per module and standard. This file is the only place that
# names a module's standards; a module CMakeLists.txt names its module key and
# gets the standards from here through lumex_add_standard_suites
# (cmake/LumexGoogleTest.cmake).
#
# The file only defines functions and variables, so it also works in script
# mode (cmake -P), where cmake.wiring_standard_suites reads it.
#
# Terms
# -----
# Module: what the table has one entry for: a directory of lumex/core or
#   lumex/applied (math, utility, ...), lumex/xml, and
#   generators/number_generator (generators is not a module of its own).
# Module key: the CTest prefix of the module's own test directory without its
#   trailing dot (cmake/LumexTestNames.cmake): lumex/tests/core/base64 ->
#   base64, lumex/tests/core/generators/number_generator ->
#   generators.number_generator, lumex/tests/xml -> xml.
# Test directory: the test tree follows the source tree. lumex/tests/<path>
#   holds the tests of lumex/<path>: lumex/tests/core/math/ops tests
#   lumex/core/math/ops, lumex/tests/core/utility/traits tests
#   lumex/core/utility/traits. The CTest prefix is the directory's own
#   (math.ops., utility.traits.), so `ctest -R '^math\.ops\.'` selects one
#   source directory and `ctest -R '^math\.'` the module. A module's own
#   directory keeps the tests of its umbrella and of files that sit beside
#   the umbrella, which belong to no subdirectory. Every directory with
#   tests belongs to the module whose key its prefix begins with, and builds
#   the module's standards.
# Test source of a standard: <Stem>.cxx<std>.tests.cpp in the test directory,
#   for example LumexBase64Encoder.cxx17.tests.cpp. <Stem> is a letter
#   followed by letters, digits or underscores (no dots); <std> is one of the
#   module's standards.
# Suite of a standard: the executable Lumex<Component>Cxx<std>Tests of one
#   test directory. It compiles the test sources of its own standard and of
#   every lower standard of the module, so the C++20 suite of base64.encode
#   (11 17 20) runs the .cxx11, .cxx17 and .cxx20 files of that directory.
#   Its CTest names end in .cxx<std>, also at the lowest standard:
#   base64.encode.Base64EncoderTest.GivenSpan_WhenEncode_...cxx20.
#   A directory has no suite of a standard below its lowest file: a directory
#   whose lowest file is a .cxx20 one has the suites from C++20 on. Every
#   directory of utility has a .cxx11 file now (utility.cast and
#   utility.ranges included, since both work from C++11), so each builds all
#   the standards of the module.
# Variant: an extra suite on the same sources with compile definitions, at
#   the standards declared for it here: Lumex<Component><Variant>Cxx<std>Tests
#   and the CTest suffix .<variant>.cxx<std> (atomic: lock_based, wait_table,
#   no_lock_free).
#   Every directory of the module builds its variants.
# Standards above LUMEX_TEST_STANDARDS_OPTIONAL_ABOVE (20) are built only
#   when the compiler supports them (cxx_std_<std> in
#   CMAKE_CXX_COMPILE_FEATURES); otherwise the suite is skipped with a STATUS
#   message (GCC 8.3 has no C++23, GCC 13 no C++26).
#
# How to convert a module
# -----------------------
# 1. Find the module key and its standards in the table below.
# 2. Put every test source in the directory of the lumex/ directory it tests
#    (git mv; a file that tests several directories is split by test suite,
#    with what the parts share in a header in the module's own test
#    directory), and rename and split it by standard: the tests that compile
#    at the lowest standard go into <Stem>.cxx<lowest>.tests.cpp. A test under
#    `#if __cplusplus >= 201703L` (or 202002L, ...) moves into
#    <Stem>.cxx17.tests.cpp (or .cxx20, ...) without the #if; a standard
#    header included only for it (<string_view>, <span>) moves with it,
#    unconditionally. A test under a LUMEX_HAS_* feature check goes into the
#    file of the standard the feature belongs to and keeps the check, with
#    GTEST_SKIP () in the #else branch, because toolchains differ. A negative
#    branch (`#if __cplusplus < 201703L`) stays in the lower file. Platform
#    branches (_WIN32, LUMEX_OS_*) stay where they are. A fixture or helper
#    that the files of several standards use goes into a header next to them
#    (for example LumexBase64TestFixtures.hpp), passed through SOURCES.
#    Suite and test names stay as they were: only the CTest suffix changes.
# 3. A standard of the table may have no file of its own: its suite then runs
#    the files of the lower standards. A directory has no suite below its
#    lowest file. Rename every *.tests.cpp of the directory into the scheme,
#    also the sources of other components in the same directory: they all go
#    into the one executable per standard (see "Several components" below).
# 4. Replace the add_executable / lumex_test_use_gtest /
#    lumex_gtest_discover_tests block of the module CMakeLists.txt with one
#    call (all arguments but <Component> and MODULE are optional):
#      lumex_add_standard_suites(<Component> MODULE <key>
#          LINK <targets...>            # target_link_libraries PRIVATE
#          SOURCES <files...>           # extra files of every suite (headers)
#          DEFINITIONS <defs...>        # compile definitions of every suite
#          EXCLUDE <files...>           # scheme files left out of the build
#          STANDARDS <stds...>          # narrows the module's row for this
#                                       # directory (see below)
#          DISCOVER_ARGS <args...>      # to lumex_gtest_discover_tests
#          PLAIN_EXECUTABLE             # own main (), registered by add_test
#          TARGETS_VAR <var>            # receives the created target names
#          VARIANT <name> DEFINITIONS <defs...>   # repeatable
#      )
#    The module's own test directory has a CMakeLists.txt that adds its
#    subdirectories (and calls the function too when it has test sources of
#    its own); a directory below it calls the function with the module's key:
#    MODULE base64 in lumex/tests/core/base64/encode, where the CTest prefix
#    is base64.encode.
#    Examples:
#      One standard (environment: 11, directory env):
#        lumex_add_standard_suites(EnvironmentEnv MODULE environment
#            LINK lumex::environment)
#      Several standards (base64: 11 17 20, directory encode):
#        lumex_add_standard_suites(Base64Encode MODULE base64
#            LINK lumex::base64 SOURCES ../LumexBase64TestFixtures.hpp)
#      Several components in one directory (utility/traits): every
#        LumexTypeTraits.cxx11.tests.cpp, LumexStreamTraits.cxx14.tests.cpp,
#        ... goes into LumexUtilityTraitsCxx<std>Tests:
#        lumex_add_standard_suites(UtilityTraits MODULE utility
#            LINK lumex::utility "$<$<PLATFORM_ID:Windows>:dbghelp>"
#            DISCOVER_ARGS DISCOVERY_TIMEOUT 60)
#      Variants (atomic: lock_based, wait_table and no_lock_free, declared below; every
#        directory of atomic passes them):
#        lumex_add_standard_suites(AtomicSmartPtr MODULE atomic
#            LINK lumex::atomic
#            DISCOVER_ARGS PROPERTIES TIMEOUT 300
#            VARIANT lock_based
#                DEFINITIONS LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED
#            VARIANT wait_table DEFINITIONS LUMEX_ATOMIC_WAIT_FORCE_TABLE)
#      Not a GoogleTest binary (number_generator), CTest name
#        generators.number_generator.LumexNumberGeneratorTests.cxx11:
#        lumex_add_standard_suites(NumberGenerator
#            MODULE generators.number_generator
#            LINK lumex::number_generator PLAIN_EXECUTABLE)
#      Options per target (exceptions: -g, -rdynamic):
#        lumex_add_standard_suites(ExceptionsException MODULE exceptions
#            LINK lumex::exceptions TARGETS_VAR _exceptions_targets)
#        foreach(_target IN LISTS _exceptions_targets)
#          target_link_options(${_target} PRIVATE -rdynamic)
#        endforeach()
#    <Component> is the executable name part without the Lumex prefix, the
#    module's name and the directory below it in CamelCase (Base64Encode ->
#    LumexBase64EncodeCxx11Tests); it is unique in the tree. One call per
#    directory: the helper stops with an error on a second call, a directory
#    without a test source, a file outside the scheme, a file of a standard
#    the table does not list for the module, a MODULE that does not begin the
#    directory's CTest prefix, or a module key or variant missing from the
#    table.
# 5. Configure and run `ctest -R '^<key>\.'` and `ctest -L cmake`. Compare the test names
#    before and after: every old name must still exist with the suffix of
#    its suite (an old unsuffixed name gets .cxx<lowest>) and the directory
#    segment of its source directory after the module key, and new suites
#    may only add names.
#
# Narrowing the row of one directory (STANDARDS)
# ----------------------------------------------
# A directory builds the module's whole row by default. A directory whose
# tests and code under test do not change with the standard (no __cplusplus,
# LUMEX_HAS_* or __cpp_* branch in the files of the directory or in the
# sources it tests, no test file of a standard between two others) names the
# standards that matter with STANDARDS, normally 11 17 20: C++11 is the floor,
# 17 is where the vendored GoogleTest changes (1.12.1 below, 1.18.0 from 17)
# and 20 is the last standard every toolchain has. The sources of a suite do
# not change: it still compiles the files of its standard and of the lower
# ones that exist. The list must be a strictly ascending subset of the
# module's row, start at its lowest standard and keep the standard of every
# test file of the directory (a file of a standard left out would not be
# built); it may not equal the row (then it only repeats the table) and it is
# not allowed with a VARIANT. Measure before narrowing: preprocess the
# directory's test files at the standard to drop and at the one below it
# (the same compile command, only -std= changes) and compare the text that
# comes from the directory's own files and from the sources it tests; when it
# is identical the standard changes nothing there. cmake.wiring_standard_suites
# also refuses a dropped standard that a __cplusplus threshold, a feature
# macro or a file name of those files names. Put the reason in the
# directory's CMakeLists.txt. Narrowing never removes a test: every test still
# runs at the standards kept.
#
# Several components in one directory
# -----------------------------------
# The helper takes every *.cxx<std>.tests.cpp of the directory, whatever its
# stem, so a directory with several components (utility/traits: TypeTraits,
# TypeTraitsTopics, RangeTraits, StreamTraits) still builds ONE executable
# per standard, named after the directory's <Component>
# (LumexUtilityTraitsCxx11Tests, LumexUtilityTraitsCxx14Tests, ...). There is
# no second call and no per-component executable.
#
# What cmake.wiring_standard_suites checks
# ----------------------------------------
# - Every test directory (a directory under lumex/tests with *.tests.cpp,
#   other than cmake/ and support/) belongs to a table entry (the longest
#   module key its CTest prefix begins with), and every entry has a test
#   directory of its own or below it.
# - The test tree follows the source tree: every test directory has a
#   directory of the same path under lumex/.
# - Every test directory calls lumex_add_standard_suites exactly once with
#   MODULE <the key of its entry>, and calls none of add_executable, add_test,
#   lumex_test_use_gtest and lumex_gtest_discover_tests itself.
# - Every *.tests.cpp of a converted directory follows the scheme with a
#   standard of the module's entry, and some directory of the module has a
#   file of the module's lowest standard.
# - Every variant of the table is passed as VARIANT <name> in the CMakeLists.txt
#   of every directory of the module, and every VARIANT there is in the table.

# Every C++ standard a test suite may name, in ascending order.
set(LUMEX_TEST_STANDARDS_KNOWN 11 14 17 20 23 26)

# A standard above this one is built only when the compiler supports it.
set(LUMEX_TEST_STANDARDS_OPTIONAL_ABOVE 20)

# Module keys in declaration order; filled by lumex_test_standards_declare.
set(LUMEX_TEST_STANDARD_MODULES "")

# lumex_test_standards_declare(<key> <std>...)
#
# Declares the standards of the test module <key>: known standards in
# strictly ascending order. Sets LUMEX_TEST_STANDARDS_<key> and appends <key>
# to LUMEX_TEST_STANDARD_MODULES in the calling scope.
function(lumex_test_standards_declare key)
  if(NOT key MATCHES "^[a-z][a-z0-9_]*(\\.[a-z][a-z0-9_]*)*$")
    message(FATAL_ERROR
      "lumex_test_standards_declare: '${key}' is not a module key "
      "(the CTest prefix of the directory without its trailing dot)")
  endif()
  if(key IN_LIST LUMEX_TEST_STANDARD_MODULES)
    message(FATAL_ERROR
      "lumex_test_standards_declare: module '${key}' is declared twice")
  endif()
  _lumex_test_standards_check_list("${key}" ${ARGN})
  set(LUMEX_TEST_STANDARDS_${key} ${ARGN} PARENT_SCOPE)
  set(LUMEX_TEST_STANDARD_VARIANTS_${key} "" PARENT_SCOPE)
  set(_modules ${LUMEX_TEST_STANDARD_MODULES} "${key}")
  set(LUMEX_TEST_STANDARD_MODULES ${_modules} PARENT_SCOPE)
endfunction()

# lumex_test_standards_declare_variant(<key> <variant> <std>...)
#
# Declares a variant suite of the module <key> (declared before) at the given
# standards, each of which must be a standard of the module. <variant> is
# snake_case; the executable gets its CamelCase form, the CTest suffix the
# name itself (.<variant>.cxx<std>).
function(lumex_test_standards_declare_variant key variant)
  if(NOT key IN_LIST LUMEX_TEST_STANDARD_MODULES)
    message(FATAL_ERROR
      "lumex_test_standards_declare_variant: module '${key}' is not declared")
  endif()
  if(NOT variant MATCHES "^[a-z][a-z0-9]*(_[a-z0-9]+)*$")
    message(FATAL_ERROR
      "lumex_test_standards_declare_variant: variant '${variant}' of "
      "'${key}' is not snake_case")
  endif()
  if(variant IN_LIST LUMEX_TEST_STANDARD_VARIANTS_${key})
    message(FATAL_ERROR
      "lumex_test_standards_declare_variant: variant '${variant}' of "
      "'${key}' is declared twice")
  endif()
  _lumex_test_standards_check_list("${key}/${variant}" ${ARGN})
  foreach(_std IN LISTS ARGN)
    if(NOT _std IN_LIST LUMEX_TEST_STANDARDS_${key})
      message(FATAL_ERROR
        "lumex_test_standards_declare_variant: C++${_std} of variant "
        "'${variant}' is not a standard of '${key}' "
        "(${LUMEX_TEST_STANDARDS_${key}})")
    endif()
  endforeach()
  set(LUMEX_TEST_STANDARDS_${key}/${variant} ${ARGN} PARENT_SCOPE)
  set(_variants ${LUMEX_TEST_STANDARD_VARIANTS_${key}} "${variant}")
  set(LUMEX_TEST_STANDARD_VARIANTS_${key} ${_variants} PARENT_SCOPE)
endfunction()

# Fails unless ARGN is a non-empty, strictly ascending list of known
# standards. <what> names the entry in the message.
function(_lumex_test_standards_check_list what)
  if(NOT ARGN)
    message(FATAL_ERROR "lumex/tests/LumexTestStandards.cmake: '${what}' has no standard")
  endif()
  set(_previous 0)
  foreach(_std IN LISTS ARGN)
    if(NOT _std IN_LIST LUMEX_TEST_STANDARDS_KNOWN)
      message(FATAL_ERROR
        "lumex/tests/LumexTestStandards.cmake: '${what}' names C++${_std}; "
        "known standards are ${LUMEX_TEST_STANDARDS_KNOWN}")
    endif()
    if(NOT _std GREATER _previous)
      message(FATAL_ERROR
        "lumex/tests/LumexTestStandards.cmake: the standards of '${what}' "
        "are not strictly ascending (${ARGN})")
    endif()
    set(_previous ${_std})
  endforeach()
endfunction()

# lumex_test_standards_get(<out_var> <key> [VARIANT <variant>])
#
# Stores in <out_var> the standards of the module <key>, or of its variant.
# Fails when the table has no such entry.
function(lumex_test_standards_get out_var key)
  cmake_parse_arguments(ARG "" "VARIANT" "" ${ARGN})
  if(NOT key IN_LIST LUMEX_TEST_STANDARD_MODULES)
    message(FATAL_ERROR
      "lumex/tests/LumexTestStandards.cmake has no module '${key}'")
  endif()
  if(ARG_VARIANT)
    if(NOT ARG_VARIANT IN_LIST LUMEX_TEST_STANDARD_VARIANTS_${key})
      message(FATAL_ERROR
        "lumex/tests/LumexTestStandards.cmake has no variant "
        "'${ARG_VARIANT}' of '${key}'")
    endif()
    set(${out_var} ${LUMEX_TEST_STANDARDS_${key}/${ARG_VARIANT}} PARENT_SCOPE)
  else()
    set(${out_var} ${LUMEX_TEST_STANDARDS_${key}} PARENT_SCOPE)
  endif()
endfunction()

# lumex_test_standards_of_file(<out_var> <file>)
#
# Stores in <out_var> the standard of a test source named
# <Stem>.cxx<std>.tests.cpp (the directory part of <file> is ignored), or an
# empty string when the name does not follow that scheme.
function(lumex_test_standards_of_file out_var file)
  get_filename_component(_name "${file}" NAME)
  set(_std "")
  if(_name MATCHES "^[A-Za-z][A-Za-z0-9_]*\\.cxx([0-9]+)\\.tests\\.cpp$")
    if(CMAKE_MATCH_1 IN_LIST LUMEX_TEST_STANDARDS_KNOWN)
      set(_std "${CMAKE_MATCH_1}")
    endif()
  endif()
  set(${out_var} "${_std}" PARENT_SCOPE)
endfunction()

# lumex_test_standards_select(<out_var> <key> <std> [<file>...])
#
# Given every *.tests.cpp file of a test directory of module <key>, stores in
# <out_var> the files the suite of standard <std> compiles: those of <std>
# and of every lower standard, lower standards first, by name within a
# standard. The result is empty when the directory has no file of <std> or a
# lower standard (no suite). Fails on a file outside the scheme or of a
# standard that is not one of the module's.
function(lumex_test_standards_select out_var key std)
  lumex_test_standards_get(_standards "${key}")
  set(_bad "")
  set(_selected "")
  foreach(_level IN LISTS _standards)
    set(_files_${_level} "")
  endforeach()
  foreach(_file IN LISTS ARGN)
    lumex_test_standards_of_file(_file_std "${_file}")
    if(_file_std STREQUAL "" OR NOT _file_std IN_LIST _standards)
      string(APPEND _bad "  ${_file}\n")
      continue()
    endif()
    list(APPEND _files_${_file_std} "${_file}")
  endforeach()
  if(_bad)
    message(FATAL_ERROR
      "Test sources of '${key}' outside the scheme <Stem>.cxx<std>.tests.cpp "
      "with <std> one of ${_standards}:\n${_bad}")
  endif()
  foreach(_level IN LISTS _standards)
    if(_level GREATER std)
      break()
    endif()
    list(SORT _files_${_level})
    list(APPEND _selected ${_files_${_level}})
  endforeach()
  set(${out_var} ${_selected} PARENT_SCOPE)
endfunction()

# lumex_test_standards_resolve(<out_var> <key> <override> [<file>...])
#
# Stores in <out_var> the standards a test directory of module <key> is built
# at: the module's row, or - when <override> (the directory's STANDARDS list,
# a ;-list; empty for none) is not empty - that list. A directory narrows the
# row when its tests and the code under test do not change with the
# standard, so the suites between the standards that matter would only run
# the same sources again. The list must be strictly ascending, every entry a
# standard of the module, and it must keep the standard of every given test
# source <file>: a file whose standard was left out would not be built.
function(lumex_test_standards_resolve out_var key override)
  lumex_test_standards_get(_row "${key}")
  if("${override}" STREQUAL "")
    set(${out_var} ${_row} PARENT_SCOPE)
    return()
  endif()
  string(REPLACE ";" " " _shown "${override}")
  _lumex_test_standards_check_list("${key} STANDARDS" ${override})
  foreach(_std IN LISTS override)
    if(NOT _std IN_LIST _row)
      message(FATAL_ERROR
        "lumex/tests/LumexTestStandards.cmake: C++${_std} of STANDARDS is "
        "not a standard of '${key}' (${_row})")
    endif()
  endforeach()
  foreach(_file IN LISTS ARGN)
    lumex_test_standards_of_file(_file_std "${_file}")
    if(NOT _file_std STREQUAL "" AND NOT _file_std IN_LIST override)
      message(FATAL_ERROR
        "lumex/tests/LumexTestStandards.cmake: ${_file} is a C++${_file_std} "
        "test source, but STANDARDS (${_shown}) does not build C++${_file_std}")
    endif()
  endforeach()
  if("${override}" STREQUAL "${_row}")
    message(FATAL_ERROR
      "lumex/tests/LumexTestStandards.cmake: STANDARDS (${_shown}) of a "
      "'${key}' directory is the module's row; leave STANDARDS out")
  endif()
  set(${out_var} ${override} PARENT_SCOPE)
endfunction()

# --- The table (user decision 2026-10-03) ---------------------------------

lumex_test_standards_declare(utility 11 14 17 20 23 26)

lumex_test_standards_declare(fmt 11 14 17 20)
lumex_test_standards_declare(reflection 11 14 17 20)
lumex_test_standards_declare(string 11 14 17 20)
lumex_test_standards_declare(logger 11 14 17 20)

lumex_test_standards_declare(crc 11 14 17 20)
lumex_test_standards_declare(hazard_pointer 11 14 17 20)
lumex_test_standards_declare(contracts 11 14 17 20)

lumex_test_standards_declare(atomic 11 17 20)
lumex_test_standards_declare_variant(atomic lock_based 11 20)
lumex_test_standards_declare_variant(atomic wait_table 20)
lumex_test_standards_declare(atomic.dwcas 11 14 17 20)
lumex_test_standards_declare_variant(atomic.dwcas builtin 11 14 17 20)
lumex_test_standards_declare_variant(atomic.dwcas msvc_wrapper 11 14 17 20)
lumex_test_standards_declare_variant(atomic no_lock_free 11 20)
lumex_test_standards_declare_variant(atomic lock_free 11 20)
lumex_test_standards_declare_variant(atomic lock_free_deferred 11 20)
lumex_test_standards_declare_variant(atomic std_backed 20)
lumex_test_standards_declare(base64 11 17 20)
lumex_test_standards_declare(expected 11 14 17 20 23)
lumex_test_standards_declare(json 11 17 20)
lumex_test_standards_declare(xml 11 17 20)
lumex_test_standards_declare(exceptions 11 17 20)
lumex_test_standards_declare(math 11 17 20)
lumex_test_standards_declare(span 11 17 20)

lumex_test_standards_declare(circular_buffer 11 20)
lumex_test_standards_declare(filesystem 11 17 20)

lumex_test_standards_declare(resource_monitor 11)

lumex_test_standards_declare(environment 11)
lumex_test_standards_declare(generators.number_generator 11)
lumex_test_standards_declare(optional 11 14 17 20 23)
lumex_test_standards_declare(string_view 11 17 20)
lumex_test_standards_declare(temporary 11)
lumex_test_standards_declare(time 11)
lumex_test_standards_declare(unicode 11)
lumex_test_standards_declare(hardware 11)
lumex_test_standards_declare(logging 11)
lumex_test_standards_declare(serial 11)
lumex_test_standards_declare(settings 11)
