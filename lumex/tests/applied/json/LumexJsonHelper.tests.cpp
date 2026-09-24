// LumexJsonHelper.tests.cpp
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/applied/json/LumexJson"
#include "lumex/tests/support/LumexPerfSkip.hpp"

#ifndef LUMEX_JSON_TEST_SUITE
#define LUMEX_JSON_TEST_SUITE "default"
#endif

using lumex::applied::json::diagnostics::LumexJsonDiagnosticLevel;
using lumex::applied::json::diagnostics::LumexJsonDiagnosticSlot;
using lumex::applied::json::helper::LumexJsonHelper;

namespace
{
struct captured_diagnostic_t
{
  LumexJsonDiagnosticLevel level;
  std::string message;
};

std::mutex g_captured_mutex;
std::vector<captured_diagnostic_t> g_captured;

void
capture_diagnostic (LumexJsonDiagnosticLevel level, char const *message)
{
  std::lock_guard<std::mutex> const lock (g_captured_mutex);
  captured_diagnostic_t entry;
  entry.level = level;
  entry.message = message;
  g_captured.push_back (entry);
}

std::size_t
count_level (LumexJsonDiagnosticLevel level)
{
  std::lock_guard<std::mutex> const lock (g_captured_mutex);
  std::size_t count = 0;
  for (std::size_t i = 0; i < g_captured.size (); ++i)
    if (g_captured[i].level == level)
      ++count;
  return count;
}

bool
any_message_contains (std::string const &needle)
{
  std::lock_guard<std::mutex> const lock (g_captured_mutex);
  for (std::size_t i = 0; i < g_captured.size (); ++i)
    if (g_captured[i].message.find (needle) != std::string::npos)
      return true;
  return false;
}

std::size_t
captured_count ()
{
  std::lock_guard<std::mutex> const lock (g_captured_mutex);
  return g_captured.size ();
}
} // namespace

// Every test gets its own file, named after the suite binary and the test,
// so the C++11 and C++20 suites can run in parallel. Diagnostics are
// captured instead of going to std::cerr, so tests can assert on them.
class LumexJsonHelperTest : public ::testing::Test
{
protected:
  std::string current_test_filename_;
  std::string test_section = "TestSection";

  void
  SetUp () override
  {
    {
      std::lock_guard<std::mutex> const lock (g_captured_mutex);
      g_captured.clear ();
    }
    LumexJsonDiagnosticSlot::set (&capture_diagnostic);

    ::testing::TestInfo const *info
        = ::testing::UnitTest::GetInstance ()->current_test_info ();
    current_test_filename_
        = std::string ("LumexJsonHelper.") + LUMEX_JSON_TEST_SUITE + "."
          + info->test_case_name () + "." + info->name () + ".json";
    if (std::remove (current_test_filename_.c_str ()) != 0 && errno != ENOENT)
      ADD_FAILURE () << "Cannot remove '" << current_test_filename_
                     << "' before the test: " << std::strerror (errno);
  }

  void
  TearDown () override
  {
    LumexJsonDiagnosticSlot::reset ();
    if (std::remove (current_test_filename_.c_str ()) != 0 && errno != ENOENT)
      ADD_FAILURE () << "Cannot remove '" << current_test_filename_
                     << "' after the test: " << std::strerror (errno);
  }

  void
  create_test_file (nlohmann::json const &content)
  {
    std::ofstream ofs (current_test_filename_.c_str ());
    if (!ofs.is_open ())
      {
        FAIL () << "Cannot create the test file " << current_test_filename_;
      }
    ofs << std::setw (4) << content << std::endl;
  }

  void
  create_raw_file (std::string const &text)
  {
    std::ofstream ofs (current_test_filename_.c_str ());
    ofs << text;
  }

  nlohmann::json
  read_file_content ()
  {
    std::ifstream ifs (current_test_filename_.c_str ());
    if (!ifs.is_open ())
      return nlohmann::json ();
    nlohmann::json content;
    try
      {
        ifs >> content;
      }
    catch (std::exception const &exc)
      {
        // Some tests write invalid JSON on purpose.
        std::cerr << "read_file_content(" << current_test_filename_
                  << "): " << exc.what () << std::endl;
      }
    return content;
  }
};

// --- load_config ---

TEST_F (LumexJsonHelperTest,
        GivenValidJsonFile_WhenLoadConfig_ThenReturnsCorrectJson)
{
  nlohmann::json expected_json = { { "key", "value" }, { "number", 123 } };
  create_test_file (expected_json);

  nlohmann::json loaded_json
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_EQ (loaded_json, expected_json);
  EXPECT_FALSE (loaded_json.empty ());
}

TEST_F (LumexJsonHelperTest,
        GivenNonExistentFile_WhenLoadConfig_ThenReturnsEmptyJsonAndLogsError)
{
  std::remove (current_test_filename_.c_str ());

  nlohmann::json loaded_json
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_TRUE (loaded_json.empty ());
}

TEST_F (LumexJsonHelperTest,
        GivenInvalidJsonFile_WhenLoadConfig_ThenReturnsEmptyJsonAndLogsError)
{
  std::ofstream ofs (current_test_filename_);
  ofs << "This is not JSON { \"key\": \"value\" ";
  ofs.close ();

  nlohmann::json loaded_json
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_TRUE (loaded_json.empty ());
}

TEST_F (LumexJsonHelperTest,
        GivenEmptyFile_WhenLoadConfig_ThenReturnsEmptyJson)
{
  std::ofstream ofs (current_test_filename_);
  ofs.close ();

  nlohmann::json loaded_json
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_TRUE (loaded_json.empty ());
}

TEST_F (LumexJsonHelperTest,
        GivenEmptyFilename_WhenLoadConfig_ThenReturnsEmptyJsonAndLogsError)
{
  nlohmann::json loaded_json = LumexJsonHelper::load_config ("");
  EXPECT_TRUE (loaded_json.empty ());
}

// --- save_config ---

TEST_F (LumexJsonHelperTest,
        GivenJsonAndFilename_WhenSaveConfig_ThenFileContainsCorrectJson)
{
  nlohmann::json json_to_save
      = { { "test_key", "test_value" }, { "array", { 1, 2, 3 } } };
  bool saved
      = LumexJsonHelper::save_config (json_to_save, current_test_filename_);
  EXPECT_TRUE (saved);

  nlohmann::json loaded_json = read_file_content ();
  EXPECT_EQ (loaded_json, json_to_save);
}

TEST_F (LumexJsonHelperTest, GivenEmptyJson_WhenSaveConfig_ThenFileIsEmptyJson)
{
  nlohmann::json empty_json = {};
  bool saved
      = LumexJsonHelper::save_config (empty_json, current_test_filename_);
  EXPECT_TRUE (saved);

  nlohmann::json loaded_json = read_file_content ();
  EXPECT_TRUE (loaded_json.empty ());
}

TEST_F (LumexJsonHelperTest,
        GivenEmptyFilename_WhenSaveConfig_ThenReturnsFalseAndLogsError)
{
  nlohmann::json json_to_save = { { "key", "value" } };
  bool saved = LumexJsonHelper::save_config (json_to_save, "");
  EXPECT_FALSE (saved);
  std::ifstream ifs ("");
  EXPECT_FALSE (ifs.is_open ());
}

// --- has_section ---

TEST_F (LumexJsonHelperTest,
        GivenConfigWithSection_WhenHasSection_ThenReturnsTrue)
{
  nlohmann::json config = { { "Section1", { { "key", "value" } } } };
  EXPECT_TRUE (LumexJsonHelper::has_section (config, "Section1"));
}

TEST_F (LumexJsonHelperTest,
        GivenConfigWithoutSection_WhenHasSection_ThenReturnsFalse)
{
  nlohmann::json config = { { "OtherSection", {} } };
  EXPECT_FALSE (LumexJsonHelper::has_section (config, "NonExistentSection"));
}

TEST_F (LumexJsonHelperTest,
        GivenEmptySectionName_WhenHasSection_ThenReturnsTrue)
{
  nlohmann::json config = { { "key", "value" } };
  EXPECT_TRUE (LumexJsonHelper::has_section (config, ""));
}

TEST_F (LumexJsonHelperTest,
        GivenSectionAsNull_WhenHasSection_ThenReturnsFalse)
{
  nlohmann::json config = { { "SectionNull", nullptr } };
  EXPECT_FALSE (LumexJsonHelper::has_section (config, "SectionNull"));
}

TEST_F (LumexJsonHelperTest, GivenEmptyConfig_WhenHasSection_ThenReturnsFalse)
{
  nlohmann::json config = {};
  EXPECT_FALSE (LumexJsonHelper::has_section (config, "AnySection"));
}

