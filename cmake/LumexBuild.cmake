# cmake/LumexBuild.cmake
#
# Wraps CMakeRoutines for LumexLib compiled targets. Include after project()
# and cmake/LumexOptions.cmake. Call lumex_configure_all_compiled_targets()
# after every add_subdirectory so newly created STATIC/SHARED/OBJECT/EXECUTABLE
# targets get warnings + PORTABLE (or the selected) optimization flags.
# When any LUMEX_USE_*SAN is ON, the same CMakeRoutines sanitizer flags
# land on libraries, examples, test executables, and vendored gtest.
# Release/MinSizeRel + any sanitizer is a WARNING (RelWithDebInfo minimum,
# Debug recommended). VersionConfig, BuildInfoPrinter, StaticAnalysis,
# MaximumStandardCompliance, and utils (CompileCommands, BuildTiming,
# GenerateBuildInfo, WarningSuppression, RecursiveSourceCollection) are
# included here. MsvcVcvarsConfig must stay in the root CMakeLists.txt
# before project().
#
# Do not pass STANDARD to configure_compiler_flags: a project-wide C++
# standard breaks base64 Decoder/Validator (#if __cplusplus signature split).
# Do not include HardwareOptimization.cmake: its defaults enable native/AVX.
# Do not replace cmake/LibraryVersioning.cmake with the CMakeRoutines copy:
# the local file has the clang-cl + Ninja rc.exe / EXTERNAL_OBJECT workaround.

include_guard(GLOBAL)

include(core/CompilerFlags)
include(core/WindowsVersionConfig)
include(core/VersionConfig)
include(optimizations/OptimizationLevelConfig)
include(utils/CompileCommandsConfig)
include(utils/BuildTiming)
include(utils/GenerateBuildInfo)
include(utils/WarningSuppression)
include(utils/RecursiveSourceCollection)
include(testing/SanitizersConfig)
include(analysis/StaticAnalysisConfig)
include(analysis/MaximumStandardCompliance)
include(deployment/BuildInfoPrinter)
include("${CMAKE_CURRENT_LIST_DIR}/LumexSanitizerBuildType.cmake")

set(_LUMEX_CMAKE_ROUTINES_UTILS_DIR
  "${PROJECT_SOURCE_DIR}/CMakeRoutines/utils")

configure_version(VERSION "${PROJECT_VERSION}")

if(LUMEX_USE_ASAN AND LUMEX_USE_TSAN)
  message(FATAL_ERROR
    "LUMEX_USE_ASAN and LUMEX_USE_TSAN cannot be used together "
    "(CMakeRoutines SanitizersConfig: shadow memory conflict). "
    "Use separate build trees.")
endif()

if(WIN32 AND LUMEX_USE_TSAN)
  message(FATAL_ERROR
    "LUMEX_USE_TSAN is not supported on Windows. "
    "ThreadSanitizer requires Linux or macOS (GCC/Clang).")
endif()

function(lumex_sanitizers_enabled out_var)
  if(LUMEX_USE_ASAN OR LUMEX_USE_UBSAN OR LUMEX_USE_TSAN)
    set(${out_var} TRUE PARENT_SCOPE)
  else()
    set(${out_var} FALSE PARENT_SCOPE)
  endif()
endfunction()

