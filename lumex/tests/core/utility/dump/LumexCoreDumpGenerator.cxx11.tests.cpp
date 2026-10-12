// LumexCoreDumpGenerator.cxx11.tests.cpp
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::core::utility::dump;

// Regression coverage for the is_admin_privileges() UNIX fix: it now also
// treats members of a configurable admin group as admin (in addition to root),
// instead of only checking `getuid() == 0`. The group name is intentionally
// NOT hardcoded by LumexLib - it defaults to empty (group check disabled) and
// must be opted into via set_admin_group_name().

class LumexCoreDumpGeneratorAdminTest : public ::testing::Test
{
protected:
  void
  TearDown () override
  {
    // Reset global state so this test doesn't leak into other tests/processes.
    core_dump_generator::set_admin_group_name ("");
  }
};

TEST_F (LumexCoreDumpGeneratorAdminTest,
        GivenNoAdminGroupConfigured_WhenIsAdminPrivileges_ThenDoesNotThrow)
{
  EXPECT_NO_THROW ({ (void)core_dump_generator::is_admin_privileges (); });
}

#if LUMEX_OS_WINDOWS

TEST_F (LumexCoreDumpGeneratorAdminTest,
        GivenWindowsPlatform_WhenSetAdminGroupName_ThenIsNoOp)
{
  // set_admin_group_name() has no effect on Windows; it must simply not throw
  // or crash.
  EXPECT_NO_THROW (
      { core_dump_generator::set_admin_group_name ("some-group"); });
  EXPECT_NO_THROW ({ (void)core_dump_generator::is_admin_privileges (); });
}

#else

TEST_F (LumexCoreDumpGeneratorAdminTest,
        GivenNoAdminGroupConfigured_WhenIsAdminPrivileges_ThenMatchesRootCheck)
{
  bool const expectedRootOnly = (getuid () == 0);
  EXPECT_EQ (core_dump_generator::is_admin_privileges (), expectedRootOnly);
}

TEST_F (LumexCoreDumpGeneratorAdminTest,
        GivenNonExistentAdminGroup_WhenIsAdminPrivileges_ThenMatchesRootCheck)
{
  // A group name that (almost certainly) does not exist on the test machine
  // must not grant admin privileges to a non-root user: the group lookup
  // fails, so is_admin_privileges() falls back to the root-only check.
  core_dump_generator::set_admin_group_name (
      "lumex-test-nonexistent-group-xyz-42");

  bool const expectedRootOnly = (getuid () == 0);
  EXPECT_EQ (core_dump_generator::is_admin_privileges (), expectedRootOnly);
}

TEST_F (LumexCoreDumpGeneratorAdminTest,
        GivenEmptyAdminGroupName_WhenSetAdminGroupName_ThenDisablesGroupCheck)
{
  core_dump_generator::set_admin_group_name ("some-group-name");
  core_dump_generator::set_admin_group_name (""); // Explicitly disable again.

  bool const expectedRootOnly = (getuid () == 0);
  EXPECT_EQ (core_dump_generator::is_admin_privileges (), expectedRootOnly);
}

