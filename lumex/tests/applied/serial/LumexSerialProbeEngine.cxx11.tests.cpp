#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/serial/probe/detail/LumexSerialProbeEngine.hpp"
#include "lumex/applied/serial/probe/detail/LumexSerialTransport.hpp"

using namespace lumex::applied::serial::probe;
using namespace lumex::applied::serial::probe::detail;

namespace
{
using ms = std::chrono::milliseconds;

// Scripted transport double: every operation replays one entry of the
// script and records the calls it received.
struct fake_transport : serial_transport_t
{
  transport_result_t open_result;        //!< Returned by open ().
  transport_result_t write_result;       //!< Returned by write ().
  std::vector<transport_result_t> reads; //!< Returned by read (), in order.
  std::vector<std::vector<std::uint8_t>> payloads; //!< Bytes per read.

  bool open_flag = false;
  std::size_t open_calls = 0U;
  std::size_t close_calls = 0U;
  std::size_t write_calls = 0U;
  std::size_t read_calls = 0U;
  std::size_t purge_calls = 0U;
  std::vector<std::size_t> read_capacities;

  bool
  is_open () const override
  {
    return open_flag;
  }

  transport_result_t
  open (ms) override
  {
    ++open_calls;
    if (open_result.status == transport_status::ok)
      open_flag = true;
    return open_result;
  }

  void
  close () override
  {
    ++close_calls;
    open_flag = false;
  }

  transport_result_t
  write (std::uint8_t const *data, std::size_t size, ms) override
  {
    ++write_calls;
    transport_result_t out = write_result;
    if (out.status == transport_status::ok && out.bytes == 0U)
      out.bytes = data == nullptr ? 0U : size;
    return out;
  }

  transport_result_t
  read (std::uint8_t *buffer, std::size_t capacity, ms) override
  {
    read_capacities.push_back (capacity);
    transport_result_t out;
    std::vector<std::uint8_t> payload;
    if (read_calls < reads.size ())
      {
        out = reads[read_calls];
        if (read_calls < payloads.size ())
          payload = payloads[read_calls];
      }
    else
      {
        out.status = transport_status::deadline_exceeded;
      }
    ++read_calls;

    std::size_t count = 0U;
    if (out.status == transport_status::ok)
      {
        count = payload.size () < capacity ? payload.size () : capacity;
        for (std::size_t i = 0U; i < count; ++i)
          buffer[i] = payload[i];
      }
    out.bytes = count;
    return out;
  }

  void
  purge () override
  {
    ++purge_calls;
  }
};
transport_result_t
transport_ok (std::size_t bytes = 0U)
{
  transport_result_t result;
  result.status = transport_status::ok;
  result.bytes = bytes;
  return result;
}

transport_result_t
transport_deadline (std::string text = std::string ())
{
  transport_result_t result;
  result.status = transport_status::deadline_exceeded;
  result.system_error = std::move (text);
  return result;
}

transport_result_t
transport_failed (std::string text)
{
  transport_result_t result;
  result.status = transport_status::failed;
  result.system_error = std::move (text);
  return result;
}

std::function<bool (std::vector<std::uint8_t> const &)>
never_complete ()
{
  return [] (std::vector<std::uint8_t> const &) { return false; };
}

std::vector<std::uint8_t>
bytes (std::initializer_list<std::uint8_t> init)
{
  return std::vector<std::uint8_t> (init);
}
} // namespace

TEST (LumexSerialProbeEngine, OpenTimeoutMapsToOpenTimeout)
{
  fake_transport transport;
  transport.open_result = transport_deadline ("open timed out after 5 ms");

  serial_probe_result_t const result = run_probe (
      transport, std::vector<std::uint8_t> (), never_complete (), ms (5), 64U);

  EXPECT_EQ (result.status, serial_probe_status::open_timeout);
  EXPECT_EQ (result.system_error, "open timed out after 5 ms");
  EXPECT_EQ (transport.open_calls, 1U);
  EXPECT_EQ (transport.purge_calls, 0U);
  EXPECT_EQ (transport.read_calls, 0U);
}

TEST (LumexSerialProbeEngine, OpenFailureMapsToOpenFailed)
{
  fake_transport transport;
  transport.open_result = transport_failed ("access denied");

  serial_probe_result_t const result
      = run_probe (transport, std::vector<std::uint8_t> (), never_complete (),
                   ms (10), 64U);

  EXPECT_EQ (result.status, serial_probe_status::open_failed);
  EXPECT_EQ (result.system_error, "access denied");
  EXPECT_EQ (transport.open_calls, 1U);
  EXPECT_EQ (transport.read_calls, 0U);
}