# Same ADDRESS/UNDEFINED/THREAD mix on every compiled target, including
# test executables and vendored gtest. Mixing sanitized tests with
# unsanitized libs (or the reverse) is an ABI / false-positive hole.
function(lumex_apply_sanitizers target_name)
  if(NOT TARGET "${target_name}")
    return()
  endif()

  get_target_property(_aliased "${target_name}" ALIASED_TARGET)
  if(_aliased)
    return()
  endif()

  get_target_property(_imported "${target_name}" IMPORTED)
  if(_imported)
    return()
  endif()

  get_target_property(_type "${target_name}" TYPE)
  if(_type STREQUAL "INTERFACE_LIBRARY" OR _type STREQUAL "UTILITY")
    return()
  endif()

  lumex_sanitizers_enabled(_on)
  if(NOT _on)
    return()
  endif()

  get_target_property(_done "${target_name}" LUMEX_SANITIZERS_CONFIGURED)
  if(_done)
    return()
  endif()

  # clang-cl sets MSVC=TRUE, so configure_sanitizers takes the MSVC
  # branch: ASan may land as /fsanitize=address (LLVM version >= 19.29
  # happens to pass that check), but UBSan is a warning and no flag.
  # Drive the Clang implementation so ASan and UBSan both compile in.
  if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    _configure_clang_sanitizers("${target_name}"
      "${LUMEX_USE_ASAN}" OFF "${LUMEX_USE_TSAN}" "${LUMEX_USE_UBSAN}"
      OFF OFF "" "" "")
  else()
    configure_sanitizers("${target_name}"
      ADDRESS "${LUMEX_USE_ASAN}"
      UNDEFINED "${LUMEX_USE_UBSAN}"
      THREAD "${LUMEX_USE_TSAN}")
  endif()

  # MSVC / clang-cl ASan without debug info emits C5072; reports need symbols.
  if(MSVC AND LUMEX_USE_ASAN)
    target_compile_options("${target_name}" PRIVATE /Zi)
    target_link_options("${target_name}" PRIVATE /DEBUG)
  endif()

  set_property(TARGET "${target_name}" PROPERTY LUMEX_SANITIZERS_CONFIGURED TRUE)
endfunction()

function(lumex_apply_static_analysis target_name)
  if(NOT LUMEX_USE_CLANG_TIDY AND NOT LUMEX_USE_CPPCHECK)
    return()
  endif()

  get_target_property(_done "${target_name}" LUMEX_STATIC_ANALYSIS_CONFIGURED)
  if(_done)
    return()
  endif()

  configure_static_analysis("${target_name}"
    CLANG_TIDY "${LUMEX_USE_CLANG_TIDY}"
    CPPCHECK "${LUMEX_USE_CPPCHECK}"
    PROFILE "${LUMEX_STATIC_ANALYSIS_PROFILE}"
    WARN_AS_ERROR OFF
    HEADER_FILTER "lumex/")

  set_property(TARGET "${target_name}" PROPERTY
    LUMEX_STATIC_ANALYSIS_CONFIGURED TRUE)
endfunction()

# GNU/Clang only. The routine always writes CXX_STANDARD; Lumex must not
# invent one for library TUs that select API shape via #if __cplusplus.
# Skip targets that have not already pinned a standard (tests do).
function(lumex_apply_maximum_standard_compliance target_name)
  if(NOT LUMEX_MAXIMUM_STANDARD_COMPLIANCE)
    return()
  endif()

  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    return()
  endif()

  get_target_property(_std "${target_name}" CXX_STANDARD)
  if(NOT _std)
    return()
  endif()

  get_target_property(_done "${target_name}" LUMEX_MAX_STD_CONFIGURED)
  if(_done)
    return()
  endif()

  maximum_standard_compliance_configure(
    TARGET "${target_name}"
    CXX_STANDARD "${_std}"
    WERROR OFF
    PEDANTIC_ERRORS OFF
    DEPENDENCY_WARNING_MODE RELAX_VENDOR)

  set_property(TARGET "${target_name}" PROPERTY LUMEX_MAX_STD_CONFIGURED TRUE)
endfunction()

