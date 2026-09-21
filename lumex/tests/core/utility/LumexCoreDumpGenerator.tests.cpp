// LumexCoreDumpGenerator.tests.cpp
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

// Regression coverage for the isAdminPrivileges() UNIX fix: it now also treats
// members of a configurable admin group as admin (in addition to root),
// instead of only checking `getuid() == 0`. The group name is intentionally
// NOT hardcoded by LumexLib - it defaults to empty (group check disabled) and
// must be opted into via setAdminGroupName().

class LumexCoreDumpGeneratorAdminTest : public ::testing::Test
{
protected:
  void
  TearDown () override
  {
    // Reset global state so this test doesn't leak into other tests/processes.
    CoreDumpGenerator::setAdminGroupName ("");
  }
};

TEST_F (LumexCoreDumpGeneratorAdminTest,
        GivenNoAdminGroupConfigured_WhenIsAdminPrivileges_ThenDoesNotThrow)
{
  EXPECT_NO_THROW ({ (void)CoreDumpGenerator::isAdminPrivileges (); });
}

#if LUMEX_OS_WINDOWS

TEST_F (LumexCoreDumpGeneratorAdminTest,
        GivenWindowsPlatform_WhenSetAdminGroupName_ThenIsNoOp)
{
  // setAdminGroupName() has no effect on Windows; it must simply not throw or
  // crash.
  EXPECT_NO_THROW ({ CoreDumpGenerator::setAdminGroupName ("some-group"); });
  EXPECT_NO_THROW ({ (void)CoreDumpGenerator::isAdminPrivileges (); });
}

#else

TEST_F (LumexCoreDumpGeneratorAdminTest,
        GivenNoAdminGroupConfigured_WhenIsAdminPrivileges_ThenMatchesRootCheck)
{
  bool const expectedRootOnly = (getuid () == 0);
  EXPECT_EQ (CoreDumpGenerator::isAdminPrivileges (), expectedRootOnly);
}

TEST_F (LumexCoreDumpGeneratorAdminTest,
        GivenNonExistentAdminGroup_WhenIsAdminPrivileges_ThenMatchesRootCheck)
{
  // A group name that (almost certainly) does not exist on the test machine
  // must not grant admin privileges to a non-root user: the group lookup
  // fails, so isAdminPrivileges() falls back to the root-only check.
  CoreDumpGenerator::setAdminGroupName ("lumex-test-nonexistent-group-xyz-42");

  bool const expectedRootOnly = (getuid () == 0);
  EXPECT_EQ (CoreDumpGenerator::isAdminPrivileges (), expectedRootOnly);
}

TEST_F (LumexCoreDumpGeneratorAdminTest,
        GivenEmptyAdminGroupName_WhenSetAdminGroupName_ThenDisablesGroupCheck)
{
  CoreDumpGenerator::setAdminGroupName ("some-group-name");
  CoreDumpGenerator::setAdminGroupName (""); // Explicitly disable again.

  bool const expectedRootOnly = (getuid () == 0);
  EXPECT_EQ (CoreDumpGenerator::isAdminPrivileges (), expectedRootOnly);
}