TEST (LumexSerialProbeEngine, WriteFailureMapsToIoError)
{
  fake_transport transport;
  transport.open_result = transport_ok ();
  transport.write_result = transport_failed ("write error");

  serial_probe_result_t const result = run_probe (
      transport, bytes ({ 0x26U }), never_complete (), ms (50), 64U);

  EXPECT_EQ (result.status, serial_probe_status::io_error);
  EXPECT_EQ (result.system_error, "write error");
  EXPECT_EQ (transport.purge_calls, 1U);
  EXPECT_EQ (transport.write_calls, 1U);
  EXPECT_EQ (transport.read_calls, 0U);
}

TEST (LumexSerialProbeEngine, WriteDeadlineMapsToIoError)
{
  fake_transport transport;
  transport.open_result = transport_ok ();
  transport.write_result = transport_deadline ("write timed out");

  serial_probe_result_t const result = run_probe (
      transport, bytes ({ 0x01U, 0x02U }), never_complete (), ms (50), 64U);

  EXPECT_EQ (result.status, serial_probe_status::io_error);
  EXPECT_EQ (result.system_error, "write timed out");
}

TEST (LumexSerialProbeEngine, ReadTimeoutWithoutBytesMapsToNoResponse)
{
  fake_transport transport;
  transport.open_result = transport_ok ();
  transport.reads.push_back (transport_deadline ());

  serial_probe_result_t const result
      = run_probe (transport, std::vector<std::uint8_t> (), never_complete (),
                   ms (50), 64U);

  EXPECT_EQ (result.status, serial_probe_status::no_response);
  EXPECT_TRUE (result.response.empty ());
  EXPECT_EQ (transport.read_calls, 1U);
  EXPECT_EQ (transport.purge_calls, 1U);
}

TEST (LumexSerialProbeEngine, ReadTimeoutAfterBytesMapsToIncomplete)
{
  fake_transport transport;
  transport.open_result = transport_ok ();
  transport.reads.push_back (transport_ok ());
  transport.reads.push_back (transport_deadline ());
  transport.payloads.push_back (bytes ({ 0x01U, 0x02U }));

  serial_probe_result_t const result
      = run_probe (transport, std::vector<std::uint8_t> (), never_complete (),
                   ms (50), 64U);

  EXPECT_EQ (result.status, serial_probe_status::incomplete);
  EXPECT_EQ (result.response, bytes ({ 0x01U, 0x02U }));
  EXPECT_EQ (transport.read_calls, 2U);
}

TEST (LumexSerialProbeEngine, ResponseCapStopsTheLoopWithIncomplete)
{
  fake_transport transport;
  transport.open_result = transport_ok ();
  transport.reads.push_back (transport_ok ());
  transport.reads.push_back (transport_ok ());
  transport.payloads.push_back (bytes ({ 0x01U, 0x02U }));
  transport.payloads.push_back (bytes ({ 0x03U, 0x04U }));

  serial_probe_result_t const result = run_probe (
      transport, std::vector<std::uint8_t> (), never_complete (), ms (50), 3U);

  EXPECT_EQ (result.status, serial_probe_status::incomplete);
  EXPECT_EQ (result.response, bytes ({ 0x01U, 0x02U, 0x03U }));
  EXPECT_EQ (transport.read_calls, 2U);
  ASSERT_EQ (transport.read_capacities.size (), 2U);
  EXPECT_EQ (transport.read_capacities[0], 3U);
  EXPECT_EQ (transport.read_capacities[1], 1U);
}

TEST (LumexSerialProbeEngine, PredicateConfirmationMapsToResponded)
{
  fake_transport transport;
  transport.open_result = transport_ok ();
  transport.write_result = transport_ok ();
  transport.reads.push_back (transport_ok ());
  transport.reads.push_back (transport_ok ());
  transport.payloads.push_back (bytes ({ 0x01U, 0x02U }));
  transport.payloads.push_back (bytes ({ 0x03U }));

  auto const complete = [] (std::vector<std::uint8_t> const &data)
    { return data.size () >= 3U; };

  serial_probe_result_t const result
      = run_probe (transport, bytes ({ 0x41U }), complete, ms (50), 64U);

  EXPECT_EQ (result.status, serial_probe_status::responded);
  EXPECT_EQ (result.response, bytes ({ 0x01U, 0x02U, 0x03U }));
  EXPECT_EQ (transport.read_calls, 2U);
  EXPECT_EQ (transport.write_calls, 1U);
}