// --- create_section ---

TEST_F (LumexJsonHelperTest,
        GivenNullConfig_WhenCreateSection_ThenReturnsFalseAndNothingIsCreated)
{
  nlohmann::json config = {};
  bool created = LumexJsonHelper::create_section (config, test_section);
  EXPECT_FALSE (created);
  EXPECT_FALSE (config.contains (test_section));
  EXPECT_FALSE (config[test_section].is_object ());
}

TEST_F (LumexJsonHelperTest,
        GivenExistingSection_WhenCreateSection_ThenReturnsTrueAndNoChange)
{
  nlohmann::json config
      = { { test_section, { { "initial_key", "initial_value" } } } };
  nlohmann::json initial_config = config;
  bool created = LumexJsonHelper::create_section (config, test_section);
  EXPECT_TRUE (created);
  EXPECT_EQ (config, initial_config);
}

TEST_F (LumexJsonHelperTest,
        GivenEmptySectionName_WhenCreateSection_ThenReturnsTrue)
{
  nlohmann::json config = { { "key", "value" } };
  bool created = LumexJsonHelper::create_section (config, "");
  EXPECT_TRUE (created);
}

TEST_F (
    LumexJsonHelperTest,
    GivenExistingKeyNotObject_WhenCreateSection_ThenReturnsFalseAndLogsError)
{
  nlohmann::json config = { { test_section, "not_an_object" } };
  bool created = LumexJsonHelper::create_section (config, test_section);
  EXPECT_FALSE (created);
  EXPECT_FALSE (config[test_section].is_object ());
  EXPECT_EQ (config[test_section], "not_an_object");
}

TEST_F (LumexJsonHelperTest,
        GivenNonObjectConfig_WhenCreateSection_ThenReturnsFalseAndLogsError)
{
  nlohmann::json config = nlohmann::json::array ();
  bool created = LumexJsonHelper::create_section (config, test_section);
  EXPECT_FALSE (created);
  EXPECT_TRUE (config.is_array ());
}

// --- has_key ---

TEST_F (LumexJsonHelperTest,
        GivenRootConfigWithKey_WhenHasKeyString_ThenReturnsTrue)
{
  nlohmann::json config = { { "my_key", "value" } };
  EXPECT_TRUE (LumexJsonHelper::has_key (config, "my_key"));
}

TEST_F (LumexJsonHelperTest,
        GivenRootConfigWithoutKey_WhenHasKeyString_ThenReturnsFalse)
{
  nlohmann::json config = { { "other_key", "value" } };
  EXPECT_FALSE (LumexJsonHelper::has_key (config, "non_existent"));
}

TEST_F (LumexJsonHelperTest,
        GivenSectionConfigWithKey_WhenHasKeyString_ThenReturnsTrue)
{
  nlohmann::json config = { { test_section, { { "section_key", 123 } } } };
  EXPECT_TRUE (LumexJsonHelper::has_key (config, "section_key", test_section));
}

TEST_F (LumexJsonHelperTest,
        GivenSectionConfigWithoutKey_WhenHasKeyString_ThenReturnsFalse)
{
  nlohmann::json config = { { test_section, { { "other_key", 123 } } } };
  EXPECT_FALSE (LumexJsonHelper::has_key (config, "non_existent_section_key",
                                          test_section));
}

TEST_F (LumexJsonHelperTest, GivenKeyAsNull_WhenHasKey_ThenReturnsFalse)
{
  nlohmann::json config = { { "null_key", nullptr } };
  EXPECT_FALSE (LumexJsonHelper::has_key (config, "null_key"));
}

TEST_F (LumexJsonHelperTest, GivenEmptySection_WhenHasKey_ThenReturnsFalse)
{
  nlohmann::json config = { { test_section, nlohmann::json::object () } };
  EXPECT_FALSE (LumexJsonHelper::has_key (config, "any_key", test_section));
}

// --- get_value ---

TEST_F (LumexJsonHelperTest,
        GivenRootConfig_WhenGetValueString_ThenReturnsCorrectValue)
{
  nlohmann::json config = { { "name", "Alice" } };
  std::string defaultVal = "Default";
  std::string value = LumexJsonHelper::get_value (config, "name", defaultVal);
  EXPECT_EQ (value, "Alice");
}

TEST_F (LumexJsonHelperTest,
        GivenRootConfig_WhenGetValueInt_ThenReturnsCorrectValue)
{
  nlohmann::json config = { { "age", 30 } };
  int value = LumexJsonHelper::get_value (config, "age", 0);
  EXPECT_EQ (value, 30);
}

TEST_F (LumexJsonHelperTest,
        GivenSectionConfig_WhenGetValueBool_ThenReturnsCorrectValue)
{
  nlohmann::json config = { { test_section, { { "enabled", true } } } };
  bool value
      = LumexJsonHelper::get_value (config, "enabled", false, test_section);
  EXPECT_TRUE (value);
}

TEST_F (LumexJsonHelperTest,
        GivenNonExistentKey_WhenGetValue_ThenReturnsDefaultValue)
{
  nlohmann::json config = { { "key", "value" } };
  std::string defaultVal = "DEFAULT_VALUE";
  std::string value
      = LumexJsonHelper::get_value (config, "non_existent", defaultVal);
  EXPECT_EQ (value, "DEFAULT_VALUE");
}

TEST_F (LumexJsonHelperTest,
        GivenNonExistentSection_WhenGetValue_ThenReturnsDefaultValue)
{
  nlohmann::json config = { { "key", "value" } };
  int value = LumexJsonHelper::get_value (config, "number", 99,
                                          "NonExistentSection");
  EXPECT_EQ (value, 99);
}

TEST_F (LumexJsonHelperTest,
        GivenKeyIsNull_WhenGetValue_ThenReturnsDefaultValue)
{
  nlohmann::json config = { { "null_key", nullptr } };
  std::string defaultVal = "NULL_DEFAULT";
  std::string value
      = LumexJsonHelper::get_value (config, "null_key", defaultVal);
  EXPECT_EQ (value, "NULL_DEFAULT");
}

TEST_F (LumexJsonHelperTest,
        GivenMismatchedType_WhenGetValue_ThenReturnsDefaultValue)
{
  nlohmann::json config = { { "number_as_string", "123" } };
  int value = LumexJsonHelper::get_value (config, "number_as_string", 0);
  EXPECT_EQ (value, 0);
}

// --- set_value ---

TEST_F (LumexJsonHelperTest, GivenRootConfig_WhenSetValueString_ThenValueIsSet)
{
  nlohmann::json config = {};
  bool set = LumexJsonHelper::set_value (config, "new_key", "new_value");
  EXPECT_TRUE (set);
  EXPECT_EQ (config["new_key"], "new_value");
}

TEST_F (LumexJsonHelperTest,
        GivenSectionConfig_WhenSetValueInt_ThenValueIsUpdated)
{
  nlohmann::json config = { { test_section, { { "count", 10 } } } };
  bool set = LumexJsonHelper::set_value (config, "count", 20, test_section);
  EXPECT_TRUE (set);
  EXPECT_EQ (config[test_section]["count"], 20);
}

TEST_F (LumexJsonHelperTest,
        GivenNonExistentSection_WhenSetValue_ThenSectionAndValueAreCreated)
{
  nlohmann::json config = {};
  bool set = LumexJsonHelper::set_value (config, "section_key", "val",
                                         "NewSection");
  EXPECT_TRUE (set);
  EXPECT_TRUE (config.contains ("NewSection"));
  EXPECT_TRUE (config["NewSection"].is_object ());
  EXPECT_EQ (config["NewSection"]["section_key"], "val");
}

TEST_F (LumexJsonHelperTest,
        GivenEmptyStringValue_WhenSetValue_ThenReturnsFalseAndLogsWarning)
{
  nlohmann::json config = {};
  bool set = LumexJsonHelper::set_value (config, "empty_str", "");
  EXPECT_FALSE (set);
  EXPECT_FALSE (config.contains ("empty_str"));
}

TEST_F (LumexJsonHelperTest,
        GivenNullptrStringValue_WhenSetValue_ThenReturnsFalseAndLogsWarning)
{
  nlohmann::json config = {};
  bool set = LumexJsonHelper::set_value (config, "null_ptr_str",
                                         static_cast<char *> (nullptr));
  EXPECT_FALSE (set);
  EXPECT_FALSE (config.contains ("null_ptr_str"));
}