# Opt-in per-library build-info (configure-time stub + POST_BUILD with
# binary hash). Output next to TARGET_FILE so PublishDistr can stage
# Lumex*-build-info.json when the format is json.
function(lumex_apply_build_info target_name)
  if(NOT LUMEX_GENERATE_BUILD_INFO)
    return()
  endif()

  if(NOT TARGET "${target_name}")
    return()
  endif()

  if(NOT target_name MATCHES "^Lumex")
    return()
  endif()
  if(target_name MATCHES "Tests$" OR target_name MATCHES "Example")
    return()
  endif()

  get_target_property(_type "${target_name}" TYPE)
  if(NOT _type STREQUAL "SHARED_LIBRARY" AND NOT _type STREQUAL "STATIC_LIBRARY"
      AND NOT _type STREQUAL "MODULE_LIBRARY")
    return()
  endif()

  get_target_property(_done "${target_name}" LUMEX_BUILD_INFO_CONFIGURED)
  if(_done)
    return()
  endif()

  string(TOLOWER "${LUMEX_BUILD_INFO_FORMAT}" _lumex_bif_fmt)
  set(BUILD_INFO_FORMAT "${_lumex_bif_fmt}")

  set(_props_file
    "${CMAKE_BINARY_DIR}/${target_name}-target-properties.cmake")
  set(_cfg_out
    "${CMAKE_BINARY_DIR}/${target_name}-build-info.${_lumex_bif_fmt}")

  if(COMMAND save_target_properties)
    save_target_properties("${target_name}" "${_props_file}")
  endif()
  if(COMMAND generate_build_info_file)
    generate_build_info_file("${target_name}" "${_cfg_out}")
  endif()

  set(_script
    "${_LUMEX_CMAKE_ROUTINES_UTILS_DIR}/GenerateBuildInfoScript.cmake")
  set(_py_script
    "${_LUMEX_CMAKE_ROUTINES_UTILS_DIR}/GenerateBuildInfo.py")
  if(EXISTS "${_script}")
    add_custom_command(
      TARGET "${target_name}"
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
        -Dtarget_name=${target_name}
        -Doutput_file=$<TARGET_FILE_DIR:${target_name}>/${target_name}-build-info.${_lumex_bif_fmt}
        -Dtarget_properties_file=${_props_file}
        -DCMAKE_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DCMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}
        -Dpython_executable=${GBINFO_PYTHON_EXECUTABLE}
        -Dpython_script=${_py_script}
        -Dbuild_info_format=${_lumex_bif_fmt}
        -Dtarget_binary_path=$<TARGET_FILE:${target_name}>
        -P "${_script}"
      COMMENT
        "Regenerating build info (${_lumex_bif_fmt}) for ${target_name}"
      VERBATIM)
  endif()

  set_property(TARGET "${target_name}" PROPERTY
    LUMEX_BUILD_INFO_CONFIGURED TRUE)
endfunction()

