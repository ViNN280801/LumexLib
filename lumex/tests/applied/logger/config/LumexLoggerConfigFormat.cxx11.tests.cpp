// Tests of the logger config file formats (lumex/applied/logger/config):
// logger_config_t read through LumexLogger::read_config_from_path for the
// plain text, INI, JSON, XML and YAML formats, whichever the logger was
// compiled with (LUMEX_LOGGER_CONFIG_FORMAT_*). Every suite of this directory
// compiles this file, so the standard-dependent branches of the logger
// headers are all built and run. The tests read and write files of their
// own and never touch the singleton's enabled state.

#include <fstream>
#include <stdexcept>

#include <cstdio>

#include <gtest/gtest.h>

#include "lumex/applied/logger/LumexLogger"
#include "lumex/applied/logger/config/LumexLoggerConfigFormat.hpp"
#include "lumex/core/utility/assert/LumexAssert.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

using namespace lumex::applied::logger::logger;

// --- Compile-time config format -------------------------------------------

namespace
{
void
expect_full_logger_config (logger_config_t const &config)
{
  EXPECT_EQ (config.log_level, LogLevel::LEVEL_WARNING);
  EXPECT_EQ (config.func_name_mode, FunctionNameMode::NORMAL);
  EXPECT_TRUE (config.use_timestamped_logs);
  EXPECT_TRUE (config.show_stack_trace);
  EXPECT_EQ (config.stack_trace_max_frames, 12);
  EXPECT_FALSE (config.create_hint_file);
  EXPECT_EQ (config.preset_components.size (), 2U);
  EXPECT_TRUE (config.preset_components.count ("NetworkManager") > 0);
  EXPECT_TRUE (config.preset_components.count ("DatabaseConnection") > 0);
  EXPECT_TRUE (config.buffering_enabled);
  EXPECT_TRUE (config.describe_frame);
  EXPECT_EQ (config.buffer_size, 240U);
  EXPECT_TRUE (config.buffering_trigger_configured);
  EXPECT_EQ (config.buffering_trigger_level, LogLevel::LEVEL_ERROR);
}

void
expect_default_logger_config (logger_config_t const &config)
{
  EXPECT_EQ (config.log_level, LogLevel::LEVEL_INFO);
  EXPECT_EQ (config.func_name_mode, FunctionNameMode::FULL);
  EXPECT_FALSE (config.use_timestamped_logs);
  EXPECT_FALSE (config.show_stack_trace);
  EXPECT_EQ (config.stack_trace_max_frames,
             logger_config_t::kMaxStackTraceFrames);
  EXPECT_TRUE (config.create_hint_file);
  EXPECT_TRUE (config.preset_components.empty ());
  EXPECT_FALSE (config.buffering_enabled);
  EXPECT_FALSE (config.describe_frame);
  EXPECT_EQ (config.buffer_size, logger_config_t::kBufferSize);
  EXPECT_FALSE (config.buffering_trigger_configured);
  EXPECT_EQ (config.buffering_trigger_level, LogLevel::LEVEL_WARNING);
}

bool
write_text_file (char const *path, char const *content)
{
  std::remove (path);
  std::ofstream out (path, std::ios::out | std::ios::trunc);
  if (!out.is_open ())
    return false;
  out << content;
  return static_cast<bool> (out);
}
} // namespace

TEST (LumexLoggerConfigFormat, ConfiguredFormatMatchesCompileTimeSelection)
{
  using lumex::applied::logger::configured_logger_config_format;
  using lumex::applied::logger::logger_config_format_t;

#if defined(LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT)
  EXPECT_EQ (configured_logger_config_format (),
             logger_config_format_t::plain_text);
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_INI)
  EXPECT_EQ (configured_logger_config_format (), logger_config_format_t::ini);
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)
  EXPECT_EQ (configured_logger_config_format (), logger_config_format_t::json);
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_YAML)
  EXPECT_EQ (configured_logger_config_format (), logger_config_format_t::yaml);
#else
  EXPECT_EQ (configured_logger_config_format (), logger_config_format_t::xml);
#endif
}