TEST_F (
    LumexJsonHelperTest,
    GivenNonObjectTargetSection_WhenSetValue_ThenConvertsToObjectAndSetsValue)
{
  nlohmann::json config = { { test_section, nlohmann::json::array () } };
  bool set = LumexJsonHelper::set_value (config, "arr_key", "arr_value",
                                         test_section);
  EXPECT_TRUE (set);
  EXPECT_TRUE (config[test_section].is_object ());
  EXPECT_EQ (config[test_section]["arr_key"], "arr_value");
}

// --- is_json_file_ok ---

TEST_F (LumexJsonHelperTest,
        GivenValidJsonFile_WhenIsJsonFileOk_ThenReturnsTrue)
{
  nlohmann::json content = { { "key", "value" } };
  create_test_file (content);
  EXPECT_TRUE (LumexJsonHelper::is_json_file_ok (current_test_filename_));
}

TEST_F (LumexJsonHelperTest,
        GivenNonExistentFile_WhenIsJsonFileOk_ThenReturnsFalseAndLogsError)
{
  std::remove (current_test_filename_.c_str ());
  EXPECT_FALSE (LumexJsonHelper::is_json_file_ok (current_test_filename_));
}

TEST_F (LumexJsonHelperTest,
        GivenInvalidJsonFile_WhenIsJsonFileOk_ThenReturnsFalseAndLogsError)
{
  std::ofstream ofs (current_test_filename_);
  ofs << "invalid json {";
  ofs.close ();
  EXPECT_FALSE (LumexJsonHelper::is_json_file_ok (current_test_filename_));
}

TEST_F (LumexJsonHelperTest, GivenEmptyFile_WhenIsJsonFileOk_ThenReturnsFalse)
{
  std::ofstream ofs (current_test_filename_);
  ofs.close ();

  EXPECT_FALSE (LumexJsonHelper::is_json_file_ok (current_test_filename_));
}

TEST_F (LumexJsonHelperTest,
        GivenEmptyFilename_WhenIsJsonFileOk_ThenReturnsFalseAndLogsError)
{
  EXPECT_FALSE (LumexJsonHelper::is_json_file_ok (""));
}

// --- write_value ---

TEST_F (LumexJsonHelperTest,
        GivenNewFileAndSection_WhenWriteValue_ThenFileContainsValue)
{
  bool written = LumexJsonHelper::write_value (
      current_test_filename_, "new_key", "new_val", test_section);
  EXPECT_TRUE (written);
  nlohmann::json loaded = read_file_content ();
  EXPECT_EQ (loaded[test_section]["new_key"], "new_val");
}

TEST_F (LumexJsonHelperTest,
        GivenExistingFile_WhenWriteValueToRoot_ThenFileContainsValue)
{
  create_test_file ({ { "initial_key", "initial_val" } });
  bool written = LumexJsonHelper::write_value (current_test_filename_,
                                               "new_root_key", 123);
  EXPECT_TRUE (written);
  nlohmann::json loaded = read_file_content ();
  EXPECT_EQ (loaded["new_root_key"], 123);
  EXPECT_EQ (loaded["initial_key"], "initial_val");
}

TEST_F (LumexJsonHelperTest,
        GivenEmptyFilename_WhenWriteValue_ThenReturnsFalseAndLogsError)
{
  bool written = LumexJsonHelper::write_value ("", "key", "value");
  EXPECT_FALSE (written);
}

TEST_F (LumexJsonHelperTest,
        GivenEmptyStringValue_WhenWriteValue_ThenReturnsFalseAndLogsWarning)
{
  bool written
      = LumexJsonHelper::write_value (current_test_filename_, "empty_val", "");
  EXPECT_FALSE (written);
  std::ifstream ifs (current_test_filename_);
  EXPECT_FALSE (ifs.is_open ());
}

// --- edit_value ---

TEST_F (LumexJsonHelperTest,
        GivenExistingValueInFile_WhenEditValue_ThenValueIsUpdated)
{
  create_test_file ({ { test_section, { { "edit_key", "old_val" } } } });
  bool edited = LumexJsonHelper::edit_value (
      current_test_filename_, "edit_key", "new_val", test_section);
  EXPECT_TRUE (edited);
  nlohmann::json loaded = read_file_content ();
  EXPECT_EQ (loaded[test_section]["edit_key"], "new_val");
}

TEST_F (LumexJsonHelperTest,
        GivenNonExistentKeyInFile_WhenEditValue_ThenReturnsFalseAndLogsWarning)
{
  create_test_file ({ { "key", "value" } });
  bool edited = LumexJsonHelper::edit_value (current_test_filename_,
                                             "non_existent_key", "new_val");
  EXPECT_FALSE (edited);
  nlohmann::json loaded = read_file_content ();
  nlohmann::json expected = { { "key", "value" } };
  EXPECT_EQ (loaded, expected);
}

TEST_F (
    LumexJsonHelperTest,
    GivenNonExistentSectionInFile_WhenEditValue_ThenReturnsFalseAndLogsWarning)
{
  create_test_file ({ { "key", "value" } });
  bool edited = LumexJsonHelper::edit_value (
      current_test_filename_, "key_in_section", "val", "NonExistentSection");
  EXPECT_FALSE (edited);
  nlohmann::json loaded = read_file_content ();
  nlohmann::json expected = { { "key", "value" } };
  EXPECT_EQ (loaded, expected);
}

TEST_F (LumexJsonHelperTest,
        GivenEmptyFilename_WhenEditValue_ThenReturnsFalseAndLogsError)
{
  bool edited = LumexJsonHelper::edit_value ("", "key", "value");
  EXPECT_FALSE (edited);
}

TEST_F (LumexJsonHelperTest,
        GivenEmptyStringValue_WhenEditValue_ThenReturnsFalseAndLogsWarning)
{
  create_test_file ({ { "empty_test", "initial" } });
  bool edited
      = LumexJsonHelper::edit_value (current_test_filename_, "empty_test", "");
  EXPECT_FALSE (edited);
  nlohmann::json loaded = read_file_content ();
  EXPECT_EQ (loaded["empty_test"], "initial");
}

// --- functional ---

TEST_F (LumexJsonHelperTest, Functional_CreateWriteReadUpdateValuesInSection)
{
  const std::string section1 = "NetworkSettings";
  const std::string section2 = "Database";
  const std::string key1 = "IPAddress";
  const std::string key2 = "Port";
  const std::string key3 = "Username";
  const std::string key4 = "Password";

  EXPECT_TRUE (LumexJsonHelper::write_value (current_test_filename_, key1,
                                             "192.168.1.1", section1));

  nlohmann::json loaded_config_step2
      = LumexJsonHelper::load_config (current_test_filename_);
  std::string loaded_ip = LumexJsonHelper::get_value (
      loaded_config_step2, key1, "0.0.0.0", section1);
  EXPECT_EQ (loaded_ip, "192.168.1.1");
  EXPECT_TRUE (LumexJsonHelper::has_section (loaded_config_step2, section1));
  EXPECT_TRUE (LumexJsonHelper::has_key (loaded_config_step2, key1, section1));

  EXPECT_TRUE (LumexJsonHelper::write_value (current_test_filename_, key3,
                                             "admin", section2));
  nlohmann::json loaded_config_step3
      = LumexJsonHelper::load_config (current_test_filename_);
  std::string loaded_username
      = LumexJsonHelper::get_value (loaded_config_step3, key3, "", section2);
  EXPECT_EQ (loaded_username, "admin");

  EXPECT_TRUE (LumexJsonHelper::edit_value (current_test_filename_, key1,
                                            "10.0.0.1", section1));
  nlohmann::json loaded_config_step4
      = LumexJsonHelper::load_config (current_test_filename_);
  loaded_ip = LumexJsonHelper::get_value (loaded_config_step4, key1, "0.0.0.0",
                                          section1);
  EXPECT_EQ (loaded_ip, "10.0.0.1");

  EXPECT_TRUE (LumexJsonHelper::write_value (current_test_filename_, key2,
                                             8080, section1));
  nlohmann::json loaded_config_step5
      = LumexJsonHelper::load_config (current_test_filename_);
  int loaded_port
      = LumexJsonHelper::get_value (loaded_config_step5, key2, 0, section1);
  EXPECT_EQ (loaded_port, 8080);

  EXPECT_TRUE (LumexJsonHelper::is_json_file_ok (current_test_filename_));
}

