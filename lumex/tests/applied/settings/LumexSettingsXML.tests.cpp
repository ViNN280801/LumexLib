// lumex/tests/applied/settings/LumexSettingsXML.tests.cpp
#include <fstream>
#include <iostream>
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
using namespace lumex::applied::settings::xml;
using namespace lumex::core::filesystem::fs;

class LumexSettingsXMLTest : public ::testing::Test
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
    std::string dir_name = "test_xml_settings_";
    dir_name += info->test_suite_name ();
    dir_name += "_";
    dir_name += info->name ();
    _test_dir = lumex::path (dir_name);
    _test_file = _test_dir / "test.xml";

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
  create_test_xml_file (lumex::path const &path, std::string const &content)
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

TEST_F (LumexSettingsXMLTest, Factory_CreateXML_MatchesXmlBuild)
{
  std::unique_ptr<ILumexSettings> settings
      = LumexSettingsFactory::create (LumexSettingsExtensions::XML);
#if defined(LUMEX_SETTINGS_WITH_XML)
  ASSERT_NE (settings, nullptr);
  EXPECT_NE (dynamic_cast<LumexSettingsXML *> (settings.get ()), nullptr);
#else
  EXPECT_EQ (settings, nullptr);
#endif
}

TEST_F (LumexSettingsXMLTest,
        Factory_CreateUnsupportedExtension_ReturnsNullptr)
{
  std::unique_ptr<ILumexSettings> settings = LumexSettingsFactory::create (
      static_cast<LumexSettingsExtensions> (99));
  EXPECT_EQ (settings, nullptr);
}

TEST_F (LumexSettingsXMLTest, GivenNullArguments_WhenGetAddRemove_ThenNoThrow)
{
  LumexSettingsXML xml_settings;
  EXPECT_EQ (xml_settings.get (nullptr, "key"), "");
  EXPECT_EQ (xml_settings.get ("section", nullptr), "");
  xml_settings.add (nullptr, "key", "value");
  xml_settings.add ("section", nullptr, "value");
  xml_settings.add ("section", "key", nullptr);
  xml_settings.remove (nullptr, "key");
  xml_settings.remove ("section", nullptr);
  EXPECT_EQ (xml_settings.get ("section", "key"), "");
}

TEST_F (LumexSettingsXMLTest,
        GivenEmptySectionOrKeyOrValue_WhenAdd_ThenDoesNothing)
{
  LumexSettingsXML xml_settings;
  xml_settings.add ("", "key", "value");
  xml_settings.add ("section", "", "value");
  xml_settings.add ("section", "key", "");
  EXPECT_EQ (xml_settings.get ("section", "key"), "");
}

TEST_F (LumexSettingsXMLTest, GivenExistingKey_WhenAdd_ThenUpdatesValue)
{
  LumexSettingsXML xml_settings;
  xml_settings.add ("pump", "flow", "1.0");
  xml_settings.add ("pump", "flow", "1.5");
  EXPECT_EQ (xml_settings.get ("pump", "flow"), "1.5");
}

TEST_F (LumexSettingsXMLTest, GivenMissingKey_WhenGetOrRemove_ThenEmptyNoOp)
{
  LumexSettingsXML xml_settings;
  xml_settings.add ("pump", "flow", "1.0");
  EXPECT_EQ (xml_settings.get ("pump", "missing"), "");
  EXPECT_EQ (xml_settings.get ("oven", "flow"), "");
  xml_settings.remove ("pump", "missing");
  xml_settings.remove ("oven", "flow");
  EXPECT_EQ (xml_settings.get ("pump", "flow"), "1.0");
}

TEST_F (LumexSettingsXMLTest, Get_WhenFound_ThenReturnsStoredValue)
{
  LumexSettingsXML xml_settings;
  xml_settings.add ("pump", "flow", "1.0");
  EXPECT_EQ (xml_settings.get ("pump", "flow"), "1.0");
}

