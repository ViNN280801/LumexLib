#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "lumex/applied/logger/LumexLogger"
#include "lumex/applied/logger/config/LumexLoggerConfigFormat.hpp"

using namespace lumex::applied::logger::logger;

namespace lumex
{
namespace examples
{
namespace logger
{
struct temporary_config_file_t
{
  temporary_config_file_t (std::string file_path, std::string const &content)
      : path (std::move (file_path))
  {
    std::remove (path.c_str ());
    std::ofstream output (path.c_str (), std::ios::out | std::ios::trunc);
    if (!output.is_open ())
      throw std::runtime_error ("cannot create example config: " + path);
    output << content;
    if (!output)
      throw std::runtime_error ("cannot write example config: " + path);
  }

  ~temporary_config_file_t () { std::remove (path.c_str ()); }

  temporary_config_file_t (temporary_config_file_t const &) = delete;
  temporary_config_file_t &operator= (temporary_config_file_t const &)
      = delete;

  std::string path;
};

static void
require (bool condition, std::string const &message)
{
  if (!condition)
    throw std::runtime_error (message);
}

static void
verify_full_config (logger_config_t const &config, std::string const &format)
{
  require (config.log_level == LogLevel::LEVEL_WARNING, format + ": LEVEL");
  require (config.func_name_mode == FunctionNameMode::NORMAL,
           format + ": FUNCNAME");
  require (config.use_timestamped_logs, format + ": TIMESTAMPED");
  require (config.show_stack_trace, format + ": STACKTRACE");
  require (config.stack_trace_max_frames == 12,
           format + ": STACKTRACE_FRAMES");
  require (!config.create_hint_file, format + ": HINT");
  require (config.preset_components.size () == 2U, format + ": PRESET size");
  require (config.preset_components.count ("NetworkManager") == 1U,
           format + ": PRESET NetworkManager");
  require (config.preset_components.count ("DatabaseConnection") == 1U,
           format + ": PRESET DatabaseConnection");
  require (config.buffering_enabled, format + ": BUFFERING");
  require (config.describe_frame, format + ": DESCRIBE_FRAME");
  require (config.buffer_size == 240U, format + ": BUFFER_SIZE");
  require (config.buffering_trigger_configured,
           format + ": BUFFERING_TRIGGER configured");
  require (config.buffering_trigger_level == LogLevel::LEVEL_ERROR,
           format + ": BUFFERING_TRIGGER level");
}

#if defined(LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT)
static void
show_compiled_format ()
{
  require (::lumex::applied::logger::configured_logger_config_format ()
               == ::lumex::applied::logger::logger_config_format_t::plain_text,
           "plain text: compiled format");
  temporary_config_file_t const file (
      "lumex_logger_example.cfg", "LEVEL=WARNING\n"
                                  "FUNCNAME=NORMAL\n"
                                  "TIMESTAMPED=true\n"
                                  "STACKTRACE=true\n"
                                  "STACKTRACE_FRAMES=12\n"
                                  "HINT=false\n"
                                  "PRESET=NetworkManager,DatabaseConnection\n"
                                  "BUFFERING=true\n"
                                  "DESCRIBE_FRAME=true\n"
                                  "BUFFER_SIZE=240\n"
                                  "BUFFERING_TRIGGER=ERROR\n");
  verify_full_config (LumexLogger::read_config_from_path (file.path),
                      "plain text");
  std::cout << "compiled format: PLAIN_TEXT\n";
}
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_INI)
static void
show_compiled_format ()
{
  require (::lumex::applied::logger::configured_logger_config_format ()
               == ::lumex::applied::logger::logger_config_format_t::ini,
           "INI: compiled format");
  temporary_config_file_t const file (
      "lumex_logger_example.ini",
      "[logger]\n"
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
      "BUFFERING_TRIGGER=ERROR\n");
  verify_full_config (LumexLogger::read_config_from_path (file.path), "INI");
  std::cout << "compiled format: INI via LumexSettingsINI\n";
}
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)
static void
show_compiled_format ()
{
  require (::lumex::applied::logger::configured_logger_config_format ()
               == ::lumex::applied::logger::logger_config_format_t::json,
           "JSON: compiled format");
  temporary_config_file_t const file (
      "lumex_logger_example.json",
      "{\n"
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
      "}\n");
  verify_full_config (LumexLogger::read_config_from_path (file.path), "JSON");
  std::cout << "compiled format: JSON via nlohmann/json\n";
}
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_YAML)
static void
show_compiled_format ()
{
  require (::lumex::applied::logger::configured_logger_config_format ()
               == ::lumex::applied::logger::logger_config_format_t::yaml,
           "YAML: compiled format");
  bool expected_exception = false;
  try
    {
      (void)LumexLogger::read_config_from_path ("lumex_logger_example.yaml");
    }
  catch (std::runtime_error const &error)
    {
      std::string const expected
          = "logger config YAML is selected "
            "(LUMEX_LOGGER_CONFIG_FORMAT=YAML) but LumexSettingsYAML "
            "is not implemented yet";
      expected_exception = error.what () == expected;
      std::cout << "YAML: expected unimplemented exception: " << error.what ()
                << '\n';
    }
  require (expected_exception, "YAML: exact unimplemented exception");
  std::cout << "compiled format: YAML (reserved, throws)\n";
}
#else
static void
show_compiled_format ()
{
  require (::lumex::applied::logger::configured_logger_config_format ()
               == ::lumex::applied::logger::logger_config_format_t::xml,
           "XML: compiled format");
  temporary_config_file_t const file (
      "lumex_logger_example.xml",
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
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
      "</logger>\n");
  verify_full_config (LumexLogger::read_config_from_path (file.path), "XML");
  std::cout << "compiled format: XML via LumexXml\n";
}
#endif
} // namespace logger
} // namespace examples
} // namespace lumex

int
main ()
{
  std::cout << "=== LumexLogger configuration formats ===\n\n";
  try
    {
      lumex::examples::logger::show_compiled_format ();
    }
  catch (std::exception const &error)
    {
      std::cerr << "config format example failed: " << error.what () << '\n';
      return 1;
    }

  std::cout << "\n=== Configuration format example finished ===\n";
  return 0;
}