function(lumex_configure_target target_name)
  if(NOT TARGET "${target_name}")
    return()
  endif()

  get_target_property(_aliased "${target_name}" ALIASED_TARGET)
  if(_aliased)
    return()
  endif()

  get_target_property(_imported "${target_name}" IMPORTED)
  if(_imported)
    return()
  endif()

  get_target_property(_type "${target_name}" TYPE)
  if(_type STREQUAL "INTERFACE_LIBRARY" OR _type STREQUAL "UTILITY")
    return()
  endif()

  if(target_name MATCHES "^lumex_gtest"
      OR target_name MATCHES "^compile_commands"
      OR target_name STREQUAL "publish_distr"
      OR target_name STREQUAL "generate_documentation"
      OR target_name STREQUAL "lumex_copy_compile_commands")
    return()
  endif()

  get_target_property(_done "${target_name}" LUMEX_BUILD_CONFIGURED)
  if(_done)
    return()
  endif()

  # WARNINGS HIGH only. STANDARD is intentionally omitted. Test/example
  # targets mark themselves for full suppression (LUMEX_SUPPRESS_ALL_WARNINGS)
  # and get WARNINGS OFF instead, so there is no /W4 for the per-source /w to
  # override (MSVC D9025).
  get_target_property(_suppress_all "${target_name}" LUMEX_SUPPRESS_ALL_WARNINGS)
  if(_suppress_all)
    configure_compiler_flags("${target_name}" WARNINGS OFF)
  else()
    configure_compiler_flags("${target_name}" WARNINGS HIGH)
  endif()
  configure_optimization_level("${target_name}"
    LEVEL "${LUMEX_OPTIMIZATION_LEVEL}"
    ENABLE_LTO "${LUMEX_ENABLE_LTO}"
    DEBUG_SYMBOLS "${LUMEX_DEBUG_SYMBOLS}")

  # CMakeRoutines configure_compiler_flags adds NOMINMAX only on the MSVC
  # path (clang-cl). GNU-like clang++ / g++ on Windows still see min/max
  # macros from the Windows SDK and break std::min / std::max (e.g.
  # LumexLogger). Always define both for every compiled Lumex target.
  if(WIN32)
    target_compile_definitions("${target_name}" PRIVATE
      NOMINMAX
      WIN32_LEAN_AND_MEAN)
  endif()

  # C4251: std members in dllexport classes (Logger, Timer). Pimpl is out
  # of scope; clients never need those members' layout to be exported.
  # /EHa must be last on LumexExceptionsTests so HIGH's /EHsc does not
  # win (MSVC C4535 on _set_se_translator).
  if(MSVC)
    target_compile_options("${target_name}" PRIVATE /wd4251)
    # /external:* is an MSVC cl.exe feature. clang-cl ignores
    # /external:anglebrackets and warns -Wunused-command-line-argument.
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      target_compile_options("${target_name}" PRIVATE
        /external:W0
        /external:anglebrackets)
    endif()
    if(target_name STREQUAL "LumexExceptionsTests")
      # HIGH injects /EHsc as a target option. Replace it so /EHa is the
      # only EH model (MSVC D9025 + C4535 on _set_se_translator).
      get_target_property(_lumex_eh_opts "${target_name}" COMPILE_OPTIONS)
      if(_lumex_eh_opts)
        string(REPLACE "/EHsc" "/EHa" _lumex_eh_opts "${_lumex_eh_opts}")
        string(REPLACE "/EHs;" "/EHa;" _lumex_eh_opts "${_lumex_eh_opts}")
        set_property(TARGET "${target_name}" PROPERTY COMPILE_OPTIONS
                     "${_lumex_eh_opts}")
      endif()
      target_compile_options("${target_name}" PRIVATE /EHa)
    endif()
  endif()

  lumex_apply_sanitizers("${target_name}")
  lumex_apply_static_analysis("${target_name}")
  lumex_apply_maximum_standard_compliance("${target_name}")
  lumex_apply_build_info("${target_name}")

  # Tests and examples silence their own warnings from their own directory:
  # lumex_test_use_gtest (cmake/LumexGoogleTest.cmake) and
  # lumex_example_executable (lumex/examples/cmake/LumexExampleHelpers.cmake)
  # call CMakeRoutines `suppress_warnings_for_sources`. It cannot be called
  # from here: CMake scopes source-file properties to the directory that sets
  # them, so the call would not reach targets declared under lumex/tests or
  # lumex/examples.

  set_property(TARGET "${target_name}" PROPERTY LUMEX_BUILD_CONFIGURED TRUE)
endfunction()

function(lumex_configure_targets_in_dir dir)
  get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_t IN LISTS _targets)
    lumex_configure_target("${_t}")
  endforeach()
  get_property(_subs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  foreach(_s IN LISTS _subs)
    lumex_configure_targets_in_dir("${_s}")
  endforeach()
endfunction()

function(lumex_configure_all_compiled_targets)
  lumex_configure_targets_in_dir("${PROJECT_SOURCE_DIR}")

  # lumex_configure_target skips lumex_gtest_* (they already use /W0 / -w).
  # Sanitizer flags still have to land on those static libs whenever
  # LUMEX_BUILD_TESTS and any LUMEX_USE_*SAN are both on.
  foreach(_g lumex_gtest_1_12 lumex_gtest_main_1_12
             lumex_gtest_1_18 lumex_gtest_main_1_18)
    lumex_apply_sanitizers("${_g}")
  endforeach()
endfunction()