TEST_F (LumexSettingsXMLTest, Get_WhenUnfound_ThenReturnsEmpty)
{
  LumexSettingsXML xml_settings;
  xml_settings.add ("pump", "flow", "1.0");
  EXPECT_EQ (xml_settings.get ("pump", "missing"), "");
  EXPECT_EQ (xml_settings.get ("oven", "flow"), "");
}

TEST_F (LumexSettingsXMLTest, GivenExistingKey_WhenRemove_ThenGone)
{
  LumexSettingsXML xml_settings;
  xml_settings.add ("pump", "flow", "1.0");
  xml_settings.add ("pump", "pressure", "10");
  xml_settings.remove ("pump", "flow");
  EXPECT_EQ (xml_settings.get ("pump", "flow"), "");
  EXPECT_EQ (xml_settings.get ("pump", "pressure"), "10");
}

TEST_F (LumexSettingsXMLTest,
        GivenNullFilePath_WhenLoadOrSaveOrValid_ThenFalse)
{
  LumexSettingsXML xml_settings;
  EXPECT_FALSE (
      LumexSettingsXML::is_xml_valid (static_cast<char const *> (nullptr)));
  EXPECT_FALSE (xml_settings.load (static_cast<char const *> (nullptr)));
  EXPECT_FALSE (xml_settings.save (static_cast<char const *> (nullptr)));
}

#if defined(LUMEX_SETTINGS_WITH_XML)

TEST_F (LumexSettingsXMLTest, GivenMissingFile_WhenIsXmlValid_ThenReturnsFalse)
{
  EXPECT_FALSE (LumexSettingsXML::is_xml_valid (_test_file.string ()));
}

TEST_F (LumexSettingsXMLTest, GivenDirectory_WhenLoad_ThenReturnsFalse)
{
  LumexSettingsXML xml_settings;
  EXPECT_FALSE (LumexSettingsXML::is_xml_valid (_test_dir.string ()));
  EXPECT_FALSE (xml_settings.load (_test_dir.string ()));
}

TEST_F (LumexSettingsXMLTest, GivenMalformedXml_WhenLoad_ThenReturnsFalse)
{
  create_test_xml_file (_test_file, "<settings><pump><flow>1.0</pump>");
  LumexSettingsXML xml_settings;
  xml_settings.add ("keep", "me", "yes");
  EXPECT_FALSE (LumexSettingsXML::is_xml_valid (_test_file.string ()));
  EXPECT_FALSE (xml_settings.load (_test_file.string ()));
  EXPECT_EQ (xml_settings.get ("keep", "me"), "yes");
}

TEST_F (LumexSettingsXMLTest, GivenEmptyRoot_WhenLoad_ThenReturnsFalse)
{
  create_test_xml_file (_test_file, "<settings/>");
  LumexSettingsXML xml_settings;
  EXPECT_TRUE (LumexSettingsXML::is_xml_valid (_test_file.string ()));
  EXPECT_FALSE (xml_settings.load (_test_file.string ()));
}

TEST_F (LumexSettingsXMLTest,
        GivenLoggerStyleRoot_WhenLoad_ThenUsesRootAsSection)
{
  create_test_xml_file (_test_file, "<logger>\n"
                                    "  <LEVEL>DEBUG</LEVEL>\n"
                                    "  <HINT>1</HINT>\n"
                                    "</logger>\n");
  LumexSettingsXML xml_settings;
  ASSERT_TRUE (xml_settings.load (_test_file.string ()));
  EXPECT_EQ (xml_settings.get ("logger", "LEVEL"), "DEBUG");
  EXPECT_EQ (xml_settings.get ("logger", "HINT"), "1");
  EXPECT_EQ (xml_settings.get ("settings", "LEVEL"), "");
}

