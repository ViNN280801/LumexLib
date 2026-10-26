// LUMEX_TRANSPORT_LOG_MIN_LEVEL removes the calls below it at compile time: in
// this translation unit the minimum is warning (3), so a trace call neither
// builds its message nor reaches the sink, even with the run-time threshold
// at trace.

#define LUMEX_TRANSPORT_LOG_MIN_LEVEL 3

#include <string>

#include <gtest/gtest.h>

#include "lumex/applied/transport/log/LumexTransportLog.hpp"

namespace
{
namespace lg = lumex::applied::transport::log;

int g_records = 0;

void
count_sink (lg::log_record_t const &)
{
  ++g_records;
}
} // namespace

TEST (
    LumexTransportLogMinLevelTest,
    GivenAMinimumOfWarning_WhenTraceAndWarningAreLogged_ThenOnlyWarningIsCompiledIn)
{
  g_records = 0;
  lg::set_log_sink (&count_sink);
  lg::set_log_level (lg::log_level::trace);
  int built = 0;
  LUMEX_TRANSPORT_LOG (trace, "c", "", (++built, std::string ("t")),
                       lg::log_bytes_t ());
  LUMEX_TRANSPORT_LOG (warning, "c", "", (++built, std::string ("w")),
                       lg::log_bytes_t ());
  lg::set_log_sink (nullptr);
  lg::set_log_level (lg::log_level::warning);
  EXPECT_EQ (built, 1);
  EXPECT_EQ (g_records, 1);
}