#if defined(LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT)
TEST (LumexLoggerConfigFormat, PlainTextReadConfigFromPath)
{
  char const *const path = "lumex_logger_cfg_plain_text.tmp";
  ASSERT_TRUE (write_text_file (path,
                                "LEVEL=WARNING\n"
                                "FUNCNAME=NORMAL\n"
                                "TIMESTAMPED=true\n"
                                "STACKTRACE=true\n"
                                "STACKTRACE_FRAMES=12\n"
                                "HINT=false\n"
                                "PRESET=NetworkManager,DatabaseConnection\n"
                                "BUFFERING=true\n"
                                "DESCRIBE_FRAME=true\n"
                                "BUFFER_SIZE=240\n"
                                "BUFFERING_TRIGGER=ERROR\n"));
  expect_full_logger_config (LumexLogger::read_config_from_path (path));
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, MissingFileReturnsDefaults)
{
  char const *const path = "lumex_logger_cfg_missing.tmp";
  std::remove (path);
  expect_default_logger_config (LumexLogger::read_config_from_path (path));
}

TEST (LumexLoggerConfigFormat, XmlMarkupIsNotParsedAsPlainText)
{
  char const *const path = "lumex_logger_cfg_plain_vs_xml.tmp";
  ASSERT_TRUE (
      write_text_file (path, "<logger><LEVEL>ERROR</LEVEL></logger>\n"));
  expect_default_logger_config (LumexLogger::read_config_from_path (path));
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, UnknownKey_WhenUnfound_ThenKnownKeysStillApply)
{
  char const *const path = "lumex_logger_cfg_unknown_key.tmp";
  ASSERT_TRUE (write_text_file (path, "LEVEL=ERROR\n"
                                      "NOT_A_REAL_KEY=zzz\n"
                                      "BUFFER_SIZE=17\n"));
  logger_config_t const cfg = LumexLogger::read_config_from_path (path);
  EXPECT_EQ (cfg.log_level, LogLevel::LEVEL_ERROR);
  EXPECT_EQ (cfg.buffer_size, 17U);
  EXPECT_EQ (cfg.func_name_mode, FunctionNameMode::FULL);
  EXPECT_TRUE (cfg.preset_components.empty ());
  std::remove (path);
}

TEST (LumexLoggerConfigFormat,
      LegacyPositional_WhenNoEquals_ThenKnownKeysApply)
{
  char const *const path = "lumex_logger_cfg_legacy_positional.tmp";
  ASSERT_TRUE (write_text_file (path, "WARNING\n"
                                      "NORMAL\n"
                                      "true\n"));
  logger_config_t const cfg = LumexLogger::read_config_from_path (path);
  EXPECT_EQ (cfg.log_level, LogLevel::LEVEL_WARNING);
  EXPECT_EQ (cfg.func_name_mode, FunctionNameMode::NORMAL);
  EXPECT_TRUE (cfg.use_timestamped_logs);
  std::remove (path);
}
#endif

#if defined(LUMEX_LOGGER_CONFIG_FORMAT_INI)
TEST (LumexLoggerConfigFormat, IniReadConfigFromPath)
{
  char const *const path = "lumex_logger_cfg_ini.tmp.ini";
  ASSERT_TRUE (
      write_text_file (path, "[logger]\n"
                             "LEVEL=WARNING\n"
                             "FUNCNAME=NORMAL\n"
                             "TIMESTAMPED=true\n"
                             "STACKTRACE=true\n"
                             "STACKTRACE_FRAMES=12\n"
                             "HINT=false\n"
                             "PRESET=\"NetworkManager,DatabaseConnection\"\n"
                             "BUFFERING=true\n"
                             "DESCRIBE_FRAME=true\n"
                             "BUFFER_SIZE=240\n"
                             "BUFFERING_TRIGGER=ERROR\n"));
  expect_full_logger_config (LumexLogger::read_config_from_path (path));
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, MissingFileReturnsDefaults)
{
  char const *const path = "lumex_logger_cfg_missing.tmp.ini";
  std::remove (path);
  expect_default_logger_config (LumexLogger::read_config_from_path (path));
}

