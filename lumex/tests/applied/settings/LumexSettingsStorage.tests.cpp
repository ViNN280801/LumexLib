// lumex/tests/applied/settings/LumexSettingsStorage.tests.cpp
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/applied/settings/LumexSettings"
#include "lumex/core/filesystem/LumexFilesystem"

using namespace lumex::applied::settings;
using namespace lumex::applied::settings::storage;

namespace
{
// Bytes of a file that a failed write must leave unchanged. The carriage
// return makes a rewrite in text mode visible on every platform.
char const *const OLD_BYTES = "old\r\ncontent\n";

// Content a successful write puts into the file.
char const *const NEW_BYTES = "new\ncontent\r\n";

// Section, key and value the backends save in the tests below.
char const *const SECTION = "section";
char const *const KEY = "key";
char const *const VALUE = "value";

using lumex::core::filesystem::fs::lumex_filesystem;

void
write_bytes (lumex::path const &path, std::string const &bytes)
{
  std::ofstream file (path.string (), std::ios::binary);
  ASSERT_TRUE (file.is_open ()) << path.string ();
  file << bytes;
  file.close ();
  ASSERT_TRUE (file.good ()) << path.string ();
}

std::string
read_bytes (lumex::path const &path)
{
  std::ifstream file (path.string (), std::ios::binary);
  return std::string ((std::istreambuf_iterator<char> (file)),
                      std::istreambuf_iterator<char> ());
}

lumex::path
temporary_of (lumex::path const &path)
{
  return lumex::path (path.string () + TEMPORARY_FILE_SUFFIX);
}

// A directory per test, removed (after its permissions are restored) when
// the test ends.
class StorageDirectory
{
public:
  StorageDirectory ()
  {
    ::testing::TestInfo const *info
        = ::testing::UnitTest::GetInstance ()->current_test_info ();
    std::string name = "test_settings_storage_";
    name += info->test_suite_name ();
    name += "_";
    name += info->name ();
    for (auto &chr : name)
      if (chr == '/')
        chr = '_';
    m_dir = lumex::path (name);
    remove ();
    if (!lumex_filesystem::create_directories (m_dir).success ())
      std::cerr << "Warning: Failed to create test directory: " << m_dir
                << std::endl;
  }

  ~StorageDirectory () { remove (); }

  StorageDirectory (StorageDirectory const &) = delete;
  StorageDirectory &operator= (StorageDirectory const &) = delete;

  lumex::path const &
  path () const
  {
    return m_dir;
  }

private:
  void
  remove ()
  {
    if (!lumex_filesystem::exists (m_dir))
      return;
#if LUMEX_OS_UNIX
    (void)chmod (m_dir.c_str (), 0777);
#endif
    if (!lumex_filesystem::remove_all (m_dir).success ())
      std::cerr << "Warning: Failed to remove test directory: " << m_dir
                << std::endl;
  }

  lumex::path m_dir;
};

#if LUMEX_OS_UNIX
// True when permission bits do not restrict this process (root).
bool
permissions_are_ignored ()
{
  return ::geteuid () == 0;
}
#endif
} // namespace

// --- replace_file_content ---------------------------------------------------

TEST (LumexSettingsStorageTest,
      GivenMissingFile_WhenReplaceFileContent_ThenCreatesItWithTheBytes)
{
  StorageDirectory dir;
  lumex::path const file = dir.path () / "settings.cfg";

  EXPECT_TRUE (replace_file_content (file.string (), NEW_BYTES,
                                     LumexWriteMode::binary));
  EXPECT_EQ (read_bytes (file), NEW_BYTES);
  EXPECT_FALSE (lumex_filesystem::exists (temporary_of (file)));
}

TEST (
    LumexSettingsStorageTest,
    GivenExistingFile_WhenReplaceFileContent_ThenReplacesItAndLeavesNoTemporary)
{
  StorageDirectory dir;
  lumex::path const file = dir.path () / "settings.cfg";
  write_bytes (file, OLD_BYTES);

  EXPECT_TRUE (replace_file_content (file.string (), NEW_BYTES,
                                     LumexWriteMode::binary));
  EXPECT_EQ (read_bytes (file), NEW_BYTES);
  EXPECT_FALSE (lumex_filesystem::exists (temporary_of (file)));
}

