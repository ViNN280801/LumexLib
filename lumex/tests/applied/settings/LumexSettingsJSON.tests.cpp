// lumex/tests/applied/settings/LumexSettingsJSON.tests.cpp
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

#include <gtest/gtest.h>

#include "lumex/applied/settings/LumexSettings"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/utility/LumexUtility"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::applied::settings;
using namespace lumex::applied::settings::json;
using namespace lumex::core::filesystem::fs;

class LumexSettingsJSONTest : public ::testing::Test
{
protected:
  lumex::path _test_dir;
  lumex::path _test_file;

  static void
  remove_directory_if_exists (lumex::path const &dir)
  {
    if (!lumex::core::filesystem::fs::lumex_filesystem::exists (dir))
      return;
    auto result
        = lumex::core::filesystem::fs::lumex_filesystem::remove_all (dir);
    if (!result.success ())
      std::cerr << "Warning: Failed to remove test directory: " << dir
                << std::endl;
  }

  void
  SetUp () override
  {
    ::testing::TestInfo const *info
        = ::testing::UnitTest::GetInstance ()->current_test_info ();
    std::string dir_name = "test_json_settings_";
    dir_name += info->test_suite_name ();
    dir_name += "_";
    dir_name += info->name ();
    _test_dir = lumex::path (dir_name);
    _test_file = _test_dir / "test.json";

    remove_directory_if_exists (_test_dir);

    auto result
        = lumex::core::filesystem::fs::lumex_filesystem::create_directories (
            _test_dir);
    if (!result.success ())
      std::cerr << "Warning: Failed to create test directory: " << _test_dir
                << std::endl;
  }

  void
  TearDown () override
  {
#if LUMEX_OS_UNIX
    if (lumex::core::filesystem::fs::lumex_filesystem::exists (_test_dir))
      {
        auto listing
            = lumex::core::filesystem::fs::lumex_filesystem::directory_paths (
                _test_dir);
        if (listing.success ())
          {
            for (auto const &entry : listing.value ())
              (void)chmod (entry.c_str (), 0777);
          }
        (void)chmod (_test_dir.c_str (), 0777);
      }
#endif
    remove_directory_if_exists (_test_dir);
  }

  void
  create_test_json_file (lumex::path const &path, std::string const &content)
  {
    lumex::path parent = path.parent_path ();
    if (!parent.empty ()
        && !lumex::core::filesystem::fs::lumex_filesystem::exists (parent))
      {
        auto result = lumex::core::filesystem::fs::lumex_filesystem::
            create_directories (parent);
        ASSERT_TRUE (result.success ())
            << "Failed to create parent directory: " << parent.string ();
      }

    std::ofstream file (path.string ());
    ASSERT_TRUE (file.is_open ())
        << "Failed to create test file: " << path.string ();
    file << content;
    ASSERT_TRUE (file.good ())
        << "Failed to write test file: " << path.string ();
    file.close ();
  }

  std::string
  read_file_content (lumex::path const &path)
  {
    std::ifstream file (path.string ());
    if (!file.is_open ())
      return "";
    return std::string ((std::istreambuf_iterator<char> (file)),
                        std::istreambuf_iterator<char> ());
  }
};

TEST_F (LumexSettingsJSONTest, Factory_CreateJSON_MatchesJsonBuild)
{
  std::unique_ptr<ILumexSettings> settings
      = LumexSettingsFactory::create (LumexSettingsExtensions::JSON);
#if defined(LUMEX_SETTINGS_WITH_JSON)
  ASSERT_NE (settings, nullptr);
  EXPECT_NE (dynamic_cast<LumexSettingsJSON *> (settings.get ()), nullptr);
#else
  EXPECT_EQ (settings, nullptr);
#endif
}