#endif

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenDefaultAuto_WhenGetDefaultDumpType_ThenReturnsPlatformDefault)
{
  DumpType const default_type = DumpFactory::getDefaultDumpType ();
#if LUMEX_OS_WINDOWS
  EXPECT_EQ (default_type, DumpType::DEFAULT_WINDOWS);
#else
  EXPECT_EQ (default_type, DumpType::DEFAULT_UNIX);
#endif
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenSupportedTypes_WhenIsSupported_ThenMatchesGetSupportedTypes)
{
  std::vector<DumpType> const supported = DumpFactory::getSupportedTypes ();
  ASSERT_FALSE (supported.empty ());
  for (DumpType const type : supported)
    {
      EXPECT_TRUE (DumpFactory::isSupported (type));
      EXPECT_FALSE (DumpFactory::getDescription (type).empty ());
      std::error_code error_code;
      DumpConfiguration const config
          = DumpFactory::createConfiguration (type, error_code);
      EXPECT_FALSE (error_code);
      EXPECT_TRUE (DumpFactory::validateConfiguration (config));
    }
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenUnixTypeOnWindowsOrWindowsTypeOnUnix_WhenIsSupported_ThenIsFalse)
{
#if LUMEX_OS_WINDOWS
  EXPECT_FALSE (DumpFactory::isSupported (DumpType::CORE_DUMP_FULL));
#else
  EXPECT_FALSE (DumpFactory::isSupported (DumpType::MINI_DUMP_NORMAL));
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
  (void)DumpFactory::createConfiguration (unsupported, error_code);
  EXPECT_TRUE (static_cast<bool> (error_code));
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenUnknownDumpType_WhenGetDescription_ThenReturnsUnknownFallback)
{
  auto const unknown = static_cast<DumpType> (127);
  EXPECT_EQ (DumpFactory::getDescription (unknown), "Unknown dump type");
  EXPECT_FALSE (DumpFactory::isSupported (unknown));
  EXPECT_EQ (DumpFactory::getEstimatedSize (unknown), 0u);
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenValidAndInvalidDumpTypes_WhenIsValid_ThenMatchesRange)
{
  EXPECT_TRUE (DumpTypeUtils::isValid (DumpType::MINI_DUMP_NORMAL));
  EXPECT_TRUE (DumpTypeUtils::isValid (DumpType::CORE_DUMP_FULL));
  EXPECT_TRUE (DumpTypeUtils::isValid (DumpType::DEFAULT_AUTO));
  EXPECT_FALSE (DumpTypeUtils::isValid (static_cast<DumpType> (127)));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenWindowsMiniDump_WhenClassified_ThenIsWindowsNotUnixNotKernel)
{
  EXPECT_TRUE (DumpTypeUtils::isWindowsType (DumpType::MINI_DUMP_NORMAL));
  EXPECT_FALSE (DumpTypeUtils::isUnixType (DumpType::MINI_DUMP_NORMAL));
  EXPECT_FALSE (DumpTypeUtils::isKernelType (DumpType::MINI_DUMP_NORMAL));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenUnixCoreDump_WhenClassified_ThenIsUnixNotWindowsNotKernel)
{
  EXPECT_TRUE (DumpTypeUtils::isUnixType (DumpType::CORE_DUMP_FULL));
  EXPECT_FALSE (DumpTypeUtils::isWindowsType (DumpType::CORE_DUMP_FULL));
  EXPECT_FALSE (DumpTypeUtils::isKernelType (DumpType::CORE_DUMP_FULL));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenKernelDump_WhenClassified_ThenIsWindowsAndKernel)
{
  EXPECT_TRUE (DumpTypeUtils::isKernelType (DumpType::KERNEL_FULL_DUMP));
  EXPECT_TRUE (DumpTypeUtils::isWindowsType (DumpType::KERNEL_FULL_DUMP));
  EXPECT_FALSE (DumpTypeUtils::isUnixType (DumpType::KERNEL_FULL_DUMP));
  EXPECT_TRUE (DumpTypeUtils::isKernelType (DumpType::KERNEL_ACTIVE_DUMP));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenDefaultAuto_WhenClassified_ThenValidButNotPlatformSpecific)
{
  EXPECT_TRUE (DumpTypeUtils::isValid (DumpType::DEFAULT_AUTO));
  EXPECT_FALSE (DumpTypeUtils::isWindowsType (DumpType::DEFAULT_AUTO));
  EXPECT_FALSE (DumpTypeUtils::isUnixType (DumpType::DEFAULT_AUTO));
  EXPECT_FALSE (DumpTypeUtils::isKernelType (DumpType::DEFAULT_AUTO));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenReservedKernelOnlyValue_WhenIsKernelType_ThenTrueButNotValid)
{
  auto const reserved
      = static_cast<DumpType> (DumpTypeUtils::Constants::KERNEL_ONLY_TYPE);
  EXPECT_TRUE (DumpTypeUtils::isKernelType (reserved));
  EXPECT_FALSE (DumpTypeUtils::isValid (reserved));
}