TEST (LumexLoggerConfigFormat, SectionOtherThanLoggerIsIgnored)
{
  char const *const path = "lumex_logger_cfg_wrong_section.tmp.ini";
  ASSERT_TRUE (
      write_text_file (path, "[other]\nLEVEL=ERROR\nBUFFER_SIZE=17\n"));
  expect_default_logger_config (LumexLogger::read_config_from_path (path));
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, UnknownKey_WhenUnfound_ThenKnownKeysStillApply)
{
  char const *const path = "lumex_logger_cfg_unknown_key.tmp.ini";
  ASSERT_TRUE (write_text_file (path, "[logger]\n"
                                      "LEVEL=ERROR\n"
                                      "NOT_A_REAL_KEY=zzz\n"
                                      "BUFFER_SIZE=17\n"));
  logger_config_t const cfg = LumexLogger::read_config_from_path (path);
  EXPECT_EQ (cfg.log_level, LogLevel::LEVEL_ERROR);
  EXPECT_EQ (cfg.buffer_size, 17U);
  EXPECT_EQ (cfg.func_name_mode, FunctionNameMode::FULL);
  EXPECT_TRUE (cfg.preset_components.empty ());
  std::remove (path);
}
#endif

#if defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)
TEST (LumexLoggerConfigFormat, JsonReadConfigFromPath)
{
  char const *const path = "lumex_logger_cfg_json.tmp.json";
  ASSERT_TRUE (write_text_file (
      path, "{\n"
            "  \"logger\": {\n"
            "    \"LEVEL\": \"WARNING\",\n"
            "    \"FUNCNAME\": \"NORMAL\",\n"
            "    \"TIMESTAMPED\": true,\n"
            "    \"STACKTRACE\": true,\n"
            "    \"STACKTRACE_FRAMES\": 12,\n"
            "    \"HINT\": false,\n"
            "    \"PRESET\": \"NetworkManager,DatabaseConnection\",\n"
            "    \"BUFFERING\": true,\n"
            "    \"DESCRIBE_FRAME\": true,\n"
            "    \"BUFFER_SIZE\": 240,\n"
            "    \"BUFFERING_TRIGGER\": \"ERROR\"\n"
            "  }\n"
            "}\n"));
  expect_full_logger_config (LumexLogger::read_config_from_path (path));
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, JsonRootObjectIsAccepted)
{
  char const *const path = "lumex_logger_cfg_json_root.tmp.json";
  ASSERT_TRUE (
      write_text_file (path, "{\"LEVEL\":\"ERROR\",\"BUFFER_SIZE\":17}\n"));
  logger_config_t const cfg = LumexLogger::read_config_from_path (path);
  EXPECT_EQ (cfg.log_level, LogLevel::LEVEL_ERROR);
  EXPECT_EQ (cfg.buffer_size, 17U);
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, MalformedJsonReturnsDefaults)
{
  char const *const path = "lumex_logger_cfg_bad.tmp.json";
  ASSERT_TRUE (write_text_file (path, "{\"logger\":"));
  expect_default_logger_config (LumexLogger::read_config_from_path (path));
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, MissingFileReturnsDefaults)
{
  char const *const path = "lumex_logger_cfg_missing.tmp.json";
  std::remove (path);
  expect_default_logger_config (LumexLogger::read_config_from_path (path));
}

TEST (LumexLoggerConfigFormat, UnknownKey_WhenUnfound_ThenKnownKeysStillApply)
{
  char const *const path = "lumex_logger_cfg_unknown_key.tmp.json";
  ASSERT_TRUE (write_text_file (
      path, "{\"logger\":{\"LEVEL\":\"ERROR\",\"NOT_A_REAL_KEY\":\"zzz\","
            "\"BUFFER_SIZE\":17}}\n"));
  logger_config_t const cfg = LumexLogger::read_config_from_path (path);
  EXPECT_EQ (cfg.log_level, LogLevel::LEVEL_ERROR);
  EXPECT_EQ (cfg.buffer_size, 17U);
  EXPECT_EQ (cfg.func_name_mode, FunctionNameMode::FULL);
  EXPECT_TRUE (cfg.preset_components.empty ());
  std::remove (path);
}
#endif