TEST_F (LumexSettingsJSONTest,
        Factory_CreateUnsupportedExtension_ReturnsNullptr)
{
  std::unique_ptr<ILumexSettings> settings = LumexSettingsFactory::create (
      static_cast<LumexSettingsExtensions> (99));
  EXPECT_EQ (settings, nullptr);
}

TEST_F (LumexSettingsJSONTest, GivenNullArguments_WhenGetAddRemove_ThenNoThrow)
{
  LumexSettingsJSON json_settings;
  EXPECT_EQ (json_settings.get (nullptr, "key"), "");
  EXPECT_EQ (json_settings.get ("section", nullptr), "");
  json_settings.add (nullptr, "key", "value");
  json_settings.add ("section", nullptr, "value");
  json_settings.add ("section", "key", nullptr);
  json_settings.remove (nullptr, "key");
  json_settings.remove ("section", nullptr);
  EXPECT_EQ (json_settings.get ("section", "key"), "");
}

TEST_F (LumexSettingsJSONTest,
        GivenEmptySectionOrKeyOrValue_WhenAdd_ThenDoesNothing)
{
  LumexSettingsJSON json_settings;
  json_settings.add ("", "key", "value");
  json_settings.add ("section", "", "value");
  json_settings.add ("section", "key", "");
  EXPECT_EQ (json_settings.get ("section", "key"), "");
}

TEST_F (LumexSettingsJSONTest, GivenExistingKey_WhenAdd_ThenUpdatesValue)
{
  LumexSettingsJSON json_settings;
  json_settings.add ("pump", "flow", "1.0");
  json_settings.add ("pump", "flow", "1.5");
  EXPECT_EQ (json_settings.get ("pump", "flow"), "1.5");
}

TEST_F (LumexSettingsJSONTest, GivenMissingKey_WhenGetOrRemove_ThenEmptyNoOp)
{
  LumexSettingsJSON json_settings;
  json_settings.add ("pump", "flow", "1.0");
  EXPECT_EQ (json_settings.get ("pump", "missing"), "");
  EXPECT_EQ (json_settings.get ("oven", "flow"), "");
  json_settings.remove ("pump", "missing");
  json_settings.remove ("oven", "flow");
  EXPECT_EQ (json_settings.get ("pump", "flow"), "1.0");
}

TEST_F (LumexSettingsJSONTest, Get_WhenFound_ThenReturnsStoredValue)
{
  LumexSettingsJSON json_settings;
  json_settings.add ("pump", "flow", "1.0");
  EXPECT_EQ (json_settings.get ("pump", "flow"), "1.0");
}

TEST_F (LumexSettingsJSONTest, Get_WhenUnfound_ThenReturnsEmpty)
{
  LumexSettingsJSON json_settings;
  json_settings.add ("pump", "flow", "1.0");
  EXPECT_EQ (json_settings.get ("pump", "missing"), "");
  EXPECT_EQ (json_settings.get ("oven", "flow"), "");
}

TEST_F (LumexSettingsJSONTest, GivenExistingKey_WhenRemove_ThenGone)
{
  LumexSettingsJSON json_settings;
  json_settings.add ("pump", "flow", "1.0");
  json_settings.add ("pump", "pressure", "10");
  json_settings.remove ("pump", "flow");
  EXPECT_EQ (json_settings.get ("pump", "flow"), "");
  EXPECT_EQ (json_settings.get ("pump", "pressure"), "10");
}

TEST_F (LumexSettingsJSONTest,
        GivenNullFilePath_WhenLoadOrSaveOrValid_ThenFalse)
{
  LumexSettingsJSON json_settings;
  EXPECT_FALSE (
      LumexSettingsJSON::is_json_valid (static_cast<char const *> (nullptr)));
  EXPECT_FALSE (json_settings.load (static_cast<char const *> (nullptr)));
  EXPECT_FALSE (json_settings.save (static_cast<char const *> (nullptr)));
}

#if defined(LUMEX_SETTINGS_WITH_JSON)

