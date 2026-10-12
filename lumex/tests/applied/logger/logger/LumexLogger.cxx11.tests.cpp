// LumexLogger.cxx11.tests.cpp: every suite of this directory (C++11, C++14,
// C++17, C++20) compiles it, so the standard-dependent branches of the logger
// headers are all built and run. The config file format tests are in
// ../config/LumexLoggerConfigFormat.cxx11.tests.cpp.
//
// Test scope note: LumexLogger is a Meyer's singleton whose "logging enabled"
// state (and most of its config-file-driven fields:
// STACKTRACE/TIMESTAMPED/BUFFERING_TRIGGER/PRESET/BUFFER_SIZE from the config
// file) is fixed once, at the very first call to LumexLogger::get_instance()
// in the process, based on whether an `enable_logs` trigger file happens to
// exist next to the running test executable. Since this test binary's
// RUNTIME_OUTPUT_DIRECTORY is shared with sibling test executables and
// `gtest_discover_tests` spawns one process per registered TEST case (so
// several instances of this very binary can run concurrently under `ctest
// -j`), deliberately creating/removing that trigger file from within a test
// would race across those parallel process instances. To keep this suite
// deterministic and CI-safe, it does NOT attempt to flip the singleton into
// the "enabled" state; instead it thoroughly covers everything that is
// reachable and deterministic regardless of that state: the pure
// formatting/parsing-adjacent free functions, the enum surface (including the
// new `FunctionNameMode::NORMAL`), and full round-trips of the programmatic
// public API (buffering, presets, log level, trigger file name,
// `get_applied_config_view()`), always saving and restoring any global
// singleton state a test touches so test order never matters.

#include <memory>
#include <string>
#include <type_traits>
#include <unordered_set>

#include <cstdint>

#include <gtest/gtest.h>

#include "lumex/applied/logger/LumexLogger"
#include "lumex/applied/logger/config/LumexLoggerConfigFormat.hpp"
#include "lumex/core/utility/assert/LumexAssert.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

using namespace lumex::applied::logger::logger;

// --- Naming / namespace ---------------------------------------------------

TEST (LumexLoggerNaming, GlobalAliasMatchesNamespacedClass)
{
  // CoT: the header exposes a global `using LumexLogger =
  // lumex::applied::logger::logger::LumexLogger;` alias, matching the
  // LumexEnvironment/LumexLogging convention -> the two names must refer to
  // the exact same type.
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<LumexLogger,
                    lumex::applied::logger::logger::LumexLogger>::value),
      "LumexLogger alias must be the same type as "
      "lumex::applied::logger::logger::LumexLogger");
  SUCCEED ();
}

TEST (LumexLoggerNaming, GetInstanceReturnsTheSameSingletonAcrossCalls)
{
  LumexLogger &first = LumexLogger::get_instance ();
  LumexLogger &second = LumexLogger::get_instance ();
  EXPECT_EQ (std::addressof (first), std::addressof (second));
}

// --- LogLevel --------------------------------------------------------------

TEST (LogLevelTest, LevelsAreOrderedFromTraceToFatal)
{
  EXPECT_LT (static_cast<uint8_t> (LogLevel::LEVEL_TRACE),
             static_cast<uint8_t> (LogLevel::LEVEL_DEBUG));
  EXPECT_LT (static_cast<uint8_t> (LogLevel::LEVEL_DEBUG),
             static_cast<uint8_t> (LogLevel::LEVEL_INFO));
  EXPECT_LT (static_cast<uint8_t> (LogLevel::LEVEL_INFO),
             static_cast<uint8_t> (LogLevel::LEVEL_SUCCESS));
  EXPECT_LT (static_cast<uint8_t> (LogLevel::LEVEL_SUCCESS),
             static_cast<uint8_t> (LogLevel::LEVEL_WARNING));
  EXPECT_LT (static_cast<uint8_t> (LogLevel::LEVEL_WARNING),
             static_cast<uint8_t> (LogLevel::LEVEL_ERROR));
  EXPECT_LT (static_cast<uint8_t> (LogLevel::LEVEL_ERROR),
             static_cast<uint8_t> (LogLevel::LEVEL_FATAL));
}