#if defined(LUMEX_LOGGER_CONFIG_FORMAT_YAML)
TEST (LumexLoggerConfigFormat, YamlReadThrowsUnimplemented)
{
  try
    {
      (void)LumexLogger::read_config_from_path ("enable_logs.yaml");
      FAIL () << "YAML reader must report the unimplemented backend";
    }
  catch (std::runtime_error const &error)
    {
      EXPECT_STREQ (error.what (),
                    "logger config YAML is selected "
                    "(LUMEX_LOGGER_CONFIG_FORMAT=YAML) but LumexSettingsYAML "
                    "is not implemented yet");
    }
}

TEST (LumexLoggerConfigFormat, MissingYamlFileStillThrowsUnimplemented)
{
  char const *const path = "lumex_logger_cfg_missing.tmp.yaml";
  std::remove (path);
  EXPECT_THROW (LumexLogger::read_config_from_path (path), std::runtime_error);
}
#endif

#if defined(LUMEX_LOGGER_CONFIG_FORMAT_XML)
TEST (LumexLoggerConfigFormat, XmlReadConfigFromPath)
{
  char const *const path = "lumex_logger_cfg_xml.tmp.xml";
  ASSERT_TRUE (write_text_file (
      path, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<logger>\n"
            "  <LEVEL>WARNING</LEVEL>\n"
            "  <FUNCNAME>NORMAL</FUNCNAME>\n"
            "  <TIMESTAMPED>true</TIMESTAMPED>\n"
            "  <STACKTRACE>true</STACKTRACE>\n"
            "  <STACKTRACE_FRAMES>12</STACKTRACE_FRAMES>\n"
            "  <HINT>false</HINT>\n"
            "  <PRESET>NetworkManager,DatabaseConnection</PRESET>\n"
            "  <BUFFERING>true</BUFFERING>\n"
            "  <DESCRIBE_FRAME>true</DESCRIBE_FRAME>\n"
            "  <BUFFER_SIZE>240</BUFFER_SIZE>\n"
            "  <BUFFERING_TRIGGER>ERROR</BUFFERING_TRIGGER>\n"
            "</logger>\n"));
  expect_full_logger_config (LumexLogger::read_config_from_path (path));
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, XmlWithoutLoggerRootReturnsDefaults)
{
  char const *const path = "lumex_logger_cfg_wrong_root.tmp.xml";
  ASSERT_TRUE (write_text_file (
      path, "<configuration><LEVEL>ERROR</LEVEL></configuration>\n"));
  expect_default_logger_config (LumexLogger::read_config_from_path (path));
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, MalformedXmlReturnsDefaults)
{
  char const *const path = "lumex_logger_cfg_bad.tmp.xml";
  ASSERT_TRUE (write_text_file (path, "<logger><LEVEL>ERROR"));
  expect_default_logger_config (LumexLogger::read_config_from_path (path));
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, MissingFileReturnsDefaults)
{
  char const *const path = "lumex_logger_cfg_missing.tmp.xml";
  std::remove (path);
  expect_default_logger_config (LumexLogger::read_config_from_path (path));
}

TEST (LumexLoggerConfigFormat, PlainTextContentIsNotParsedAsXml)
{
  char const *const path = "lumex_logger_cfg_xml_vs_plain.tmp.xml";
  ASSERT_TRUE (write_text_file (path, "LEVEL=ERROR\nBUFFER_SIZE=17\n"));
  expect_default_logger_config (LumexLogger::read_config_from_path (path));
  std::remove (path);
}

TEST (LumexLoggerConfigFormat, UnknownKey_WhenUnfound_ThenKnownKeysStillApply)
{
  char const *const path = "lumex_logger_cfg_unknown_key.tmp.xml";
  ASSERT_TRUE (write_text_file (path,
                                "<logger>\n"
                                "  <LEVEL>ERROR</LEVEL>\n"
                                "  <NOT_A_REAL_KEY>zzz</NOT_A_REAL_KEY>\n"
                                "  <BUFFER_SIZE>17</BUFFER_SIZE>\n"
                                "</logger>\n"));
  logger_config_t const cfg = LumexLogger::read_config_from_path (path);
  EXPECT_EQ (cfg.log_level, LogLevel::LEVEL_ERROR);
  EXPECT_EQ (cfg.buffer_size, 17U);
  EXPECT_EQ (cfg.func_name_mode, FunctionNameMode::FULL);
  EXPECT_TRUE (cfg.preset_components.empty ());
  std::remove (path);
}
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
