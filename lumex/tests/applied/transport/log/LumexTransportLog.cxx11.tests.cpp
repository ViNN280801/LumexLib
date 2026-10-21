// The transport log (lumex/applied/transport/log): threshold, sink, record
// fields, component filter, dump format and limit, the default sink, a sink
// that throws, and concurrent use.

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/log/LumexTransportLog.hpp"

namespace
{
namespace lg = lumex::applied::transport::log;
using lg::log_bytes_t;
using lg::log_level;
using lg::log_record_t;

struct captured_t
{
  log_level level;
  std::string component;
  std::string instance;
  std::string message;
  std::vector<std::uint8_t> bytes;
};

std::vector<captured_t> &
captured ()
{
  static std::vector<captured_t> records;
  return records;
}

void
capture_sink (log_record_t const &record)
{
  captured_t entry;
  entry.level = record.level;
  entry.component = record.component;
  entry.instance = record.instance;
  entry.message = record.message;
  entry.bytes.assign (record.bytes.begin (), record.bytes.end ());
  captured ().push_back (entry);
}

void
throwing_sink (log_record_t const &)
{
  throw std::runtime_error ("sink failed");
}

std::atomic<int> g_counted (0);

void
counting_sink (log_record_t const &)
{
  g_counted.fetch_add (1);
}

class LumexTransportLogTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    captured ().clear ();
    lg::set_log_sink (&capture_sink);
    lg::set_log_level (log_level::trace);
  }

  void
  TearDown () override
  {
    lg::set_log_sink (nullptr);
    lg::set_log_level (log_level::warning);
    lg::set_component_filter (std::vector<std::string> (),
                              std::vector<std::string> ());
    lg::set_dump_limit (256);
    lg::set_dump_unlimited (false);
  }
};

std::vector<std::uint8_t>
bytes_of (std::string const &text)
{
  return std::vector<std::uint8_t> (text.begin (), text.end ());
}
} // namespace

TEST_F (LumexTransportLogTest,
        GivenTheDefaults_WhenRead_ThenWarningAndTheDefaultSink)
{
  lg::set_log_sink (nullptr);
  lg::set_log_level (log_level::warning);
  EXPECT_EQ (lg::get_log_level (), log_level::warning);
  EXPECT_EQ (lg::get_log_sink (), nullptr);
  EXPECT_EQ (lg::get_dump_limit (), 256u);
  EXPECT_FALSE (lg::is_dump_unlimited ());
  EXPECT_FALSE (lg::log_enabled (log_level::info, "transport.serial"));
  EXPECT_TRUE (lg::log_enabled (log_level::warning, "transport.serial"));
}

TEST_F (LumexTransportLogTest,
        GivenAThreshold_WhenEnabledIsAsked_ThenOnlyLevelsAtOrAboveIt)
{
  lg::set_log_level (log_level::info);
  EXPECT_FALSE (lg::log_enabled (log_level::debug, "c"));
  EXPECT_TRUE (lg::log_enabled (log_level::info, "c"));
  EXPECT_TRUE (lg::log_enabled (log_level::error, "c"));
  lg::set_log_level (log_level::off);
  EXPECT_FALSE (lg::log_enabled (log_level::error, "c"));
}

TEST_F (LumexTransportLogTest,
        GivenARecord_WhenWritten_ThenTheSinkSeesEveryField)
{
  std::vector<std::uint8_t> const data = { 0x41, 0x0D };
  LUMEX_TRANSPORT_LOG (debug, "transport.tcp", "10.0.0.1:5000",
                       std::string ("sent ") + std::to_string (2),
                       log_bytes_t (data));
  ASSERT_EQ (captured ().size (), 1u);
  EXPECT_EQ (captured ()[0].level, log_level::debug);
  EXPECT_EQ (captured ()[0].component, "transport.tcp");
  EXPECT_EQ (captured ()[0].instance, "10.0.0.1:5000");
  EXPECT_EQ (captured ()[0].message, "sent 2");
  EXPECT_EQ (captured ()[0].bytes, data);
}

TEST_F (LumexTransportLogTest,
        GivenALevelBelowTheThreshold_WhenLogged_ThenTheMessageIsNotBuilt)
{
  lg::set_log_level (log_level::error);
  int built = 0;
  LUMEX_TRANSPORT_LOG (info, "c", "", (++built, std::string ("x")),
                       log_bytes_t ());
  EXPECT_EQ (built, 0);
  EXPECT_TRUE (captured ().empty ());
}