TEST_F (LumexJsonHelperTest, Functional_InvalidFilePathHandling)
{
  const std::string invalid_filename = "/non/existent/path/config.json";
  const std::string key = "TestKey";
  const std::string value = "TestValue";

  EXPECT_FALSE (LumexJsonHelper::write_value (invalid_filename, key, value));

  nlohmann::json loaded_config
      = LumexJsonHelper::load_config (invalid_filename);
  EXPECT_TRUE (loaded_config.empty ());

  EXPECT_FALSE (LumexJsonHelper::edit_value (invalid_filename, key, value));

  EXPECT_FALSE (LumexJsonHelper::is_json_file_ok (invalid_filename));
}

TEST_F (LumexJsonHelperTest, Functional_NestedObjectsAndArrays)
{
  nlohmann::json initial_config
      = { { "level1",
            { { "level2_obj", { { "key_in_level2", "value_L2" } } },
              { "level2_arr", { 1, 2, 3 } },
              { "another_key", true } } } };

  create_test_file (initial_config);

  nlohmann::json loaded_config_step1
      = LumexJsonHelper::load_config (current_test_filename_);
  std::string val_l2 = LumexJsonHelper::get_value (
      loaded_config_step1, "key_in_level2", "", "level1.level2_obj");
  EXPECT_EQ (val_l2, "value_L2");

  bool another_val = LumexJsonHelper::get_value (
      loaded_config_step1, "another_key", false, "level1");
  EXPECT_TRUE (another_val);

  EXPECT_TRUE (LumexJsonHelper::write_value (
      current_test_filename_, "new_key_L2", 42, "level1.level2_obj"));
  nlohmann::json loaded_config_step2
      = LumexJsonHelper::load_config (current_test_filename_);
  int new_val_l2 = LumexJsonHelper::get_value (
      loaded_config_step2, "new_key_L2", 0, "level1.level2_obj");
  EXPECT_EQ (new_val_l2, 42);

  EXPECT_TRUE (LumexJsonHelper::write_value (
      current_test_filename_, "element_in_arr", "text", "level1.level2_arr"));

  nlohmann::json loaded_modified_config
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_TRUE (loaded_modified_config["level1"]["level2_arr"].is_object ());
  EXPECT_EQ (loaded_modified_config["level1"]["level2_arr"]["element_in_arr"],
             "text");

  EXPECT_EQ (loaded_modified_config["level1"]["level2_obj"]["key_in_level2"],
             "value_L2");
  EXPECT_EQ (loaded_modified_config["level1"]["another_key"], true);
}

TEST_F (LumexJsonHelperTest, Functional_LargeDataHandling)
{
  const std::string large_string (1024 * 1024, 'X');
  const std::string key = "LargeData";

  EXPECT_TRUE (LumexJsonHelper::write_value (current_test_filename_, key,
                                             large_string));

  nlohmann::json loaded_config_step1
      = LumexJsonHelper::load_config (current_test_filename_);
  std::string loaded_large_string
      = LumexJsonHelper::get_value (loaded_config_step1, key, "");
  EXPECT_EQ (loaded_large_string, large_string);

  const std::string even_larger_string (2 * 1024 * 1024, 'Y');
  EXPECT_TRUE (LumexJsonHelper::edit_value (current_test_filename_, key,
                                            even_larger_string));

  nlohmann::json loaded_config_step2
      = LumexJsonHelper::load_config (current_test_filename_);
  std::string loaded_even_larger_string
      = LumexJsonHelper::get_value (loaded_config_step2, key, "");
  EXPECT_EQ (loaded_even_larger_string, even_larger_string);
}

TEST_F (LumexJsonHelperTest, Functional_GetValueTypeConversion)
{
  nlohmann::json config_in_memory = { { "str_val", "hello" },
                                      { "int_val", 123 },
                                      { "bool_val", true },
                                      { "double_val", 3.14 },
                                      { "null_val", nullptr } };
  create_test_file (config_in_memory);
  nlohmann::json loaded_config
      = LumexJsonHelper::load_config (current_test_filename_);

  // String
  EXPECT_EQ (
      LumexJsonHelper::get_value (loaded_config, "str_val", std::string ("")),
      "hello");
  EXPECT_EQ (
      LumexJsonHelper::get_value (loaded_config, "int_val", std::string ("")),
      "");

  // Int
  EXPECT_EQ (LumexJsonHelper::get_value (loaded_config, "int_val", 0), 123);
  EXPECT_EQ (LumexJsonHelper::get_value (loaded_config, "str_val", 0), 0);

  // Bool
  EXPECT_EQ (LumexJsonHelper::get_value (loaded_config, "bool_val", false),
             true);

  // Double
  EXPECT_EQ (LumexJsonHelper::get_value (loaded_config, "double_val", 0.0),
             3.14);

  // Null value should return default
  EXPECT_EQ (LumexJsonHelper::get_value (loaded_config, "null_val",
                                         std::string ("default")),
             "default");
}

TEST_F (LumexJsonHelperTest, Functional_SetValueConvertsNonObjectSection)
{
  nlohmann::json config_with_array_section
      = { { "ArraySection", nlohmann::json::array ({ 1, 2, 3 }) } };
  create_test_file (config_with_array_section);

  EXPECT_TRUE (LumexJsonHelper::write_value (current_test_filename_, "NewKey",
                                             "NewValue", "ArraySection"));

  nlohmann::json loaded_config
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_TRUE (loaded_config.contains ("ArraySection"));
  EXPECT_TRUE (loaded_config["ArraySection"].is_object ());
  EXPECT_EQ (loaded_config["ArraySection"]["NewKey"], "NewValue");
}

TEST_F (LumexJsonHelperTest, Functional_CreateSectionLogicInWriteValue)
{
  EXPECT_TRUE (LumexJsonHelper::write_value (current_test_filename_, "key1",
                                             "val1", "SectionA"));
  nlohmann::json config_a
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_TRUE (config_a.contains ("SectionA"));
  EXPECT_TRUE (config_a["SectionA"].is_object ());
  EXPECT_EQ (config_a["SectionA"]["key1"], "val1");

  EXPECT_TRUE (LumexJsonHelper::write_value (current_test_filename_, "key_b",
                                             "val_b", "SectionB"));
  nlohmann::json config_b
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_TRUE (config_b.contains ("SectionB"));
  EXPECT_TRUE (config_b["SectionB"].is_object ());
  EXPECT_EQ (config_b["SectionB"]["key_b"], "val_b");
  EXPECT_EQ (config_b["SectionA"]["key1"], "val1");

  nlohmann::json bad_section_config
      = { { "ExistingNonObjectSection", "some_string_value" } };
  create_test_file (bad_section_config);

  EXPECT_TRUE (LumexJsonHelper::write_value (current_test_filename_,
                                             "nested_key", "nested_val",
                                             "ExistingNonObjectSection"));

  nlohmann::json reloaded_bad_section_config
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_TRUE (
      reloaded_bad_section_config["ExistingNonObjectSection"].is_object ());
  EXPECT_EQ (
      reloaded_bad_section_config["ExistingNonObjectSection"]["nested_key"],
      "nested_val");
}

// --- thread safety ---

TEST_F (LumexJsonHelperTest, ThreadSafety_ConcurrentReadsFromSameFile)
{
  nlohmann::json test_data
      = { { "key1", "value1" }, { "key2", 42 }, { "key3", true } };
  create_test_file (test_data);

  constexpr int num_threads = 10;
  constexpr int reads_per_thread = 100;
  std::vector<std::thread> reader_threads;
  std::atomic<int> successful_reads{ 0 };
  std::atomic<int> failed_reads{ 0 };

  for (int i = 0; i < num_threads; ++i)
    {
      reader_threads.emplace_back ([&] () {
        for (int j = 0; j < reads_per_thread; ++j)
          {
            try
              {
                nlohmann::json loaded
                    = LumexJsonHelper::load_config (current_test_filename_);
                if (!loaded.empty () && loaded.contains ("key1")
                    && loaded.contains ("key2"))
                  {
                    successful_reads++;
                  }
                else
                  {
                    failed_reads++;
                  }
              }
            catch (...)
              {
                failed_reads++;
              }
          }
      });
    }

  for (auto &thread : reader_threads)
    {
      thread.join ();
    }

  EXPECT_EQ (successful_reads.load (), num_threads * reads_per_thread);
  EXPECT_EQ (failed_reads.load (), 0);
}