TEST (LumexSerialProbeEngine, PredicateSeesAccumulatedBytesAndNeverEmpty)
{
  fake_transport transport;
  transport.open_result = transport_ok ();
  transport.reads.push_back (transport_ok ());
  transport.reads.push_back (transport_ok ());
  transport.reads.push_back (transport_deadline ());
  transport.payloads.push_back (bytes ({ 0x01U, 0x02U }));
  transport.payloads.push_back (bytes ({ 0x03U }));

  std::vector<std::size_t> seen;
  auto const record = [&seen] (std::vector<std::uint8_t> const &data)
    {
      seen.push_back (data.size ());
      return false;
    };

  serial_probe_result_t const result = run_probe (
      transport, std::vector<std::uint8_t> (), record, ms (50), 64U);

  EXPECT_EQ (result.status, serial_probe_status::incomplete);
  EXPECT_EQ (result.response, bytes ({ 0x01U, 0x02U, 0x03U }));
  ASSERT_EQ (seen.size (), 2U);
  EXPECT_EQ (seen[0], 2U);
  EXPECT_EQ (seen[1], 3U);
}

TEST (LumexSerialProbeEngine, EmptyRequestSkipsTheWrite)
{
  fake_transport transport;
  transport.open_result = transport_ok ();
  transport.reads.push_back (transport_deadline ());

  serial_probe_result_t const result
      = run_probe (transport, std::vector<std::uint8_t> (), never_complete (),
                   ms (30), 64U);

  EXPECT_EQ (result.status, serial_probe_status::no_response);
  EXPECT_EQ (transport.write_calls, 0U);
  EXPECT_EQ (transport.purge_calls, 1U);
}

TEST (LumexSerialProbeEngine, OpenRequestIsSkippedWhenAlreadyOpen)
{
  fake_transport transport;
  transport.open_flag = true;
  transport.open_result = transport_failed ("open must not be called");
  transport.reads.push_back (transport_deadline ());

  serial_probe_result_t const result
      = run_probe (transport, std::vector<std::uint8_t> (), never_complete (),
                   ms (30), 64U);

  EXPECT_EQ (result.status, serial_probe_status::no_response);
  EXPECT_EQ (transport.open_calls, 0U);
}

TEST (LumexSerialProbeEngine, ZeroCapClampsToOne)
{
  fake_transport transport;
  transport.open_result = transport_ok ();
  transport.reads.push_back (transport_ok ());
  transport.payloads.push_back (bytes ({ 0x05U, 0x06U }));

  serial_probe_result_t const result = run_probe (
      transport, std::vector<std::uint8_t> (), never_complete (), ms (30), 0U);

  EXPECT_EQ (result.status, serial_probe_status::incomplete);
  ASSERT_EQ (result.response.size (), 1U);
  EXPECT_EQ (result.response[0], 0x05U);
  EXPECT_EQ (transport.read_calls, 1U);
  ASSERT_EQ (transport.read_capacities.size (), 1U);
  EXPECT_EQ (transport.read_capacities[0], 1U);
}

TEST (LumexSerialProbeEngine, ReadFailureMapsToIoErrorAndKeepsBytes)
{
  fake_transport transport;
  transport.open_result = transport_ok ();
  transport.reads.push_back (transport_ok ());
  transport.reads.push_back (transport_failed ("read error"));
  transport.payloads.push_back (bytes ({ 0x09U }));

  serial_probe_result_t const result
      = run_probe (transport, std::vector<std::uint8_t> (), never_complete (),
                   ms (30), 64U);

  EXPECT_EQ (result.status, serial_probe_status::io_error);
  EXPECT_EQ (result.system_error, "read error");
  EXPECT_EQ (result.response, bytes ({ 0x09U }));
}

TEST (LumexSerialProbeEngine, ElapsedIsFilledOnEveryPath)
{
  fake_transport transport;
  transport.open_result = transport_deadline ();

  serial_probe_result_t const result = run_probe (
      transport, std::vector<std::uint8_t> (), never_complete (), ms (5), 64U);

  EXPECT_EQ (result.status, serial_probe_status::open_timeout);
  EXPECT_GE (result.elapsed.count (), 0);
}