TEST_F (LumexSettingsJSONTest,
        GivenMissingFile_WhenIsJsonValid_ThenReturnsFalse)
{
  EXPECT_FALSE (LumexSettingsJSON::is_json_valid (_test_file.string ()));
}

TEST_F (LumexSettingsJSONTest, GivenDirectory_WhenLoad_ThenReturnsFalse)
{
  LumexSettingsJSON json_settings;
  EXPECT_FALSE (LumexSettingsJSON::is_json_valid (_test_dir.string ()));
  EXPECT_FALSE (json_settings.load (_test_dir.string ()));
}

TEST_F (LumexSettingsJSONTest, GivenMalformedJson_WhenLoad_ThenReturnsFalse)
{
  create_test_json_file (_test_file, "{\"pump\": {\"flow\": ");
  LumexSettingsJSON json_settings;
  json_settings.add ("keep", "me", "yes");
  EXPECT_FALSE (LumexSettingsJSON::is_json_valid (_test_file.string ()));
  EXPECT_FALSE (json_settings.load (_test_file.string ()));
  EXPECT_EQ (json_settings.get ("keep", "me"), "yes");
}

TEST_F (LumexSettingsJSONTest, GivenArrayRoot_WhenLoad_ThenReturnsFalse)
{
  create_test_json_file (_test_file, "[1, 2, 3]");
  LumexSettingsJSON json_settings;
  EXPECT_FALSE (LumexSettingsJSON::is_json_valid (_test_file.string ()));
  EXPECT_FALSE (json_settings.load (_test_file.string ()));
}

TEST_F (LumexSettingsJSONTest, GivenEmptyObject_WhenLoad_ThenReturnsFalse)
{
  create_test_json_file (_test_file, "{}");
  LumexSettingsJSON json_settings;
  EXPECT_TRUE (LumexSettingsJSON::is_json_valid (_test_file.string ()));
  EXPECT_FALSE (json_settings.load (_test_file.string ()));
}

TEST_F (LumexSettingsJSONTest,
        GivenLoggerStyleNested_WhenLoad_ThenUsesNestedSection)
{
  create_test_json_file (_test_file, "{\n"
                                     "  \"logger\": {\n"
                                     "    \"LEVEL\": \"DEBUG\",\n"
                                     "    \"HINT\": 1,\n"
                                     "    \"enabled\": true\n"
                                     "  }\n"
                                     "}\n");
  LumexSettingsJSON json_settings;
  ASSERT_TRUE (json_settings.load (_test_file.string ()));
  EXPECT_EQ (json_settings.get ("logger", "LEVEL"), "DEBUG");
  EXPECT_EQ (json_settings.get ("logger", "HINT"), "1");
  EXPECT_EQ (json_settings.get ("logger", "enabled"), "true");
  EXPECT_EQ (json_settings.get ("settings", "LEVEL"), "");
}

TEST_F (LumexSettingsJSONTest, GivenFlatRoot_WhenLoad_ThenUsesSettingsSection)
{
  create_test_json_file (_test_file, "{\n"
                                     "  \"LEVEL\": \"DEBUG\",\n"
                                     "  \"HINT\": 1\n"
                                     "}\n");
  LumexSettingsJSON json_settings;
  ASSERT_TRUE (json_settings.load (_test_file.string ()));
  EXPECT_EQ (json_settings.get ("settings", "LEVEL"), "DEBUG");
  EXPECT_EQ (json_settings.get ("settings", "HINT"), "1");
  EXPECT_EQ (json_settings.get ("logger", "LEVEL"), "");
}