TEST_F (LumexJsonHelperTest, ThreadSafety_ConcurrentWritesToDifferentSections)
{
  constexpr int num_threads = 8;
  std::vector<std::thread> writer_threads;
  std::atomic<int> successful_writes{ 0 };
  std::atomic<int> failed_writes{ 0 };

  for (int i = 0; i < num_threads; ++i)
    {
      writer_threads.emplace_back ([&, i] () {
        std::string section_name = "Section" + std::to_string (i);
        std::string key_name = "key" + std::to_string (i);
        std::string value = "value" + std::to_string (i);

        try
          {
            bool written = LumexJsonHelper::write_value (
                current_test_filename_, key_name, value, section_name);
            if (written)
              {
                successful_writes++;
              }
            else
              {
                failed_writes++;
              }
          }
        catch (...)
          {
            failed_writes++;
          }
      });
    }

  for (auto &thread : writer_threads)
    {
      thread.join ();
    }

  EXPECT_EQ (successful_writes.load (), num_threads);
  EXPECT_EQ (failed_writes.load (), 0);

  nlohmann::json final_config
      = LumexJsonHelper::load_config (current_test_filename_);
  for (int i = 0; i < num_threads; ++i)
    {
      std::string section_name = "Section" + std::to_string (i);
      std::string key_name = "key" + std::to_string (i);
      std::string expected_value = "value" + std::to_string (i);

      EXPECT_TRUE (final_config.contains (section_name));
      EXPECT_TRUE (final_config[section_name].contains (key_name));
      EXPECT_EQ (final_config[section_name][key_name], expected_value);
    }
}

TEST_F (LumexJsonHelperTest, ThreadSafety_ConcurrentReadsAndWrites)
{
  nlohmann::json initial_data = { { "initial_key", "initial_value" } };
  create_test_file (initial_data);

  constexpr int num_readers = 5;
  constexpr int num_writers = 3;
  constexpr int operations_per_thread = 50;

  std::vector<std::thread> reader_threads;
  std::vector<std::thread> writer_threads;
  std::atomic<int> successful_reads{ 0 };
  std::atomic<int> successful_writes{ 0 };
  std::atomic<int> failed_operations{ 0 };

  for (int i = 0; i < num_readers; ++i)
    {
      reader_threads.emplace_back ([&] () {
        for (int j = 0; j < operations_per_thread; ++j)
          {
            try
              {
                nlohmann::json loaded
                    = LumexJsonHelper::load_config (current_test_filename_);
                if (!loaded.empty ())
                  {
                    successful_reads++;
                  }
                else
                  {
                    failed_operations++;
                  }
              }
            catch (...)
              {
                failed_operations++;
              }
          }
      });
    }

  for (int i = 0; i < num_writers; ++i)
    {
      writer_threads.emplace_back ([&, i] () {
        for (int j = 0; j < operations_per_thread; ++j)
          {
            try
              {
                std::string key = "writer" + std::to_string (i) + "_key"
                                  + std::to_string (j);
                std::string value = "writer" + std::to_string (i) + "_value"
                                    + std::to_string (j);

                bool written = LumexJsonHelper::write_value (
                    current_test_filename_, key, value);
                if (written)
                  {
                    successful_writes++;
                  }
                else
                  {
                    failed_operations++;
                  }
              }
            catch (...)
              {
                failed_operations++;
              }
          }
      });
    }

  for (auto &thread : reader_threads)
    {
      thread.join ();
    }
  for (auto &thread : writer_threads)
    {
      thread.join ();
    }

  EXPECT_EQ (successful_reads.load (), num_readers * operations_per_thread);
  EXPECT_EQ (successful_writes.load (), num_writers * operations_per_thread);
  EXPECT_EQ (failed_operations.load (), 0);

  nlohmann::json final_config
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_TRUE (LumexJsonHelper::is_json_file_ok (current_test_filename_));
  EXPECT_TRUE (final_config.contains ("initial_key"));
  EXPECT_EQ (final_config["initial_key"], "initial_value");
}

TEST_F (LumexJsonHelperTest, ThreadSafety_LargeFileConcurrentOperations)
{
  nlohmann::json large_config;
  for (int i = 0; i < 100; ++i)
    {
      std::string section_name = "Section" + std::to_string (i);
      large_config[section_name] = nlohmann::json::object ();
      for (int j = 0; j < 10; ++j)
        {
          std::string key_name = "key" + std::to_string (j);
          large_config[section_name][key_name]
              = "value" + std::to_string (i) + "_" + std::to_string (j);
        }
    }
  create_test_file (large_config);

  constexpr int num_threads = 6;
  std::vector<std::thread> worker_threads;
  std::atomic<int> successful_operations{ 0 };
  std::atomic<int> failed_operations{ 0 };

  for (int i = 0; i < num_threads; ++i)
    {
      worker_threads.emplace_back ([&, i] () {
        for (int j = 0; j < 20; ++j)
          {
            try
              {
                if (j % 2 == 0)
                  {
                    nlohmann::json loaded = LumexJsonHelper::load_config (
                        current_test_filename_);
                    if (!loaded.empty ())
                      {
                        successful_operations++;
                      }
                    else
                      {
                        failed_operations++;
                      }
                  }
                else
                  {
                    std::string section = "Section" + std::to_string (i);
                    std::string key = "new_key" + std::to_string (j);
                    std::string value = "new_value" + std::to_string (i) + "_"
                                        + std::to_string (j);

                    bool written = LumexJsonHelper::write_value (
                        current_test_filename_, key, value, section);
                    if (written)
                      {
                        successful_operations++;
                      }
                    else
                      {
                        failed_operations++;
                      }
                  }
              }
            catch (...)
              {
                failed_operations++;
              }
          }
      });
    }

  for (auto &thread : worker_threads)
    {
      thread.join ();
    }

  EXPECT_EQ (successful_operations.load (), num_threads * 20);
  EXPECT_EQ (failed_operations.load (), 0);

  EXPECT_TRUE (LumexJsonHelper::is_json_file_ok (current_test_filename_));
}

// --- exception safety ---

TEST_F (LumexJsonHelperTest, ExceptionSafety_LoadConfigWithCorruptedFile)
{
  std::ofstream ofs (current_test_filename_);
  ofs << "{\"key\": \"value\", \"unclosed\": {";
  ofs.close ();

  nlohmann::json result;
  EXPECT_NO_THROW (result
                   = LumexJsonHelper::load_config (current_test_filename_));
  EXPECT_TRUE (result.empty ());
}

TEST_F (LumexJsonHelperTest, ExceptionSafety_SaveConfigWithInvalidPath)
{
  nlohmann::json test_data = { { "key", "value" } };

  std::string invalid_path = "/non/existent/path/file.json";
  bool result = false;

  EXPECT_NO_THROW (result
                   = LumexJsonHelper::save_config (test_data, invalid_path));
  EXPECT_FALSE (result);
}

TEST_F (LumexJsonHelperTest, ExceptionSafety_SetValueWithInvalidJson)
{
  nlohmann::json invalid_config;
  invalid_config["valid_section"] = nlohmann::json::object ();
  invalid_config["valid_section"]["key"] = "value";

  bool result = false;
  EXPECT_NO_THROW (result = LumexJsonHelper::set_value (invalid_config,
                                                        "new_key", "new_value",
                                                        "valid_section"));
  EXPECT_TRUE (result);
}

TEST_F (LumexJsonHelperTest, ExceptionSafety_WriteValueWithFileSystemIssues)
{
  create_test_file ({ { "key", "value" } });

  std::atomic<bool> write_started{ false };
  std::atomic<bool> write_completed{ false };
  std::atomic<bool> write_succeeded{ false };

  std::thread writer_thread ([&] () {
    write_started.store (true);
    bool result = LumexJsonHelper::write_value (current_test_filename_,
                                                "new_key", "new_value");
    write_succeeded.store (result);
    write_completed.store (true);
  });

  while (!write_started.load ())
    {
      std::this_thread::yield ();
    }

  std::remove (current_test_filename_.c_str ());

  writer_thread.join ();

  EXPECT_TRUE (write_completed.load ());
}

TEST_F (LumexJsonHelperTest, ExceptionSafety_EditValueWithMissingKey)
{
  create_test_file ({ { "existing_key", "existing_value" } });

  bool result = false;
  EXPECT_NO_THROW (
      result = LumexJsonHelper::edit_value (current_test_filename_,
                                            "non_existent_key", "new_value"));
  EXPECT_FALSE (result);

  nlohmann::json final_config
      = LumexJsonHelper::load_config (current_test_filename_);
  EXPECT_EQ (final_config["existing_key"], "existing_value");
}

TEST_F (LumexJsonHelperTest, ExceptionSafety_HasSectionWithInvalidInput)
{
  nlohmann::json config = { { "valid_section", { { "key", "value" } } } };

  bool result = false;
  EXPECT_NO_THROW (result = LumexJsonHelper::has_section (config, ""));
  EXPECT_TRUE (result);

  EXPECT_NO_THROW (result
                   = LumexJsonHelper::has_section (config, "valid_section"));
  EXPECT_TRUE (result);

  EXPECT_NO_THROW (result
                   = LumexJsonHelper::has_section (config, "non_existent"));
  EXPECT_FALSE (result);
}

