# CTest names of LumexLib tests, derived from the directory that registers
# them, so that `ctest -R '^<module>\.'` selects a module together with every
# subdirectory under it.
#
# A test registered from lumex/tests/<path> is named <prefix><name>. The
# prefix is <path> without a leading core/ or applied/ group, with every "/"
# replaced by "." and a "." appended:
#   lumex/tests/core/crc                         -> crc.
#   lumex/tests/applied/json                     -> json.
#   lumex/tests/xml                              -> xml.
#   lumex/tests/core/generators/number_generator -> generators.number_generator.
#   lumex/tests/cmake                            -> cmake.
# Examples are named from lumex/ as the root instead, so a test registered
# from lumex/examples/atomic gets the prefix examples.atomic.
#
# The file only defines functions and one variable, so it also works in
# script mode (cmake -P), where the CMake-file tests call it directly.

# lumex/ of this checkout. Resolved from this file, not from CMAKE_SOURCE_DIR,
# which names the parent project when LumexLib is embedded.
get_filename_component(LUMEX_TEST_NAMES_LUMEX_DIR
  "${CMAKE_CURRENT_LIST_DIR}/../lumex" ABSOLUTE)

# lumex_test_name_prefix(<out_var> <directory> <root>)
#
# Stores in <out_var> the CTest name prefix of <directory>, which must be
# <root> or a directory under it. <root> itself gives an empty prefix. Only
# the first path component is dropped when it is core or applied.
function(lumex_test_name_prefix out_var directory root)
  get_filename_component(_dir "${directory}" ABSOLUTE)
  get_filename_component(_root "${root}" ABSOLUTE)
  file(RELATIVE_PATH _rel "${_root}" "${_dir}")
  # A directory outside the root gives "../..." (or an absolute path when it
  # is on another Windows drive).
  if(_rel MATCHES "^\\.\\.(/|$)" OR IS_ABSOLUTE "${_rel}")
    message(FATAL_ERROR
      "lumex_test_name_prefix: the directory is not under the root\n"
      "  directory: ${directory}\n"
      "  root: ${root}")
  endif()

  string(REGEX REPLACE "/+$" "" _rel "${_rel}")
  string(REGEX REPLACE "^(core|applied)/" "" _rel "${_rel}")
  if(_rel STREQUAL "")
    set(_prefix "")
  else()
    string(REPLACE "/" "." _prefix "${_rel}.")
  endif()
  set(${out_var} "${_prefix}" PARENT_SCOPE)
endfunction()

# lumex_test_name(<out_var> <name> [DIRECTORY <dir>] [ROOT <dir>]
#                 [TEST_PREFIX <prefix>])
#
# Stores in <out_var> the CTest name <prefix><name>. The prefix is derived by
# lumex_test_name_prefix from DIRECTORY (default: the calling directory,
# CMAKE_CURRENT_SOURCE_DIR) and ROOT (default: lumex/tests of this checkout).
# An explicit TEST_PREFIX, even an empty one, replaces the derived prefix.
# With an empty <name> the result is the prefix alone.
function(lumex_test_name out_var name)
  cmake_parse_arguments(ARG "" "DIRECTORY;ROOT;TEST_PREFIX" "" ${ARGN})
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "lumex_test_name: unexpected arguments: ${ARG_UNPARSED_ARGUMENTS}")
  endif()

  if(DEFINED ARG_TEST_PREFIX OR "TEST_PREFIX" IN_LIST ARG_KEYWORDS_MISSING_VALUES)
    set(_prefix "${ARG_TEST_PREFIX}")
  else()
    if(NOT ARG_DIRECTORY)
      set(ARG_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    if(NOT ARG_ROOT)
      # Set by include() in the including directory and inherited by its
      # subdirectories; a directory outside that tree has to include the file.
      if(NOT LUMEX_TEST_NAMES_LUMEX_DIR)
        message(FATAL_ERROR
          "lumex_test_name: include cmake/LumexTestNames.cmake in this directory")
      endif()
      set(ARG_ROOT "${LUMEX_TEST_NAMES_LUMEX_DIR}/tests")
    endif()
    lumex_test_name_prefix(_prefix "${ARG_DIRECTORY}" "${ARG_ROOT}")
  endif()
  set(${out_var} "${_prefix}${name}" PARENT_SCOPE)
endfunction()