TEST (LumexSettingsStorageTest,
      GivenEmptyContent_WhenReplaceFileContent_ThenFileBecomesEmpty)
{
  StorageDirectory dir;
  lumex::path const file = dir.path () / "settings.cfg";
  write_bytes (file, OLD_BYTES);

  EXPECT_TRUE (
      replace_file_content (file.string (), "", LumexWriteMode::binary));
  EXPECT_EQ (read_bytes (file), "");
}

TEST (LumexSettingsStorageTest,
      GivenMissingParent_WhenReplaceFileContent_ThenCreatesDirectories)
{
  StorageDirectory dir;
  lumex::path const file = dir.path () / "nested" / "deeper" / "settings.cfg";

  EXPECT_TRUE (replace_file_content (file.string (), NEW_BYTES,
                                     LumexWriteMode::binary));
  EXPECT_EQ (read_bytes (file), NEW_BYTES);
}

TEST (LumexSettingsStorageTest,
      GivenStaleTemporaryFile_WhenReplaceFileContent_ThenOverwritesIt)
{
  StorageDirectory dir;
  lumex::path const file = dir.path () / "settings.cfg";
  write_bytes (temporary_of (file), "left over by an interrupted write");

  EXPECT_TRUE (replace_file_content (file.string (), NEW_BYTES,
                                     LumexWriteMode::binary));
  EXPECT_EQ (read_bytes (file), NEW_BYTES);
  EXPECT_FALSE (lumex_filesystem::exists (temporary_of (file)));
}

TEST (LumexSettingsStorageTest,
      GivenEmptyPath_WhenReplaceFileContent_ThenReturnsFalse)
{
  EXPECT_FALSE (replace_file_content ("", NEW_BYTES, LumexWriteMode::binary));
}

TEST (
    LumexSettingsStorageTest,
    GivenDirectoryAtTemporaryPath_WhenReplaceFileContent_ThenReturnsFalseAndKeepsTheFile)
{
  StorageDirectory dir;
  lumex::path const file = dir.path () / "settings.cfg";
  write_bytes (file, OLD_BYTES);
  ASSERT_TRUE (
      lumex_filesystem::create_directories (temporary_of (file)).success ());

  EXPECT_FALSE (replace_file_content (file.string (), NEW_BYTES,
                                      LumexWriteMode::binary));
  EXPECT_EQ (read_bytes (file), OLD_BYTES);
  EXPECT_TRUE (lumex_filesystem::is_directory (temporary_of (file)));
}

TEST (
    LumexSettingsStorageTest,
    GivenDirectoryAtTargetPath_WhenReplaceFileContent_ThenReturnsFalseAndRemovesTheTemporary)
{
  StorageDirectory dir;
  lumex::path const target = dir.path () / "settings.cfg";
  ASSERT_TRUE (lumex_filesystem::create_directories (target).success ());

  EXPECT_FALSE (replace_file_content (target.string (), NEW_BYTES,
                                      LumexWriteMode::binary));
  EXPECT_TRUE (lumex_filesystem::is_directory (target));
  EXPECT_FALSE (lumex_filesystem::exists (temporary_of (target)));
}

TEST (
    LumexSettingsStorageTest,
    GivenReadOnlyDirectory_WhenReplaceFileContent_ThenReturnsFalseAndKeepsTheFile)
{
#if LUMEX_OS_UNIX
  if (permissions_are_ignored ())
    GTEST_SKIP () << "running as root: permission bits do not deny writing";
  StorageDirectory dir;
  lumex::path const file = dir.path () / "settings.cfg";
  write_bytes (file, OLD_BYTES);
  ASSERT_EQ (chmod (dir.path ().c_str (), 0555), 0);

  bool const result = replace_file_content (file.string (), NEW_BYTES,
                                            LumexWriteMode::binary);
  (void)chmod (dir.path ().c_str (), 0777);

  EXPECT_FALSE (result);
  EXPECT_EQ (read_bytes (file), OLD_BYTES);
#else
  GTEST_SKIP () << "directory permission bits are POSIX-only";
#endif
}