TEST_F (LumexJsonHelperTest, ExceptionSafety_CreateSectionWithInvalidConfig)
{
  nlohmann::json invalid_config = nlohmann::json::array ();

  bool result = false;
  EXPECT_NO_THROW (result = LumexJsonHelper::create_section (invalid_config,
                                                             "new_section"));
  EXPECT_FALSE (result);

  EXPECT_TRUE (invalid_config.is_array ());
}

TEST_F (LumexJsonHelperTest, ExceptionSafety_IsJsonFileOkWithVariousIssues)
{
  bool result = false;
  EXPECT_NO_THROW (
      result = LumexJsonHelper::is_json_file_ok ("/non/existent/file.json"));
  EXPECT_FALSE (result);

  EXPECT_NO_THROW (result = LumexJsonHelper::is_json_file_ok (""));
  EXPECT_FALSE (result);

  std::ofstream ofs (current_test_filename_);
  ofs << "invalid json content";
  ofs.close ();

  EXPECT_NO_THROW (
      result = LumexJsonHelper::is_json_file_ok (current_test_filename_));
  EXPECT_FALSE (result);
}

TEST_F (LumexJsonHelperTest, ExceptionSafety_ComplexOperationChain)
{
  create_test_file ({ { "section1", { { "key1", "value1" } } },
                      { "section2", { { "key2", 42 } } } });

  nlohmann::json config;
  EXPECT_NO_THROW (config
                   = LumexJsonHelper::load_config (current_test_filename_));
  EXPECT_FALSE (config.empty ());

  bool has_section1 = false;
  bool has_section2 = false;
  EXPECT_NO_THROW (has_section1
                   = LumexJsonHelper::has_section (config, "section1"));
  EXPECT_NO_THROW (has_section2
                   = LumexJsonHelper::has_section (config, "section2"));
  EXPECT_TRUE (has_section1);
  EXPECT_TRUE (has_section2);

  std::string value1;
  int value2;
  EXPECT_NO_THROW (
      value1 = LumexJsonHelper::get_value (config, "key1", "", "section1"));
  EXPECT_NO_THROW (
      value2 = LumexJsonHelper::get_value (config, "key2", 0, "section2"));
  EXPECT_EQ (value1, "value1");
  EXPECT_EQ (value2, 42);

  bool section_created = false;
  EXPECT_NO_THROW (section_created
                   = LumexJsonHelper::create_section (config, "section3"));
  EXPECT_TRUE (section_created);

  bool value_set = false;
  EXPECT_NO_THROW (value_set = LumexJsonHelper::set_value (
                       config, "key3", "value3", "section3"));
  EXPECT_TRUE (value_set);

  bool saved = false;
  EXPECT_NO_THROW (
      saved = LumexJsonHelper::save_config (config, current_test_filename_));
  EXPECT_TRUE (saved);

  nlohmann::json final_config;
  EXPECT_NO_THROW (final_config
                   = LumexJsonHelper::load_config (current_test_filename_));
  EXPECT_EQ (final_config["section3"]["key3"], "value3");
}

// --- stress (functional; no wall-clock budget) ---

TEST_F (LumexJsonHelperTest, Stress_LargeNumberOfSectionsAndKeys)
{
  int const num_sections = 100;
  int const keys_per_section = 50;

  nlohmann::json large_config;
  for (int i = 0; i < num_sections; ++i)
    {
      std::string const section_name = "Section" + std::to_string (i);
      large_config[section_name] = nlohmann::json::object ();
      for (int j = 0; j < keys_per_section; ++j)
        large_config[section_name]["key" + std::to_string (j)]
            = "value_" + std::to_string (i) + "_" + std::to_string (j);
    }

  EXPECT_TRUE (
      LumexJsonHelper::save_config (large_config, current_test_filename_));
  nlohmann::json const loaded
      = LumexJsonHelper::load_config (current_test_filename_);
  ASSERT_EQ (loaded.size (), static_cast<std::size_t> (num_sections));
  for (int i = 0; i < num_sections; ++i)
    {
      std::string const section_name = "Section" + std::to_string (i);
      ASSERT_TRUE (loaded.contains (section_name));
      EXPECT_EQ (loaded[section_name].size (),
                 static_cast<std::size_t> (keys_per_section));
    }
  EXPECT_EQ (LumexJsonHelper::get_value (loaded, "key49", "", "Section99"),
             "value_99_49");
}

TEST_F (LumexJsonHelperTest, Stress_DeeplyNestedJsonStructures)
{
  int const nesting_depth = 10;
  int const keys_per_level = 5;

  nlohmann::json deep_config;
  nlohmann::json *current_level = &deep_config;
  std::string deep_path;
  for (int level = 0; level < nesting_depth; ++level)
    {
      std::string const level_name = "Level" + std::to_string (level);
      (*current_level)[level_name] = nlohmann::json::object ();
      for (int key_idx = 0; key_idx < keys_per_level; ++key_idx)
        (*current_level)[level_name]["key" + std::to_string (key_idx)]
            = "value_level" + std::to_string (level) + "_key"
              + std::to_string (key_idx);
      current_level = &((*current_level)[level_name]);
      if (!deep_path.empty ())
        deep_path += ".";
      deep_path += level_name;
    }

  EXPECT_EQ (LumexJsonHelper::get_value (deep_config, "key0", "", deep_path),
             "value_level" + std::to_string (nesting_depth - 1) + "_key0");
  EXPECT_TRUE (
      LumexJsonHelper::save_config (deep_config, current_test_filename_));
  EXPECT_EQ (LumexJsonHelper::load_config (current_test_filename_),
             deep_config);
}

TEST_F (LumexJsonHelperTest, Stress_LargeStringValues)
{
  int const num_large_strings = 20;
  std::size_t const string_size = 100 * 1024;

  nlohmann::json config;
  for (int i = 0; i < num_large_strings; ++i)
    config["LargeString" + std::to_string (i)]
        = std::string (string_size, static_cast<char> ('A' + (i % 26)));

  for (int i = 0; i < num_large_strings; ++i)
    {
      std::string const value = LumexJsonHelper::get_value (
          config, "LargeString" + std::to_string (i), "");
      ASSERT_EQ (value.size (), string_size);
      EXPECT_EQ (value[0], static_cast<char> ('A' + (i % 26)));
    }
  EXPECT_TRUE (LumexJsonHelper::save_config (config, current_test_filename_));
  EXPECT_EQ (LumexJsonHelper::load_config (current_test_filename_).size (),
             static_cast<std::size_t> (num_large_strings));
}

TEST_F (LumexJsonHelperTest, Stress_MultipleOperationsInLoop)
{
  int const num_iterations = 1000;
  nlohmann::json config = nlohmann::json::object ();
  int successful_operations = 0;

  for (int i = 0; i < num_iterations; ++i)
    {
      std::string const section_name = "Section" + std::to_string (i % 10);
      std::string const key_name = "key" + std::to_string (i);
      std::string const value = "value" + std::to_string (i);
      if (LumexJsonHelper::create_section (config, section_name)
          && LumexJsonHelper::set_value (config, key_name, value, section_name)
          && LumexJsonHelper::get_value (config, key_name, "", section_name)
                 == value)
        ++successful_operations;
    }

  EXPECT_EQ (successful_operations, num_iterations);
  EXPECT_TRUE (LumexJsonHelper::save_config (config, current_test_filename_));
  EXPECT_EQ (LumexJsonHelper::load_config (current_test_filename_), config);
}

TEST_F (LumexJsonHelperTest, Stress_MixedTypeArraysAndObjects)
{
  int const num_arrays = 50;
  int const array_size = 100;

  nlohmann::json mixed_config;
  for (int array_idx = 0; array_idx < num_arrays; ++array_idx)
    {
      std::string const array_name = "Array" + std::to_string (array_idx);
      mixed_config[array_name] = nlohmann::json::array ();
      for (int elem_idx = 0; elem_idx < array_size; ++elem_idx)
        {
          if (elem_idx % 4 == 0)
            mixed_config[array_name].push_back ("string_"
                                                + std::to_string (elem_idx));
          else if (elem_idx % 4 == 1)
            mixed_config[array_name].push_back (elem_idx);
          else if (elem_idx % 4 == 2)
            mixed_config[array_name].push_back (elem_idx % 2 == 0);
          else
            mixed_config[array_name].push_back (static_cast<double> (elem_idx)
                                                / 10.0);
        }
    }

  EXPECT_TRUE (
      LumexJsonHelper::save_config (mixed_config, current_test_filename_));
  nlohmann::json const loaded
      = LumexJsonHelper::load_config (current_test_filename_);
  ASSERT_EQ (loaded.size (), static_cast<std::size_t> (num_arrays));
  for (int array_idx = 0; array_idx < num_arrays; ++array_idx)
    {
      nlohmann::json const &array
          = loaded["Array" + std::to_string (array_idx)];
      ASSERT_TRUE (array.is_array ());
      ASSERT_EQ (array.size (), static_cast<std::size_t> (array_size));
      EXPECT_TRUE (array[0].is_string ());
      EXPECT_TRUE (array[1].is_number_integer ());
      EXPECT_TRUE (array[2].is_boolean ());
      EXPECT_TRUE (array[3].is_number_float ());
    }
}