#endif

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenDefaultAuto_WhenGetDefaultDumpType_ThenReturnsPlatformDefault)
{
  DumpType const default_type = dump_factory::get_default_dump_type ();
#if LUMEX_OS_WINDOWS
  EXPECT_EQ (default_type, DumpType::DEFAULT_WINDOWS);
#else
  EXPECT_EQ (default_type, DumpType::DEFAULT_UNIX);
#endif
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenSupportedTypes_WhenIsSupported_ThenMatchesGetSupportedTypes)
{
  std::vector<DumpType> const supported = dump_factory::get_supported_types ();
  ASSERT_FALSE (supported.empty ());
  for (DumpType const type : supported)
    {
      EXPECT_TRUE (dump_factory::is_supported (type));
      EXPECT_FALSE (dump_factory::get_description (type).empty ());
      std::error_code error_code;
      dump_configuration const config
          = dump_factory::create_configuration (type, error_code);
      EXPECT_FALSE (error_code);
      EXPECT_TRUE (dump_factory::validate_configuration (config));
    }
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenUnixTypeOnWindowsOrWindowsTypeOnUnix_WhenIsSupported_ThenIsFalse)
{
#if LUMEX_OS_WINDOWS
  EXPECT_FALSE (dump_factory::is_supported (DumpType::CORE_DUMP_FULL));
#else
  EXPECT_FALSE (dump_factory::is_supported (DumpType::MINI_DUMP_NORMAL));
#endif
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenUnsupportedType_WhenCreateConfigurationWithErrorCode_ThenSetsError)
{
#if LUMEX_OS_WINDOWS
  DumpType const unsupported = DumpType::CORE_DUMP_FULL;
#else
  DumpType const unsupported = DumpType::MINI_DUMP_NORMAL;
#endif
  std::error_code error_code;
  (void)dump_factory::create_configuration (unsupported, error_code);
  EXPECT_TRUE (static_cast<bool> (error_code));
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenUnknownDumpType_WhenGetDescription_ThenReturnsUnknownFallback)
{
  auto const unknown = static_cast<DumpType> (127);
  EXPECT_EQ (dump_factory::get_description (unknown), "Unknown dump type");
  EXPECT_FALSE (dump_factory::is_supported (unknown));
  EXPECT_EQ (dump_factory::get_estimated_size (unknown), 0u);
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenValidAndInvalidDumpTypes_WhenIsValid_ThenMatchesRange)
{
  EXPECT_TRUE (DumpTypeUtils::is_valid (DumpType::MINI_DUMP_NORMAL));
  EXPECT_TRUE (DumpTypeUtils::is_valid (DumpType::CORE_DUMP_FULL));
  EXPECT_TRUE (DumpTypeUtils::is_valid (DumpType::DEFAULT_AUTO));
  EXPECT_FALSE (DumpTypeUtils::is_valid (static_cast<DumpType> (127)));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenWindowsMiniDump_WhenClassified_ThenIsWindowsNotUnixNotKernel)
{
  EXPECT_TRUE (DumpTypeUtils::is_windows_type (DumpType::MINI_DUMP_NORMAL));
  EXPECT_FALSE (DumpTypeUtils::is_unix_type (DumpType::MINI_DUMP_NORMAL));
  EXPECT_FALSE (DumpTypeUtils::is_kernel_type (DumpType::MINI_DUMP_NORMAL));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenUnixCoreDump_WhenClassified_ThenIsUnixNotWindowsNotKernel)
{
  EXPECT_TRUE (DumpTypeUtils::is_unix_type (DumpType::CORE_DUMP_FULL));
  EXPECT_FALSE (DumpTypeUtils::is_windows_type (DumpType::CORE_DUMP_FULL));
  EXPECT_FALSE (DumpTypeUtils::is_kernel_type (DumpType::CORE_DUMP_FULL));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenKernelDump_WhenClassified_ThenIsWindowsAndKernel)
{
  EXPECT_TRUE (DumpTypeUtils::is_kernel_type (DumpType::KERNEL_FULL_DUMP));
  EXPECT_TRUE (DumpTypeUtils::is_windows_type (DumpType::KERNEL_FULL_DUMP));
  EXPECT_FALSE (DumpTypeUtils::is_unix_type (DumpType::KERNEL_FULL_DUMP));
  EXPECT_TRUE (DumpTypeUtils::is_kernel_type (DumpType::KERNEL_ACTIVE_DUMP));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenDefaultAuto_WhenClassified_ThenValidButNotPlatformSpecific)
{
  EXPECT_TRUE (DumpTypeUtils::is_valid (DumpType::DEFAULT_AUTO));
  EXPECT_FALSE (DumpTypeUtils::is_windows_type (DumpType::DEFAULT_AUTO));
  EXPECT_FALSE (DumpTypeUtils::is_unix_type (DumpType::DEFAULT_AUTO));
  EXPECT_FALSE (DumpTypeUtils::is_kernel_type (DumpType::DEFAULT_AUTO));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenReservedKernelOnlyValue_WhenIsKernelType_ThenTrueButNotValid)
{
  auto const reserved
      = static_cast<DumpType> (DumpTypeUtils::Constants::KERNEL_ONLY_TYPE);
  EXPECT_TRUE (DumpTypeUtils::is_kernel_type (reserved));
  EXPECT_FALSE (DumpTypeUtils::is_valid (reserved));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenRangeHelpers_WhenQueried_ThenMinIsNormalAndMaxIsFullCore)
{
  EXPECT_EQ (DumpTypeUtils::get_min_value (), DumpType::MINI_DUMP_NORMAL);
  EXPECT_EQ (DumpTypeUtils::get_max_value (), DumpType::CORE_DUMP_FULL);
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenDefaultConstructedConfig_WhenInspected_ThenHasSensibleDefaults)
{
  dump_configuration const config;
  EXPECT_EQ (config.get_type (), DumpType::DEFAULT_AUTO);
  EXPECT_TRUE (config.get_filename ().empty ());
  EXPECT_TRUE (config.get_directory ().empty ());
  EXPECT_FALSE (config.is_compress ());
  EXPECT_TRUE (config.is_include_unloaded_modules ());
  EXPECT_TRUE (config.is_include_handle_data ());
  EXPECT_TRUE (config.is_include_thread_info ());
  EXPECT_TRUE (config.is_include_process_data ());
  EXPECT_EQ (config.get_max_size_bytes (), 0u);
  EXPECT_TRUE (config.get_memory_filters ().empty ());
  EXPECT_TRUE (config.is_enable_symbols ());
  EXPECT_TRUE (config.is_enable_source_info ());
  EXPECT_TRUE (config.is_valid ());
  EXPECT_TRUE (config.get_validation_error ().empty ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenEmptyFilename_WhenSetFilename_ThenAcceptedAsAutoGenerated)
{
  dump_configuration config;
  EXPECT_TRUE (config.set_filename (std::string ()));
  EXPECT_TRUE (config.get_filename ().empty ());
  EXPECT_TRUE (config.is_valid ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenSafeFilename_WhenSetFilename_ThenStoresValue)
{
  dump_configuration config;
  EXPECT_TRUE (config.set_filename ("crash.dump"));
  EXPECT_EQ (config.get_filename (), "crash.dump");
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenForbiddenFilenameChars_WhenSetFilename_ThenRejectedAndUnchanged)
{
  dump_configuration config;
  EXPECT_TRUE (config.set_filename ("ok.dump"));
  char const *const bad[]
      = { "a/b.dump",  "a\\b.dump", "a:b.dump", "a*b.dump", "a?b.dump",
          "a\"b.dump", "a<b.dump",  "a>b.dump", "a|b.dump" };
  for (char const *name : bad)
    {
      EXPECT_FALSE (config.set_filename (name)) << name;
      EXPECT_EQ (config.get_filename (), "ok.dump") << name;
    }
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenControlCharInFilename_WhenSetFilename_ThenRejected)
{
  dump_configuration config;
  std::string const with_tab = std::string ("bad") + '\t' + "name.dump";
  EXPECT_FALSE (config.set_filename (with_tab));
  EXPECT_TRUE (config.get_filename ().empty ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenInvalidType_WhenSetType_ThenRejectedAndKeepsDefault)
{
  dump_configuration config;
  EXPECT_FALSE (config.set_type (static_cast<DumpType> (127)));
  EXPECT_EQ (config.get_type (), DumpType::DEFAULT_AUTO);
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenValidTypes_WhenSetType_ThenAccepted)
{
  dump_configuration config;
  EXPECT_TRUE (config.set_type (DumpType::MINI_DUMP_NORMAL));
  EXPECT_EQ (config.get_type (), DumpType::MINI_DUMP_NORMAL);
  EXPECT_TRUE (config.set_type (DumpType::DEFAULT_AUTO));
  EXPECT_EQ (config.get_type (), DumpType::DEFAULT_AUTO);
  EXPECT_TRUE (config.set_type (DumpType::CORE_DUMP_FULL));
  EXPECT_EQ (config.get_type (), DumpType::CORE_DUMP_FULL);
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenTwoEqualConfigs_WhenCompared_ThenEqualAndNotUnequal)
{
  dump_configuration left;
  dump_configuration right;
  EXPECT_TRUE (left == right);
  EXPECT_FALSE (left != right);
  left.set_filename ("a.dump");
  EXPECT_TRUE (left != right);
  EXPECT_FALSE (left == right);
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenCopiedConfig_WhenMutatedIndependently_ThenOriginalUnchanged)
{
  dump_configuration original;
  original.set_filename ("orig.dump");
  dump_configuration copy = original;
  EXPECT_EQ (copy, original);
  EXPECT_TRUE (copy.set_filename ("copy.dump"));
  EXPECT_EQ (original.get_filename (), "orig.dump");
  EXPECT_EQ (copy.get_filename (), "copy.dump");
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenMovedConfig_WhenInspected_ThenPreservesFilename)
{
  dump_configuration source;
  EXPECT_TRUE (source.set_filename ("moved.dump"));
  dump_configuration dest (std::move (source));
  EXPECT_EQ (dest.get_filename (), "moved.dump");
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenToggleFlags_WhenSet_ThenGettersMatch)
{
  dump_configuration config;
  config.set_compress (true);
  config.set_include_unloaded_modules (false);
  config.set_include_handle_data (false);
  config.set_include_thread_info (false);
  config.set_include_process_data (false);
  config.set_enable_symbols (false);
  config.set_enable_source_info (false);
  EXPECT_TRUE (config.is_compress ());
  EXPECT_FALSE (config.is_include_unloaded_modules ());
  EXPECT_FALSE (config.is_include_handle_data ());
  EXPECT_FALSE (config.is_include_thread_info ());
  EXPECT_FALSE (config.is_include_process_data ());
  EXPECT_FALSE (config.is_enable_symbols ());
  EXPECT_FALSE (config.is_enable_source_info ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenMaxSize_WhenSetMaxSizeBytes_ThenStoresZeroAndPositive)
{
  dump_configuration config;
  EXPECT_TRUE (config.set_max_size_bytes (0));
  EXPECT_EQ (config.get_max_size_bytes (), 0u);
  EXPECT_TRUE (config.set_max_size_bytes (4096));
  EXPECT_EQ (config.get_max_size_bytes (), 4096u);
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenMemoryFilter_WhenAddedAndCleared_ThenListMatches)
{
  dump_configuration config;
  EXPECT_FALSE (config.add_memory_filter (std::string ()));
  EXPECT_FALSE (config.add_memory_filter ("bad*filter"));
  EXPECT_TRUE (config.add_memory_filter ("heap"));
  ASSERT_EQ (config.get_memory_filters ().size (), 1u);
  EXPECT_EQ (config.get_memory_filters ().front (), "heap");
  config.clear_memory_filters ();
  EXPECT_TRUE (config.get_memory_filters ().empty ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenEmptyDirectory_WhenSetDirectory_ThenAccepted)
{
  dump_configuration config;
  EXPECT_TRUE (config.set_directory (std::string ()));
  EXPECT_TRUE (config.get_directory ().empty ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenForbiddenDirectoryChars_WhenSetDirectory_ThenRejected)
{
  dump_configuration config;
  EXPECT_FALSE (config.set_directory ("bad*dir"));
  EXPECT_FALSE (config.set_directory ("bad?dir"));
  EXPECT_FALSE (config.set_directory ("bad|dir"));
  EXPECT_TRUE (config.get_directory ().empty ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenColonInDirectory_WhenSetDirectory_ThenAcceptedOnUnixStyleDrive)
{
  dump_configuration config;
  EXPECT_TRUE (config.set_directory ("C:\\dumps"));
  EXPECT_EQ (config.get_directory (), "C:\\dumps");
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenDefaultAuto_WhenIsSupported_ThenTrue)
{
  EXPECT_TRUE (dump_factory::is_supported (DumpType::DEFAULT_AUTO));
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenKnownTypes_WhenGetDescription_ThenMatchesCatalog)
{
  EXPECT_EQ (dump_factory::get_description (DumpType::MINI_DUMP_NORMAL),
             "Basic mini-dump (64KB)");
  EXPECT_EQ (dump_factory::get_description (DumpType::CORE_DUMP_FULL),
             "Full core dump with all memory");
  EXPECT_EQ (dump_factory::get_description (DumpType::DEFAULT_AUTO),
             "Auto-detect based on platform");
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenKnownTypes_WhenGetEstimatedSize_ThenMatchesCatalog)
{
  EXPECT_EQ (dump_factory::get_estimated_size (DumpType::MINI_DUMP_NORMAL),
             core_dump_generator::KB_64);
  EXPECT_EQ (dump_factory::get_estimated_size (DumpType::KERNEL_SMALL_DUMP),
             core_dump_generator::KB_64);
  EXPECT_EQ (dump_factory::get_estimated_size (DumpType::DEFAULT_AUTO), 0u);
  EXPECT_EQ (dump_factory::get_estimated_size (DumpType::CORE_DUMP_FULL), 0u);
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenDefaultAuto_WhenCreateConfigurationWithErrorCode_ThenSucceeds)
{
  std::error_code error_code;
  dump_configuration const config = dump_factory::create_configuration (
      DumpType::DEFAULT_AUTO, error_code);
  EXPECT_FALSE (error_code);
  EXPECT_TRUE (dump_factory::validate_configuration (config));
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenSupportedTypes_WhenGetSupportedTypes_ThenContainsDefaultAuto)
{
  std::vector<DumpType> const supported = dump_factory::get_supported_types ();
  bool found_auto = false;
  for (DumpType const type : supported)
    {
      if (type == DumpType::DEFAULT_AUTO)
        found_auto = true;
    }
  EXPECT_TRUE (found_auto);
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenPlatformDefault_WhenCreateConfiguration_ThenTypeIsPlatformDefault)
{
  dump_configuration const config
      = dump_factory::create_configuration (DumpType::DEFAULT_AUTO);
#if LUMEX_OS_WINDOWS
  EXPECT_EQ (config.get_type (), DumpType::DEFAULT_WINDOWS);
#else
  EXPECT_EQ (config.get_type (), DumpType::DEFAULT_UNIX);
#endif
  EXPECT_TRUE (config.is_enable_symbols ());
  EXPECT_TRUE (config.is_enable_source_info ());
}