TEST_F (LumexSettingsXMLTest, GivenSectionChildren_WhenLoad_ThenLoadsAll)
{
  create_test_xml_file (_test_file, "<settings>\n"
                                    "  <pump>\n"
                                    "    <flow>1.0</flow>\n"
                                    "    <pressure>10</pressure>\n"
                                    "  </pump>\n"
                                    "  <detector>\n"
                                    "    <wavelength>254</wavelength>\n"
                                    "  </detector>\n"
                                    "</settings>\n");
  LumexSettingsXML xml_settings;
  ASSERT_TRUE (xml_settings.load (_test_file.string ()));
  EXPECT_EQ (xml_settings.get ("pump", "flow"), "1.0");
  EXPECT_EQ (xml_settings.get ("pump", "pressure"), "10");
  EXPECT_EQ (xml_settings.get ("detector", "wavelength"), "254");
}

TEST_F (LumexSettingsXMLTest,
        GivenMixedRoot_WhenLoad_ThenRootLeavesUseRootName)
{
  create_test_xml_file (_test_file, "<app>\n"
                                    "  <version>2</version>\n"
                                    "  <pump>\n"
                                    "    <flow>0.8</flow>\n"
                                    "  </pump>\n"
                                    "</app>\n");
  LumexSettingsXML xml_settings;
  ASSERT_TRUE (xml_settings.load (_test_file.string ()));
  EXPECT_EQ (xml_settings.get ("app", "version"), "2");
  EXPECT_EQ (xml_settings.get ("pump", "flow"), "0.8");
}

TEST_F (LumexSettingsXMLTest, GivenAddSaveLoad_WhenRoundtrip_ThenValuesMatch)
{
  LumexSettingsXML xml_settings;
  xml_settings.add ("pump", "flow", "1.0");
  xml_settings.add ("detector", "wavelength", "254");
  ASSERT_TRUE (xml_settings.save (_test_file.string ()));
  ASSERT_TRUE (LumexSettingsXML::is_xml_valid (_test_file.string ()));

  std::string const written = read_file_content (_test_file);
  EXPECT_NE (written.find ("<settings>"), std::string::npos);
  EXPECT_NE (written.find ("<pump>"), std::string::npos);
  EXPECT_NE (written.find ("<flow>"), std::string::npos);

  LumexSettingsXML reloaded;
  ASSERT_TRUE (reloaded.load (_test_file.string ()));
  EXPECT_EQ (reloaded.get ("pump", "flow"), "1.0");
  EXPECT_EQ (reloaded.get ("detector", "wavelength"), "254");
}

TEST_F (LumexSettingsXMLTest, GivenSpecialXmlChars_WhenSaveLoad_ThenPreserved)
{
  LumexSettingsXML xml_settings;
  xml_settings.add ("notes", "text", "a<b & c>\"d\"");
  ASSERT_TRUE (xml_settings.save (_test_file.string ()));

  LumexSettingsXML reloaded;
  ASSERT_TRUE (reloaded.load (_test_file.string ()));
  EXPECT_EQ (reloaded.get ("notes", "text"), "a<b & c>\"d\"");
}

TEST_F (LumexSettingsXMLTest,
        GivenNestedParent_WhenSave_ThenCreatesDirectories)
{
  lumex::path nested = _test_dir / "nested" / "out.xml";
  LumexSettingsXML xml_settings;
  xml_settings.add ("oven", "temperature", "40");
  ASSERT_TRUE (xml_settings.save (nested.string ()));
  LumexSettingsXML reloaded;
  ASSERT_TRUE (reloaded.load (nested.string ()));
  EXPECT_EQ (reloaded.get ("oven", "temperature"), "40");
}

TEST_F (LumexSettingsXMLTest, GivenFactoryXml_WhenSaveLoad_ThenWorks)
{
  std::unique_ptr<ILumexSettings> settings
      = LumexSettingsFactory::create (LumexSettingsExtensions::XML);
  ASSERT_NE (settings, nullptr);
  settings->add ("run", "operator", "lab");
  ASSERT_TRUE (settings->save (_test_file.string ()));
  ASSERT_TRUE (settings->load (_test_file.string ()));
  EXPECT_EQ (settings->get ("run", "operator"), "lab");
}

#endif // defined(LUMEX_SETTINGS_WITH_XML)