// --- performance (Release only, no sanitizers) ---

TEST_F (LumexJsonHelperTest, Perf_LargeConfigSaveAndLoadWithinBudget)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  nlohmann::json large_config;
  for (int i = 0; i < 100; ++i)
    for (int j = 0; j < 50; ++j)
      large_config["Section" + std::to_string (i)]["key" + std::to_string (j)]
          = "value_" + std::to_string (i) + "_" + std::to_string (j);

  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  EXPECT_TRUE (
      LumexJsonHelper::save_config (large_config, current_test_filename_));
  nlohmann::json const loaded
      = LumexJsonHelper::load_config (current_test_filename_);
  long long const elapsed_ms
      = std::chrono::duration_cast<std::chrono::milliseconds> (
            std::chrono::steady_clock::now () - start)
            .count ();

  EXPECT_EQ (loaded, large_config);
  EXPECT_LT (elapsed_ms, 2000) << "save + load took " << elapsed_ms << " ms";
#else
  GTEST_SKIP () << "wall-clock budget runs only in Release without sanitizers";
#endif
}

// --- diagnostics ---

TEST_F (LumexJsonHelperTest,
        GivenMissingFile_WhenLoadConfig_ThenReportsErrorWithFileName)
{
  EXPECT_TRUE (
      LumexJsonHelper::load_config (current_test_filename_).is_null ());
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::error), 1u);
  EXPECT_TRUE (any_message_contains ("Failed to open config file"));
  EXPECT_TRUE (any_message_contains (current_test_filename_));
}

TEST_F (LumexJsonHelperTest,
        GivenInvalidJson_WhenLoadConfig_ThenReportsParseError)
{
  create_raw_file ("{ \"key\": ");
  EXPECT_TRUE (
      LumexJsonHelper::load_config (current_test_filename_).is_null ());
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::error), 1u);
  EXPECT_TRUE (any_message_contains ("Failed to parse config file"));
}

TEST_F (LumexJsonHelperTest, GivenValidFile_WhenLoadConfig_ThenReportsNothing)
{
  create_test_file (nlohmann::json{ { "a", 1 } });
  EXPECT_FALSE (
      LumexJsonHelper::load_config (current_test_filename_).empty ());
  EXPECT_EQ (captured_count (), 0u);
}

TEST_F (LumexJsonHelperTest,
        GivenParseErrorOnLineThree_WhenIsJsonFileOk_ThenReportsLineAndColumn)
{
  create_raw_file ("{\n  \"a\": 1,\n  \"b\": ]\n}\n");
  EXPECT_FALSE (LumexJsonHelper::is_json_file_ok (current_test_filename_));
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::error), 1u);
  EXPECT_TRUE (any_message_contains ("at line 3"));
}

TEST_F (LumexJsonHelperTest,
        GivenValidFile_WhenIsJsonFileOk_ThenReportsDebugOnly)
{
  create_test_file (nlohmann::json{ { "a", 1 } });
  EXPECT_TRUE (LumexJsonHelper::is_json_file_ok (current_test_filename_));
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::debug), 1u);
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::error), 0u);
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::warning), 0u);
}

TEST_F (LumexJsonHelperTest, GivenNewSection_WhenCreateSection_ThenReportsInfo)
{
  nlohmann::json config = nlohmann::json::object ();
  EXPECT_TRUE (LumexJsonHelper::create_section (config, "Fresh"));
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::info), 1u);
  EXPECT_TRUE (any_message_contains ("Fresh"));
}

TEST_F (LumexJsonHelperTest,
        GivenExistingSection_WhenCreateSection_ThenReportsNothing)
{
  nlohmann::json config = { { "Old", nlohmann::json::object () } };
  EXPECT_TRUE (LumexJsonHelper::create_section (config, "Old"));
  EXPECT_EQ (captured_count (), 0u);
}

TEST_F (LumexJsonHelperTest,
        GivenEmptyStringValue_WhenSetValue_ThenReportsWarningWithKey)
{
  nlohmann::json config = nlohmann::json::object ();
  EXPECT_FALSE (LumexJsonHelper::set_value (config, "blank", std::string ()));
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::warning), 1u);
  EXPECT_TRUE (any_message_contains ("'blank'"));
}

TEST_F (LumexJsonHelperTest,
        GivenMismatchedType_WhenGetValue_ThenReportsWarning)
{
  nlohmann::json const config = { { "n", "text" } };
  EXPECT_EQ (LumexJsonHelper::get_value (config, "n", 7), 7);
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::warning), 1u);
}

TEST_F (LumexJsonHelperTest,
        GivenMissingKey_WhenGetValue_ThenDefaultWithoutDiagnostic)
{
  nlohmann::json const config = { { "n", 1 } };
  EXPECT_EQ (LumexJsonHelper::get_value (config, "absent", 7), 7);
  EXPECT_EQ (captured_count (), 0u);
}

TEST_F (LumexJsonHelperTest,
        GivenSetValueThroughNonObjectStep_WhenCalled_ThenReportsOverwrite)
{
  nlohmann::json config = { { "a", 5 } };
  EXPECT_TRUE (LumexJsonHelper::set_value (config, "k", 1, "a.b"));
  EXPECT_EQ (config["a"]["b"]["k"], 1);
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::warning), 2u);
}

TEST_F (LumexJsonHelperTest,
        GivenMissingKey_WhenEditValue_ThenReportsWarningWithKey)
{
  create_test_file (nlohmann::json{ { "present", 1 } });
  EXPECT_FALSE (
      LumexJsonHelper::edit_value (current_test_filename_, "absent", 2));
  EXPECT_TRUE (any_message_contains ("'absent' not found"));
}

TEST_F (LumexJsonHelperTest,
        GivenNoReporter_WhenErrorReported_ThenGoesToStderrWithoutThrow)
{
  LumexJsonDiagnosticSlot::reset ();
  ::testing::internal::CaptureStderr ();
  EXPECT_TRUE (LumexJsonHelper::load_config ("").is_null ());
  std::string const text = ::testing::internal::GetCapturedStderr ();
  EXPECT_NE (text.find ("[error]"), std::string::npos);
  EXPECT_NE (text.find ("Config filename is empty"), std::string::npos);
}

TEST_F (LumexJsonHelperTest,
        GivenNoReporter_WhenInfoReported_ThenStderrStaysEmpty)
{
  LumexJsonDiagnosticSlot::reset ();
  nlohmann::json config = nlohmann::json::object ();
  ::testing::internal::CaptureStderr ();
  EXPECT_TRUE (LumexJsonHelper::create_section (config, "Quiet"));
  EXPECT_TRUE (::testing::internal::GetCapturedStderr ().empty ());
}

// --- additional contract cases ---

TEST_F (LumexJsonHelperTest,
        GivenDottedName_WhenCreateSection_ThenCreatesLiteralTopLevelKey)
{
  nlohmann::json config = nlohmann::json::object ();
  EXPECT_TRUE (LumexJsonHelper::create_section (config, "a.b"));
  EXPECT_TRUE (config.contains ("a.b"));
  EXPECT_FALSE (config.contains ("a"));
}

TEST_F (LumexJsonHelperTest,
        GivenDottedName_WhenHasSection_ThenLooksUpLiteralTopLevelKey)
{
  nlohmann::json const config = { { "a", { { "b", { { "k", 1 } } } } } };
  EXPECT_FALSE (LumexJsonHelper::has_section (config, "a.b"));
  EXPECT_TRUE (LumexJsonHelper::has_key (config, "k", "a.b"));
}

TEST_F (LumexJsonHelperTest,
        GivenPathWithEmptyComponents_WhenGetValue_ThenEmptyPartsAreSkipped)
{
  nlohmann::json const config = { { "a", { { "b", { { "k", 3 } } } } } };
  EXPECT_EQ (LumexJsonHelper::get_value (config, "k", 0, ".a..b."), 3);
}