TEST (LogLevelTest, LogLevelToStringMapsKnownLevels)
{
  EXPECT_EQ (log_level_to_string (LogLevel::LEVEL_TRACE), "LEVEL_TRACE");
  EXPECT_EQ (log_level_to_string (LogLevel::LEVEL_DEBUG), "LEVEL_DEBUG");
  EXPECT_EQ (log_level_to_string (LogLevel::LEVEL_INFO), "LEVEL_INFO");
  EXPECT_EQ (log_level_to_string (LogLevel::LEVEL_SUCCESS), "LEVEL_SUCCESS");
  EXPECT_EQ (log_level_to_string (LogLevel::LEVEL_WARNING), "LEVEL_WARNING");
  EXPECT_EQ (log_level_to_string (LogLevel::LEVEL_ERROR), "LEVEL_ERROR");
  EXPECT_EQ (log_level_to_string (LogLevel::LEVEL_FATAL), "LEVEL_FATAL");
}

TEST (LogLevelTest, LogLevelToStringFallsBackToUnknownForOutOfRangeValue)
{
  auto bogus = static_cast<LogLevel> (255);
  EXPECT_EQ (log_level_to_string (bogus), "UNKNOWN");
}

// --- FunctionNameMode (new NORMAL mode, item 2 of the migration) -----------

TEST (FunctionNameModeTest, HasFourDistinctModesIncludingNormal)
{
  EXPECT_NE (FunctionNameMode::NONE, FunctionNameMode::SHORT);
  EXPECT_NE (FunctionNameMode::SHORT, FunctionNameMode::FULL);
  EXPECT_NE (FunctionNameMode::FULL, FunctionNameMode::NORMAL);
  EXPECT_NE (FunctionNameMode::NORMAL, FunctionNameMode::NONE);

  // Regression-pin the documented numeric values (see LumexLogger.hpp doc
  // comment).
  EXPECT_EQ (static_cast<uint8_t> (FunctionNameMode::NONE), 0);
  EXPECT_EQ (static_cast<uint8_t> (FunctionNameMode::SHORT), 1);
  EXPECT_EQ (static_cast<uint8_t> (FunctionNameMode::FULL), 2);
  EXPECT_EQ (static_cast<uint8_t> (FunctionNameMode::NORMAL), 3);
}

// --- logger_stringify (hygienic, prefixed name - item 3 of the migration) --

TEST (LoggerStringifyTest, EmptyArgsProduceEmptyString)
{
  EXPECT_EQ (logger_stringify (), "");
}

TEST (LoggerStringifyTest, ConcatenatesHeterogeneousStreamableArguments)
{
  std::string result
      = logger_stringify ("value=", 42, ", pi~", 3.5, ", flag=", true);
  EXPECT_EQ (result, "value=42, pi~3.5, flag=1");
}

TEST (LoggerStringifyTest, ConcatenatesPlainStrings)
{
  EXPECT_EQ (
      logger_stringify ("[", std::string ("Component"), "]: ", "message"),
      "[Component]: message");
}

// --- logger_format_function_name_clean (NORMAL mode formatting helper)
// --------

TEST (LoggerFormatFunctionNameCleanTest, NullptrYieldsEmptyString)
{
  EXPECT_EQ (logger_format_function_name_clean (nullptr), "");
}

TEST (LoggerFormatFunctionNameCleanTest, EmptyStringYieldsEmptyString)
{
  EXPECT_EQ (logger_format_function_name_clean (""), "");
}

TEST (LoggerFormatFunctionNameCleanTest,
      StripsMsvcStyleReturnTypeCallingConventionAndArguments)
{
  // Synthetic __FUNCSIG__-like input, as produced by MSVC.
  char const *prettySig
      = "bool __cdecl MyNamespace::MyClass::DoWork(int, char const *)";
  EXPECT_EQ (logger_format_function_name_clean (prettySig),
             "MyNamespace::MyClass::DoWork()");
}

TEST (LoggerFormatFunctionNameCleanTest, StripsGccStyleReturnTypeAndArguments)
{
  // Synthetic __PRETTY_FUNCTION__-like input, as produced by GCC/Clang.
  char const *prettySig = "void MyNamespace::MyClass::doWork()";
  EXPECT_EQ (logger_format_function_name_clean (prettySig),
             "MyNamespace::MyClass::doWork()");
}

TEST (LoggerFormatFunctionNameCleanTest,
      FreeFunctionWithoutClassKeepsJustTheFunctionName)
{
  char const *prettySig = "int __cdecl compute(int)";
  EXPECT_EQ (logger_format_function_name_clean (prettySig), "compute()");
}