TEST_F (LumexSettingsJSONTest, GivenSectionChildren_WhenLoad_ThenLoadsAll)
{
  create_test_json_file (_test_file, "{\n"
                                     "  \"pump\": {\n"
                                     "    \"flow\": \"1.0\",\n"
                                     "    \"pressure\": 10\n"
                                     "  },\n"
                                     "  \"detector\": {\n"
                                     "    \"wavelength\": \"254\"\n"
                                     "  }\n"
                                     "}\n");
  LumexSettingsJSON json_settings;
  ASSERT_TRUE (json_settings.load (_test_file.string ()));
  EXPECT_EQ (json_settings.get ("pump", "flow"), "1.0");
  EXPECT_EQ (json_settings.get ("pump", "pressure"), "10");
  EXPECT_EQ (json_settings.get ("detector", "wavelength"), "254");
}

TEST_F (LumexSettingsJSONTest,
        GivenMixedRoot_WhenLoad_ThenRootLeavesUseSettings)
{
  create_test_json_file (_test_file, "{\n"
                                     "  \"version\": 2,\n"
                                     "  \"pump\": {\n"
                                     "    \"flow\": \"0.8\"\n"
                                     "  }\n"
                                     "}\n");
  LumexSettingsJSON json_settings;
  ASSERT_TRUE (json_settings.load (_test_file.string ()));
  EXPECT_EQ (json_settings.get ("settings", "version"), "2");
  EXPECT_EQ (json_settings.get ("pump", "flow"), "0.8");
}

TEST_F (LumexSettingsJSONTest, GivenAddSaveLoad_WhenRoundtrip_ThenValuesMatch)
{
  LumexSettingsJSON json_settings;
  json_settings.add ("pump", "flow", "1.0");
  json_settings.add ("detector", "wavelength", "254");
  ASSERT_TRUE (json_settings.save (_test_file.string ()));
  ASSERT_TRUE (LumexSettingsJSON::is_json_valid (_test_file.string ()));

  std::string const written = read_file_content (_test_file);
  EXPECT_NE (written.find ("\"pump\""), std::string::npos);
  EXPECT_NE (written.find ("\"flow\""), std::string::npos);
  EXPECT_NE (written.find ("\"1.0\""), std::string::npos);

  LumexSettingsJSON reloaded;
  ASSERT_TRUE (reloaded.load (_test_file.string ()));
  EXPECT_EQ (reloaded.get ("pump", "flow"), "1.0");
  EXPECT_EQ (reloaded.get ("detector", "wavelength"), "254");
}

TEST_F (LumexSettingsJSONTest,
        GivenSpecialJsonChars_WhenSaveLoad_ThenPreserved)
{
  LumexSettingsJSON json_settings;
  json_settings.add ("notes", "text", "a\"b\\c");
  ASSERT_TRUE (json_settings.save (_test_file.string ()));

  LumexSettingsJSON reloaded;
  ASSERT_TRUE (reloaded.load (_test_file.string ()));
  EXPECT_EQ (reloaded.get ("notes", "text"), "a\"b\\c");
}

TEST_F (LumexSettingsJSONTest,
        GivenNestedParent_WhenSave_ThenCreatesDirectories)
{
  lumex::path nested = _test_dir / "nested" / "out.json";
  LumexSettingsJSON json_settings;
  json_settings.add ("oven", "temperature", "40");
  ASSERT_TRUE (json_settings.save (nested.string ()));
  LumexSettingsJSON reloaded;
  ASSERT_TRUE (reloaded.load (nested.string ()));
  EXPECT_EQ (reloaded.get ("oven", "temperature"), "40");
}

TEST_F (LumexSettingsJSONTest, GivenFactoryJson_WhenSaveLoad_ThenWorks)
{
  std::unique_ptr<ILumexSettings> settings
      = LumexSettingsFactory::create (LumexSettingsExtensions::JSON);
  ASSERT_NE (settings, nullptr);
  settings->add ("run", "operator", "lab");
  ASSERT_TRUE (settings->save (_test_file.string ()));
  ASSERT_TRUE (settings->load (_test_file.string ()));
  EXPECT_EQ (settings->get ("run", "operator"), "lab");
}

#endif // defined(LUMEX_SETTINGS_WITH_JSON)