// --- save () of every backend -----------------------------------------------

namespace
{
// Backends the factory can create in this build.
std::vector<LumexSettingsExtensions>
available_backends ()
{
  std::vector<LumexSettingsExtensions> result;
  for (LumexSettingsExtensions ext :
       { LumexSettingsExtensions::INI, LumexSettingsExtensions::JSON,
         LumexSettingsExtensions::XML })
    if (LumexSettingsFactory::create (ext))
      result.push_back (ext);
  return result;
}

std::string
backend_name (::testing::TestParamInfo<LumexSettingsExtensions> const &info)
{
  switch (info.param)
    {
    case LumexSettingsExtensions::INI:
      return "INI";
    case LumexSettingsExtensions::JSON:
      return "JSON";
    case LumexSettingsExtensions::XML:
      return "XML";
    default:
      return "Unknown";
    }
}

class LumexSettingsSaveTest
    : public ::testing::TestWithParam<LumexSettingsExtensions>
{
protected:
  std::unique_ptr<ILumexSettings>
  settings_with_one_key () const
  {
    std::unique_ptr<ILumexSettings> settings
        = LumexSettingsFactory::create (GetParam ());
    if (settings)
      settings->add (SECTION, KEY, VALUE);
    return settings;
  }
};
} // namespace

TEST_P (LumexSettingsSaveTest,
        GivenSuccessfulSave_WhenSaved_ThenLoadsBackAndLeavesNoTemporary)
{
  StorageDirectory dir;
  lumex::path const file = dir.path () / "settings.cfg";
  write_bytes (file, OLD_BYTES);
  std::unique_ptr<ILumexSettings> settings = settings_with_one_key ();
  ASSERT_NE (settings, nullptr);

  ASSERT_TRUE (settings->save (file.string ()));
  EXPECT_FALSE (lumex_filesystem::exists (temporary_of (file)));

  std::unique_ptr<ILumexSettings> reloaded
      = LumexSettingsFactory::create (GetParam ());
  ASSERT_TRUE (reloaded->load (file.string ()));
  EXPECT_EQ (reloaded->get (SECTION, KEY), VALUE);
}

TEST_P (LumexSettingsSaveTest,
        GivenDirectoryAtTemporaryPath_WhenSave_ThenReturnsFalseAndKeepsOldFile)
{
  StorageDirectory dir;
  lumex::path const file = dir.path () / "settings.cfg";
  write_bytes (file, OLD_BYTES);
  ASSERT_TRUE (
      lumex_filesystem::create_directories (temporary_of (file)).success ());
  std::unique_ptr<ILumexSettings> settings = settings_with_one_key ();
  ASSERT_NE (settings, nullptr);

  EXPECT_FALSE (settings->save (file.string ()));
  EXPECT_EQ (read_bytes (file), OLD_BYTES);
}

TEST_P (LumexSettingsSaveTest,
        GivenReadOnlyDirectory_WhenSave_ThenReturnsFalseAndKeepsOldFile)
{
#if LUMEX_OS_UNIX
  if (permissions_are_ignored ())
    GTEST_SKIP () << "running as root: permission bits do not deny writing";
  StorageDirectory dir;
  lumex::path const file = dir.path () / "settings.cfg";
  write_bytes (file, OLD_BYTES);
  std::unique_ptr<ILumexSettings> settings = settings_with_one_key ();
  ASSERT_NE (settings, nullptr);
  ASSERT_EQ (chmod (dir.path ().c_str (), 0555), 0);

  bool const result = settings->save (file.string ());
  (void)chmod (dir.path ().c_str (), 0777);

  EXPECT_FALSE (result);
  EXPECT_EQ (read_bytes (file), OLD_BYTES);
#else
  GTEST_SKIP () << "directory permission bits are POSIX-only";
#endif
}

INSTANTIATE_TEST_SUITE_P (Backends, LumexSettingsSaveTest,
                          ::testing::ValuesIn (available_backends ()),
                          backend_name);