TEST_F (LumexJsonHelperTest,
        GivenPathThroughScalar_WhenGetValue_ThenReturnsDefault)
{
  nlohmann::json const config = { { "a", 5 } };
  EXPECT_EQ (LumexJsonHelper::get_value (config, "k", 9, "a.b"), 9);
  EXPECT_FALSE (LumexJsonHelper::has_key (config, "k", "a"));
}

TEST_F (LumexJsonHelperTest,
        GivenNonObjectRoot_WhenReading_ThenBehavesAsEmptyObject)
{
  nlohmann::json const config = nlohmann::json::array ({ 1, 2 });
  EXPECT_FALSE (LumexJsonHelper::has_key (config, "k"));
  EXPECT_EQ (LumexJsonHelper::get_value (config, "k", 4), 4);
  EXPECT_FALSE (LumexJsonHelper::has_section (config, "k"));
}

TEST_F (LumexJsonHelperTest,
        GivenNonObjectRoot_WhenSetValue_ThenRootBecomesObject)
{
  nlohmann::json config = nlohmann::json::array ({ 1, 2 });
  EXPECT_TRUE (LumexJsonHelper::set_value (config, "k", 1));
  EXPECT_TRUE (config.is_object ());
  EXPECT_EQ (config["k"], 1);
}

TEST_F (LumexJsonHelperTest,
        GivenStdStringKey_WhenGetValueWithLiteralDefault_ThenReturnsString)
{
  nlohmann::json const config = { { "name", "x" } };
  std::string const key ("name");
  std::string const missing ("missing");
  EXPECT_EQ (LumexJsonHelper::get_value (config, key, "d"), "x");
  EXPECT_EQ (LumexJsonHelper::get_value (config, missing, "d"), "d");
}

TEST_F (LumexJsonHelperTest,
        GivenNullLiteralDefault_WhenGetValueMissing_ThenReturnsEmptyString)
{
  nlohmann::json const config = nlohmann::json::object ();
  char const *const no_default = nullptr;
  EXPECT_EQ (LumexJsonHelper::get_value (config, "k", no_default),
             std::string ());
}

TEST_F (LumexJsonHelperTest,
        GivenStringValueForLiteralOverload_WhenTypeMismatch_ThenDefault)
{
  nlohmann::json const config = { { "n", 5 } };
  EXPECT_EQ (LumexJsonHelper::get_value (config, "n", "d"), "d");
  EXPECT_EQ (count_level (LumexJsonDiagnosticLevel::warning), 1u);
}

TEST_F (LumexJsonHelperTest,
        GivenCharPointers_WhenEmptyOrNull_ThenIsEmptyValueMatches)
{
  char buffer[] = "";
  char *mutable_empty = buffer;
  char const *const null_text = nullptr;
  EXPECT_TRUE (
      lumex::applied::json::helper::Detail::is_empty_value (mutable_empty));
  EXPECT_TRUE (
      lumex::applied::json::helper::Detail::is_empty_value (null_text));
  EXPECT_TRUE (lumex::applied::json::helper::Detail::is_empty_value (""));
  EXPECT_FALSE (lumex::applied::json::helper::Detail::is_empty_value ("x"));
  EXPECT_FALSE (lumex::applied::json::helper::Detail::is_empty_value (0));
}

#if __cplusplus >= 201703L
TEST_F (LumexJsonHelperTest, GivenStdStringView_WhenEmpty_ThenIsEmptyValue)
{
  EXPECT_TRUE (lumex::applied::json::helper::Detail::is_empty_value (
      std::string_view ()));
  EXPECT_FALSE (lumex::applied::json::helper::Detail::is_empty_value (
      std::string_view ("x")));
}
#endif

TEST_F (LumexJsonHelperTest,
        GivenNonStringValues_WhenSetValue_ThenZeroAndFalseAreStored)
{
  nlohmann::json config = nlohmann::json::object ();
  EXPECT_TRUE (LumexJsonHelper::set_value (config, "zero", 0));
  EXPECT_TRUE (LumexJsonHelper::set_value (config, "no", false));
  EXPECT_TRUE (LumexJsonHelper::set_value (config, "nothing", nullptr));
  EXPECT_EQ (config["zero"], 0);
  EXPECT_EQ (config["no"], false);
  EXPECT_TRUE (config.contains ("nothing"));
}

TEST_F (LumexJsonHelperTest, GivenNullValue_WhenHasKeyStdString_ThenFalse)
{
  nlohmann::json const config = { { "k", nullptr } };
  EXPECT_FALSE (LumexJsonHelper::has_key (config, std::string ("k")));
}

TEST_F (LumexJsonHelperTest, GivenEmptyObjectFile_WhenEditValue_ThenFalse)
{
  create_test_file (nlohmann::json::object ());
  EXPECT_FALSE (LumexJsonHelper::edit_value (current_test_filename_, "k", 1));
  EXPECT_TRUE (any_message_contains ("empty or invalid"));
}

TEST_F (LumexJsonHelperTest,
        GivenMissingFile_WhenEditValue_ThenFileIsNotCreated)
{
  EXPECT_FALSE (LumexJsonHelper::edit_value (current_test_filename_, "k", 1));
  std::ifstream ifs (current_test_filename_.c_str ());
  EXPECT_FALSE (ifs.is_open ());
}

TEST_F (LumexJsonHelperTest,
        GivenSavedConfig_WhenReadRaw_ThenIndentedByFourSpaces)
{
  EXPECT_TRUE (LumexJsonHelper::save_config (nlohmann::json{ { "a", 1 } },
                                             current_test_filename_));
  std::ifstream ifs (current_test_filename_.c_str ());
  std::string const text ((std::istreambuf_iterator<char> (ifs)),
                          std::istreambuf_iterator<char> ());
  EXPECT_EQ (text, "{\n    \"a\": 1\n}\n");
}

TEST_F (LumexJsonHelperTest,
        GivenFileMutex_WhenRequestedTwice_ThenSameRecursiveInstance)
{
  std::recursive_mutex &first = LumexJsonHelper::get_file_mutex ();
  std::recursive_mutex &second = LumexJsonHelper::get_file_mutex ();
  EXPECT_EQ (&first, &second);

  std::lock_guard<std::recursive_mutex> const outer (first);
  // A caller holding the mutex can still use the file operations.
  EXPECT_TRUE (LumexJsonHelper::write_value (current_test_filename_, "k", 1));
  EXPECT_EQ (LumexJsonHelper::load_config (current_test_filename_)["k"], 1);
}

TEST_F (LumexJsonHelperTest,
        GivenFileMutexHeldByCaller_WhenOtherThreadWrites_ThenItWaits)
{
  std::atomic<bool> written (false);
  std::thread writer;
  {
    std::lock_guard<std::recursive_mutex> const lock (
        LumexJsonHelper::get_file_mutex ());
    std::string const filename = current_test_filename_;
    writer = std::thread ([filename, &written] () {
      written.store (LumexJsonHelper::write_value (filename, "k", 1));
    });
    std::this_thread::sleep_for (std::chrono::milliseconds (50));
    EXPECT_FALSE (written.load ());
  }
  writer.join ();
  EXPECT_TRUE (written.load ());
}

TEST_F (LumexJsonHelperTest, GivenPublicApi_WhenInspected_ThenIsNoexcept)
{
  nlohmann::json config;
  nlohmann::json const const_config;
  std::string const name ("x");
  static_assert (noexcept (LumexJsonHelper::load_config (name)), "noexcept");
  static_assert (noexcept (LumexJsonHelper::save_config (config, name)),
                 "noexcept");
  static_assert (noexcept (LumexJsonHelper::is_json_file_ok (name)),
                 "noexcept");
  static_assert (noexcept (LumexJsonHelper::has_section (const_config, name)),
                 "noexcept");
  static_assert (noexcept (LumexJsonHelper::create_section (config, name)),
                 "noexcept");
  static_assert (noexcept (LumexJsonHelper::has_key (const_config, name)),
                 "noexcept");
  static_assert (noexcept (LumexJsonHelper::get_value (const_config, name, 1)),
                 "noexcept");
  static_assert (noexcept (LumexJsonHelper::set_value (config, name, 1)),
                 "noexcept");
  static_assert (noexcept (LumexJsonHelper::write_value (name, name, 1)),
                 "noexcept");
  static_assert (noexcept (LumexJsonHelper::edit_value (name, name, 1)),
                 "noexcept");
  static_assert (!std::is_default_constructible<LumexJsonHelper>::value,
                 "static-only");
  SUCCEED ();
}