TEST (LumexCoreDumpGeneratorTypeUtilsTest,
      GivenRangeHelpers_WhenQueried_ThenMinIsNormalAndMaxIsFullCore)
{
  EXPECT_EQ (DumpTypeUtils::getMinValue (), DumpType::MINI_DUMP_NORMAL);
  EXPECT_EQ (DumpTypeUtils::getMaxValue (), DumpType::CORE_DUMP_FULL);
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenDefaultConstructedConfig_WhenInspected_ThenHasSensibleDefaults)
{
  DumpConfiguration const config;
  EXPECT_EQ (config.getType (), DumpType::DEFAULT_AUTO);
  EXPECT_TRUE (config.getFilename ().empty ());
  EXPECT_TRUE (config.getDirectory ().empty ());
  EXPECT_FALSE (config.isCompress ());
  EXPECT_TRUE (config.isIncludeUnloadedModules ());
  EXPECT_TRUE (config.isIncludeHandleData ());
  EXPECT_TRUE (config.isIncludeThreadInfo ());
  EXPECT_TRUE (config.isIncludeProcessData ());
  EXPECT_EQ (config.getMaxSizeBytes (), 0u);
  EXPECT_TRUE (config.getMemoryFilters ().empty ());
  EXPECT_TRUE (config.isEnableSymbols ());
  EXPECT_TRUE (config.isEnableSourceInfo ());
  EXPECT_TRUE (config.isValid ());
  EXPECT_TRUE (config.getValidationError ().empty ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenEmptyFilename_WhenSetFilename_ThenAcceptedAsAutoGenerated)
{
  DumpConfiguration config;
  EXPECT_TRUE (config.setFilename (std::string ()));
  EXPECT_TRUE (config.getFilename ().empty ());
  EXPECT_TRUE (config.isValid ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenSafeFilename_WhenSetFilename_ThenStoresValue)
{
  DumpConfiguration config;
  EXPECT_TRUE (config.setFilename ("crash.dump"));
  EXPECT_EQ (config.getFilename (), "crash.dump");
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenForbiddenFilenameChars_WhenSetFilename_ThenRejectedAndUnchanged)
{
  DumpConfiguration config;
  EXPECT_TRUE (config.setFilename ("ok.dump"));
  char const *const bad[]
      = { "a/b.dump",  "a\\b.dump", "a:b.dump", "a*b.dump", "a?b.dump",
          "a\"b.dump", "a<b.dump",  "a>b.dump", "a|b.dump" };
  for (char const *name : bad)
    {
      EXPECT_FALSE (config.setFilename (name)) << name;
      EXPECT_EQ (config.getFilename (), "ok.dump") << name;
    }
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenControlCharInFilename_WhenSetFilename_ThenRejected)
{
  DumpConfiguration config;
  std::string const with_tab = std::string ("bad") + '\t' + "name.dump";
  EXPECT_FALSE (config.setFilename (with_tab));
  EXPECT_TRUE (config.getFilename ().empty ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenInvalidType_WhenSetType_ThenRejectedAndKeepsDefault)
{
  DumpConfiguration config;
  EXPECT_FALSE (config.setType (static_cast<DumpType> (127)));
  EXPECT_EQ (config.getType (), DumpType::DEFAULT_AUTO);
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenValidTypes_WhenSetType_ThenAccepted)
{
  DumpConfiguration config;
  EXPECT_TRUE (config.setType (DumpType::MINI_DUMP_NORMAL));
  EXPECT_EQ (config.getType (), DumpType::MINI_DUMP_NORMAL);
  EXPECT_TRUE (config.setType (DumpType::DEFAULT_AUTO));
  EXPECT_EQ (config.getType (), DumpType::DEFAULT_AUTO);
  EXPECT_TRUE (config.setType (DumpType::CORE_DUMP_FULL));
  EXPECT_EQ (config.getType (), DumpType::CORE_DUMP_FULL);
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenTwoEqualConfigs_WhenCompared_ThenEqualAndNotUnequal)
{
  DumpConfiguration left;
  DumpConfiguration right;
  EXPECT_TRUE (left == right);
  EXPECT_FALSE (left != right);
  left.setFilename ("a.dump");
  EXPECT_TRUE (left != right);
  EXPECT_FALSE (left == right);
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenCopiedConfig_WhenMutatedIndependently_ThenOriginalUnchanged)
{
  DumpConfiguration original;
  original.setFilename ("orig.dump");
  DumpConfiguration copy = original;
  EXPECT_EQ (copy, original);
  EXPECT_TRUE (copy.setFilename ("copy.dump"));
  EXPECT_EQ (original.getFilename (), "orig.dump");
  EXPECT_EQ (copy.getFilename (), "copy.dump");
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenMovedConfig_WhenInspected_ThenPreservesFilename)
{
  DumpConfiguration source;
  EXPECT_TRUE (source.setFilename ("moved.dump"));
  DumpConfiguration dest (std::move (source));
  EXPECT_EQ (dest.getFilename (), "moved.dump");
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenToggleFlags_WhenSet_ThenGettersMatch)
{
  DumpConfiguration config;
  config.setCompress (true);
  config.setIncludeUnloadedModules (false);
  config.setIncludeHandleData (false);
  config.setIncludeThreadInfo (false);
  config.setIncludeProcessData (false);
  config.setEnableSymbols (false);
  config.setEnableSourceInfo (false);
  EXPECT_TRUE (config.isCompress ());
  EXPECT_FALSE (config.isIncludeUnloadedModules ());
  EXPECT_FALSE (config.isIncludeHandleData ());
  EXPECT_FALSE (config.isIncludeThreadInfo ());
  EXPECT_FALSE (config.isIncludeProcessData ());
  EXPECT_FALSE (config.isEnableSymbols ());
  EXPECT_FALSE (config.isEnableSourceInfo ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenMaxSize_WhenSetMaxSizeBytes_ThenStoresZeroAndPositive)
{
  DumpConfiguration config;
  EXPECT_TRUE (config.setMaxSizeBytes (0));
  EXPECT_EQ (config.getMaxSizeBytes (), 0u);
  EXPECT_TRUE (config.setMaxSizeBytes (4096));
  EXPECT_EQ (config.getMaxSizeBytes (), 4096u);
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenMemoryFilter_WhenAddedAndCleared_ThenListMatches)
{
  DumpConfiguration config;
  EXPECT_FALSE (config.addMemoryFilter (std::string ()));
  EXPECT_FALSE (config.addMemoryFilter ("bad*filter"));
  EXPECT_TRUE (config.addMemoryFilter ("heap"));
  ASSERT_EQ (config.getMemoryFilters ().size (), 1u);
  EXPECT_EQ (config.getMemoryFilters ().front (), "heap");
  config.clearMemoryFilters ();
  EXPECT_TRUE (config.getMemoryFilters ().empty ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenEmptyDirectory_WhenSetDirectory_ThenAccepted)
{
  DumpConfiguration config;
  EXPECT_TRUE (config.setDirectory (std::string ()));
  EXPECT_TRUE (config.getDirectory ().empty ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenForbiddenDirectoryChars_WhenSetDirectory_ThenRejected)
{
  DumpConfiguration config;
  EXPECT_FALSE (config.setDirectory ("bad*dir"));
  EXPECT_FALSE (config.setDirectory ("bad?dir"));
  EXPECT_FALSE (config.setDirectory ("bad|dir"));
  EXPECT_TRUE (config.getDirectory ().empty ());
}

TEST (LumexCoreDumpGeneratorConfigTest,
      GivenColonInDirectory_WhenSetDirectory_ThenAcceptedOnUnixStyleDrive)
{
  DumpConfiguration config;
  EXPECT_TRUE (config.setDirectory ("C:\\dumps"));
  EXPECT_EQ (config.getDirectory (), "C:\\dumps");
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenDefaultAuto_WhenIsSupported_ThenTrue)
{
  EXPECT_TRUE (DumpFactory::isSupported (DumpType::DEFAULT_AUTO));
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenKnownTypes_WhenGetDescription_ThenMatchesCatalog)
{
  EXPECT_EQ (DumpFactory::getDescription (DumpType::MINI_DUMP_NORMAL),
             "Basic mini-dump (64KB)");
  EXPECT_EQ (DumpFactory::getDescription (DumpType::CORE_DUMP_FULL),
             "Full core dump with all memory");
  EXPECT_EQ (DumpFactory::getDescription (DumpType::DEFAULT_AUTO),
             "Auto-detect based on platform");
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenKnownTypes_WhenGetEstimatedSize_ThenMatchesCatalog)
{
  EXPECT_EQ (DumpFactory::getEstimatedSize (DumpType::MINI_DUMP_NORMAL),
             CoreDumpGenerator::KB_64);
  EXPECT_EQ (DumpFactory::getEstimatedSize (DumpType::KERNEL_SMALL_DUMP),
             CoreDumpGenerator::KB_64);
  EXPECT_EQ (DumpFactory::getEstimatedSize (DumpType::DEFAULT_AUTO), 0u);
  EXPECT_EQ (DumpFactory::getEstimatedSize (DumpType::CORE_DUMP_FULL), 0u);
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenDefaultAuto_WhenCreateConfigurationWithErrorCode_ThenSucceeds)
{
  std::error_code error_code;
  DumpConfiguration const config
      = DumpFactory::createConfiguration (DumpType::DEFAULT_AUTO, error_code);
  EXPECT_FALSE (error_code);
  EXPECT_TRUE (DumpFactory::validateConfiguration (config));
}

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenSupportedTypes_WhenGetSupportedTypes_ThenContainsDefaultAuto)
{
  std::vector<DumpType> const supported = DumpFactory::getSupportedTypes ();
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
  DumpConfiguration const config
      = DumpFactory::createConfiguration (DumpType::DEFAULT_AUTO);
#if LUMEX_OS_WINDOWS
  EXPECT_EQ (config.getType (), DumpType::DEFAULT_WINDOWS);
#else
  EXPECT_EQ (config.getType (), DumpType::DEFAULT_UNIX);
#endif
  EXPECT_TRUE (config.isEnableSymbols ());
  EXPECT_TRUE (config.isEnableSourceInfo ());
}