TEST_F (LumexTransportLogTest,
        GivenAnAllowAndADenyList_WhenFiltered_ThenDenyWinsAndAllowLimits)
{
  lg::set_component_filter ({ "transport." }, { "transport.serial" });
  EXPECT_TRUE (lg::log_enabled (log_level::info, "transport.tcp"));
  EXPECT_FALSE (lg::log_enabled (log_level::info, "transport.serial"));
  EXPECT_FALSE (lg::log_enabled (log_level::info, "framing.slip"));
  lg::set_component_filter ({}, { "framing." });
  EXPECT_TRUE (lg::log_enabled (log_level::info, "transport.serial"));
  EXPECT_FALSE (lg::log_enabled (log_level::info, "framing.slip"));
  lg::set_component_filter ({}, {});
  EXPECT_TRUE (lg::log_enabled (log_level::info, "framing.slip"));
}

TEST_F (LumexTransportLogTest,
        GivenTextAndControlBytes_WhenDumped_ThenHexThenNamedText)
{
  std::vector<std::uint8_t> const data = bytes_of ("A\r\n");
  EXPECT_EQ (lg::format_dump (log_bytes_t (data)), "41 0D 0A |A<CR><LF>|");
  std::vector<std::uint8_t> const control
      = { 0x02, 0x03, 0x06, 0x15, 0x1A, 0x00, 0xFF };
  EXPECT_EQ (lg::format_dump (log_bytes_t (control)),
             "02 03 06 15 1A 00 FF |<STX><ETX><ACK><NAK><SUB>..|");
  EXPECT_EQ (lg::format_dump (log_bytes_t ()), " ||");
}

TEST_F (LumexTransportLogTest,
        GivenMoreBytesThanTheLimit_WhenDumped_ThenTheRestIsCounted)
{
  lg::set_dump_limit (2);
  std::vector<std::uint8_t> const data = bytes_of ("ABCDE");
  EXPECT_EQ (lg::format_dump (log_bytes_t (data)), "41 42 |AB| (+3 B)");
  lg::set_dump_unlimited (true);
  EXPECT_EQ (lg::format_dump (log_bytes_t (data)), "41 42 43 44 45 |ABCDE|");
}

TEST_F (
    LumexTransportLogTest,
    GivenTheDefaultSink_WhenLevelsAreWritten_ThenOnlyWarningAndAboveReachStderr)
{
  lg::set_log_sink (nullptr);
  testing::internal::CaptureStderr ();
  lg::log_write (log_level::info, "transport.serial", "COM3", "opened");
  std::vector<std::uint8_t> const data = bytes_of ("A");
  lg::log_write (log_level::warning, "transport.serial", "COM3", "timeout",
                 log_bytes_t (data));
  std::string const text = testing::internal::GetCapturedStderr ();
  EXPECT_EQ (text.find ("opened"), std::string::npos);
  EXPECT_NE (text.find ("WARNING transport.serial COM3: timeout | 41 |A|"),
             std::string::npos);
}

TEST_F (LumexTransportLogTest,
        GivenASinkThatThrows_WhenWritten_ThenTheDefaultSinkTakesOver)
{
  lg::set_log_sink (&throwing_sink);
  testing::internal::CaptureStderr ();
  EXPECT_NO_THROW (
      lg::log_write (log_level::error, "transport.tcp", "", "connect failed"));
  std::string const text = testing::internal::GetCapturedStderr ();
  EXPECT_NE (text.find ("connect failed"), std::string::npos);
}

TEST_F (LumexTransportLogTest,
        GivenThreadsThatLogAndChangeTheLevel_WhenRun_ThenNothingBreaks)
{
  g_counted.store (0);
  lg::set_log_sink (&counting_sink);
  std::atomic<bool> stop (false);
  std::thread changer (
      [&stop]
        {
          while (!stop.load ())
            {
              lg::set_log_level (log_level::trace);
              lg::set_component_filter ({ "transport." }, {});
              lg::set_log_level (log_level::error);
              lg::set_component_filter ({}, {});
            }
        });
  for (int i = 0; i < 20000; ++i)
    LUMEX_TRANSPORT_LOG (error, "transport.tcp", "", std::string ("e"),
                         log_bytes_t ());
  stop.store (true);
  changer.join ();
  EXPECT_GT (g_counted.load (), 0);
}