// The spelling of the full signature depends on the compiler, not on the
// target: __FUNCSIG__ exists only in MSVC and clang-cl, so GCC and Clang on
// Windows (MinGW) must get __PRETTY_FUNCTION__.
TEST (LoggerFunctionNameMacroTest, FollowsTheCompilerAndNotTheTarget)
{
#if defined(_MSC_VER)
  char const *const expected = "__FUNCSIG__";
#elif defined(__GNUC__) || defined(__clang__)
  char const *const expected = "__PRETTY_FUNCTION__";
#else
  char const *const expected = "__func__";
#endif

  EXPECT_STREQ (expected, LUMEX_STRINGIZE (LOGGER_FUNCTION_NAME));
}

// --- Disabled-state default getters (also exercises the new item-2 methods) -

// CoT: if some unexpected environment already has an `enable_logs` file next
// to this test binary, the singleton would come up "enabled" and these
// particular assertions would no longer hold; skip gracefully rather than
// producing a false failure unrelated to the code under test.
#define LUMEX_LOGGER_SKIP_IF_UNEXPECTEDLY_ENABLED(logger)                     \
  if ((logger).is_logging_enabled ())                                         \
    {                                                                         \
      GTEST_SKIP () << "LumexLogger singleton came up enabled (an "           \
                       "enable_logs trigger file exists next to the test "    \
                       "binary) - this test only covers the deterministic "   \
                       "disabled-by-default state.";                          \
    }                                                                         \
  (void)0

TEST (LumexLoggerDefaultsTest, DefaultTriggerFileNameIsEnableLogs)
{
  LumexLogger &logger = LumexLogger::get_instance ();
  EXPECT_EQ (logger.get_trigger_file_name (), "enable_logs");
}

TEST (LumexLoggerDefaultsTest,
      DisabledLoggerHasNoLogFilePathAndNoLevelsEnabled)
{
  LumexLogger &logger = LumexLogger::get_instance ();
  LUMEX_LOGGER_SKIP_IF_UNEXPECTEDLY_ENABLED (logger);

  EXPECT_EQ (logger.get_log_file_path (), "");
  EXPECT_FALSE (logger.is_level_enabled (LogLevel::LEVEL_TRACE));
  EXPECT_FALSE (logger.is_level_enabled (LogLevel::LEVEL_FATAL));
}

TEST (LumexLoggerDefaultsTest, DescribeFrameIsDisabledByDefault)
{
  // is_describe_frame_enabled() is new (item 2 of the migration) - verify it
  // exists, links, and defaults to false when no DESCRIBE_FRAME= line was ever
  // read from a config file.
  LumexLogger &logger = LumexLogger::get_instance ();
  LUMEX_LOGGER_SKIP_IF_UNEXPECTEDLY_ENABLED (logger);

  EXPECT_FALSE (logger.is_describe_frame_enabled ());
}

TEST (LumexLoggerDefaultsTest, AppliedConfigViewReflectsConstructionDefaults)
{
  // get_applied_config_view() and the BUFFERING_TRIGGER-related fields on it
  // are new (item 2 of the migration). None of these five fields have a public
  // setter, so - regardless of test execution order - they always reflect
  // whatever the constructor read (or, when disabled, the constructor's
  // member-init-list defaults).
  LumexLogger &logger = LumexLogger::get_instance ();
  lumex::applied::logger::logger::logger_applied_config_view_t const configView
      = logger.get_applied_config_view ();

  EXPECT_EQ (configView.stack_trace_max_frames, 16);
  EXPECT_EQ (configView.log_buffer_flush_trigger_level,
             LogLevel::LEVEL_WARNING);

  LUMEX_LOGGER_SKIP_IF_UNEXPECTEDLY_ENABLED (logger);
  EXPECT_FALSE (configView.use_timestamped_logs);
  EXPECT_FALSE (configView.show_stack_trace_in_messages);
  EXPECT_FALSE (configView.log_buffer_flush_level_configured);
}

#undef LUMEX_LOGGER_SKIP_IF_UNEXPECTEDLY_ENABLED

// --- Programmatic API round-trips (order-independent: always saved/restored)
// -

TEST (LumexLoggerProgrammaticApiTest, SetAndGetLogLevelRoundTrip)
{
  LumexLogger &logger = LumexLogger::get_instance ();
  LogLevel const original = logger.get_log_level ();

  logger.set_log_level (LogLevel::LEVEL_WARNING);
  EXPECT_EQ (logger.get_log_level (), LogLevel::LEVEL_WARNING);

  logger.set_log_level (LogLevel::LEVEL_FATAL);
  EXPECT_EQ (logger.get_log_level (), LogLevel::LEVEL_FATAL);

  logger.set_log_level (original); // restore, so other tests are unaffected
                                   // regardless of run order
}

TEST (LumexLoggerProgrammaticApiTest, EnableDisableBufferingRoundTrip)
{
  LumexLogger &logger = LumexLogger::get_instance ();
  bool const wasEnabled = logger.is_buffering_enabled ();
  std::size_t const originalSz = logger.get_buffer_size ();

  logger.enable_buffering (25);
  EXPECT_TRUE (logger.is_buffering_enabled ());
  EXPECT_EQ (logger.get_buffer_size (), 25U);

  logger.set_buffer_size (7);
  EXPECT_EQ (logger.get_buffer_size (), 7U);

  logger.disable_buffering ();
  EXPECT_FALSE (logger.is_buffering_enabled ());

  // restore
  if (wasEnabled)
    logger.enable_buffering (originalSz);
  else
    {
      logger.set_buffer_size (originalSz);
      logger.disable_buffering ();
    }
}

TEST (LumexLoggerProgrammaticApiTest,
      EnableBufferingWithZeroSizeFallsBackToDefault)
{
  LumexLogger &logger = LumexLogger::get_instance ();
  bool const wasEnabled = logger.is_buffering_enabled ();
  std::size_t const originalSz = logger.get_buffer_size ();

  logger.enable_buffering (0);
  EXPECT_EQ (logger.get_buffer_size (),
             lumex::applied::logger::logger::logger_config_t::kBufferSize);

  if (wasEnabled)
    logger.enable_buffering (originalSz);
  else
    {
      logger.set_buffer_size (originalSz);
      logger.disable_buffering ();
    }
}

TEST (LumexLoggerProgrammaticApiTest, PresetEnableSetDisableRoundTrip)
{
  LumexLogger &logger = LumexLogger::get_instance ();
  bool const wasEnabled = logger.is_preset_enabled ();
  auto const originalPreset = logger.get_preset_components ();

  std::unordered_set<std::string> const components{ "NetworkManager",
                                                    "DatabaseConnection" };
  logger.set_preset_components (components);
  EXPECT_TRUE (logger.is_preset_enabled ());
  auto readBack = logger.get_preset_components ();
  EXPECT_EQ (readBack.size (), 2U);
  EXPECT_TRUE (readBack.count ("NetworkManager") > 0);
  EXPECT_TRUE (readBack.count ("DatabaseConnection") > 0);

  logger.disable_preset ();
  EXPECT_FALSE (logger.is_preset_enabled ());
  EXPECT_TRUE (logger.get_preset_components ().empty ());

  logger.enable_preset ({ "OnlyOne" });
  EXPECT_TRUE (logger.is_preset_enabled ());
  EXPECT_EQ (logger.get_preset_components ().size (), 1U);

  // restore
  if (wasEnabled)
    logger.set_preset_components (originalPreset);
  else
    logger.disable_preset ();
}

TEST (LumexLoggerProgrammaticApiTest, TriggerFileNameRoundTrip)
{
  LumexLogger &logger = LumexLogger::get_instance ();
  std::string const originalName = logger.get_trigger_file_name ();

  logger.set_trigger_file_name ("custom_trigger_for_test");
  EXPECT_EQ (logger.get_trigger_file_name (), "custom_trigger_for_test");

  // Setting an empty name must be a no-op (documented behavior: only non-empty
  // names are applied).
  logger.set_trigger_file_name ("");
  EXPECT_EQ (logger.get_trigger_file_name (), "custom_trigger_for_test");

  logger.set_trigger_file_name (originalName); // restore
  EXPECT_EQ (logger.get_trigger_file_name (), originalName);
}

// --- flush()/print_logger_hint() must never throw, enabled or not
// ------------

TEST (LumexLoggerProgrammaticApiTest,
      FlushAndPrintHintDoNotThrowRegardlessOfState)
{
  LumexLogger &logger = LumexLogger::get_instance ();
  EXPECT_NO_THROW (logger.flush ());
  EXPECT_NO_THROW (logger.print_logger_hint ());
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
