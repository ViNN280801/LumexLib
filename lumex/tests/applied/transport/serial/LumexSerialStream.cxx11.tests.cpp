// The serial stream (lumex/applied/transport/serial): the open of a port that
// does not exist, the path of a COM name, the settings a platform cannot
// apply, the path resolver that finds the port at each open on the open's
// worker thread, and, with a real port named in LUMEX_TEST_SERIAL_PORT
// (skipped
// without it), the regression of a blocked synchronous read that close () from
// another thread must end within 200 ms (a close that hangs).

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexSyncWait.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/serial/LumexSerialStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
namespace tr = lumex::applied::transport;
namespace sp = lumex::applied::serial::probe;
using tr::deadline_t;
using tr::error::transport_errc;
using tr::serial::serial_stream_options_t;

std::string
missing_port ()
{
#if defined(_WIN32)
  return "COM253";
#else
  return "/dev/ttyLUMEX_NO_SUCH_PORT";
#endif
}

std::string
test_port ()
{
  char const *const name = std::getenv ("LUMEX_TEST_SERIAL_PORT");
  return name != nullptr ? std::string (name) : std::string ();
}

// LUMEX_TEST_SERIAL_PAIR="COM8,COM9": two ends of a null-modem pair (com0com,
// socat, a cable).
bool
test_pair (std::string &first, std::string &second)
{
  char const *const text = std::getenv ("LUMEX_TEST_SERIAL_PAIR");
  if (text == nullptr)
    return false;
  std::string const pair (text);
  std::string::size_type const comma = pair.find (',');
  if (comma == std::string::npos)
    return false;
  first = pair.substr (0, comma);
  second = pair.substr (comma + 1);
  return !first.empty () && !second.empty ();
}
} // namespace

TEST (LumexSerialStreamTest,
      GivenAPortThatDoesNotExist_WhenOpened_ThenASystemErrorAndNotATimeout)
{
  tr::stream::io_runner runner;
  serial_stream_options_t options;
  options.path = missing_port ();
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  tr::status_result_t const result = stream.open (deadline_t (5000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_FALSE (result.error () == transport_errc::timeout);
  EXPECT_TRUE (result.error ().category () == std::system_category ());
  EXPECT_FALSE (stream.is_open ());
}

// The error the open of `stream` ends with (empty on success).
std::error_code
open_error (tr::stream::byte_stream &stream)
{
  std::future<void> opened = stream.async_open (asio::use_future);
  try
    {
      opened.get ();
      return std::error_code ();
    }
  catch (std::system_error const &failure)
    {
      return failure.code ();
    }
}

// REVIEW13 point 5: the resolver runs at each open, on the open's worker
// thread (not where the stream was made, not on its executor).
TEST (
    LumexSerialStreamTest,
    GivenAPathResolver_WhenOpened_ThenItRunsOnTheWorkerAndItsResultDecidesThePort)
{
  tr::stream::io_runner runner;
  tr::executor_type const executor = runner.get_executor ();
  std::shared_ptr<std::mutex> const mutex = std::make_shared<std::mutex> ();
  std::shared_ptr<std::vector<std::thread::id>> const threads
      = std::make_shared<std::vector<std::thread::id>> ();
  std::shared_ptr<std::vector<bool>> const on_executor
      = std::make_shared<std::vector<bool>> ();
  std::shared_ptr<std::string> const answer = std::make_shared<std::string> ();
  serial_stream_options_t options;
  options.path = "USB VID 16E0";
  options.resolve_path = [mutex, threads, on_executor, answer, executor] ()
    {
      std::lock_guard<std::mutex> const lock (*mutex);
      threads->push_back (std::this_thread::get_id ());
      on_executor->push_back (tr::detail::runs_on (executor));
      return *answer;
    };
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  EXPECT_EQ (stream.name (), "USB VID 16E0");
  // No port found: invalid_endpoint, nothing opened.
  EXPECT_TRUE (open_error (stream) == transport_errc::invalid_endpoint);
  EXPECT_FALSE (stream.is_open ());
  // A port that is not there: the open tries it.
  {
    std::lock_guard<std::mutex> const lock (*mutex);
    *answer = missing_port ();
  }
  std::error_code const refused = open_error (stream);
  EXPECT_TRUE (refused.category () == std::system_category ())
      << refused.message ();
  std::lock_guard<std::mutex> const lock (*mutex);
  ASSERT_EQ (threads->size (), 2u);
  for (std::size_t i = 0; i < threads->size (); ++i)
    {
      EXPECT_NE ((*threads)[i], std::this_thread::get_id ());
      EXPECT_FALSE ((*on_executor)[i]) << "the resolver ran on the executor";
    }
}

TEST (LumexSerialStreamTest,
      GivenAPathResolverThatThrows_WhenOpened_ThenTheOpenFailsWithItsCode)
{
  tr::stream::io_runner runner;
  serial_stream_options_t options;
  options.path = "USB VID 16E0";
  options.resolve_path = [] () -> std::string
    {
      throw std::system_error (
          std::make_error_code (std::errc::no_such_device), "gone");
    };
  tr::stream::byte_stream with_code
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  EXPECT_TRUE (open_error (with_code) == std::errc::no_such_device);
  options.resolve_path = [] () -> std::string
    { throw std::runtime_error ("enumeration failed"); };
  tr::stream::byte_stream without_code
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  EXPECT_TRUE (open_error (without_code) == transport_errc::invalid_endpoint);
}

TEST (LumexSerialStreamTest,
      GivenAComName_WhenTheOpenPathIsMade_ThenWindowsGetsThePrefix)
{
#if defined(_WIN32)
  EXPECT_EQ (tr::serial::serial_open_path ("COM12"), "\\\\.\\COM12");
  EXPECT_EQ (tr::serial::serial_open_path ("com3"), "\\\\.\\com3");
  EXPECT_EQ (tr::serial::serial_open_path ("\\\\.\\COM4"), "\\\\.\\COM4");
#else
  EXPECT_EQ (tr::serial::serial_open_path ("/dev/ttyUSB0"), "/dev/ttyUSB0");
#endif
}

TEST (LumexSerialStreamTest,
      GivenSettingsThePlatformCannotApply_WhenValidated_ThenNotSupported)
{
  sp::serial_port_settings_t settings;
  EXPECT_FALSE (tr::serial::validate_serial_settings (settings));
  settings.parity = sp::serial_parity::mark;
  EXPECT_TRUE (tr::serial::validate_serial_settings (settings)
               == std::errc::not_supported);
  settings.parity = sp::serial_parity::even;
  settings.stop_bits = sp::serial_stop_bits::two;
  EXPECT_FALSE (tr::serial::validate_serial_settings (settings));
  settings.data_bits = 9;
  EXPECT_TRUE (tr::serial::validate_serial_settings (settings)
               == std::errc::not_supported);
  settings.data_bits = 7;
  settings.stop_bits = sp::serial_stop_bits::one_point_five;
#if defined(_WIN32)
  EXPECT_FALSE (tr::serial::validate_serial_settings (settings));
#else
  EXPECT_TRUE (tr::serial::validate_serial_settings (settings)
               == std::errc::not_supported);
#endif
}

TEST (
    LumexSerialStreamTest,
    GivenUnsupportedSettings_WhenOpened_ThenNotSupportedWithoutTouchingThePort)
{
  tr::stream::io_runner runner;
  serial_stream_options_t options;
  options.path = missing_port ();
  options.settings.parity = sp::serial_parity::space;
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  tr::status_result_t const result = stream.open (deadline_t (2000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == std::errc::not_supported);
}

TEST (
    LumexSerialStreamTest,
    GivenARealPortAndABlockedRead_WhenAnotherThreadCloses_ThenTheReadEndsWithin200Ms)
{
  std::string const port = test_port ();
  if (port.empty ())
    GTEST_SKIP () << "set LUMEX_TEST_SERIAL_PORT to a port nobody answers on";
  tr::stream::io_runner runner;
  serial_stream_options_t options;
  options.path = port;
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  ASSERT_TRUE (stream.open (deadline_t (5000)).has_value ());
  std::chrono::steady_clock::time_point closed_at;
  std::thread closer (
      [&stream, &closed_at]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (100));
          closed_at = std::chrono::steady_clock::now ();
          stream.close ();
        });
  std::vector<std::uint8_t> buffer (16);
  tr::io_result_t const result
      = stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (10000));
  std::chrono::steady_clock::time_point const returned_at
      = std::chrono::steady_clock::now ();
  closer.join ();
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::cancelled);
  EXPECT_LT (std::chrono::duration_cast<std::chrono::milliseconds> (
                 returned_at - closed_at)
                 .count (),
             200);
}

TEST (
    LumexSerialStreamTest,
    GivenAPairWithEvenParityAndTwoStopBits_WhenOneEndWrites_ThenTheOtherReadsEveryByte)
{
  std::string first;
  std::string second;
  if (!test_pair (first, second))
    GTEST_SKIP () << "set LUMEX_TEST_SERIAL_PAIR to two ends of a null modem";
  tr::stream::io_runner runner (2);
  serial_stream_options_t options;
  options.settings.baud_rate = 9600;
  options.settings.parity = sp::serial_parity::even;
  options.settings.stop_bits = sp::serial_stop_bits::two;
  options.path = first;
  tr::stream::byte_stream writer
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  options.path = second;
  tr::stream::byte_stream reader
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  ASSERT_TRUE (writer.open (deadline_t (5000)).has_value ());
  ASSERT_TRUE (reader.open (deadline_t (5000)).has_value ());

  std::vector<std::uint8_t> sent (300);
  for (std::size_t i = 0; i < sent.size (); ++i)
    sent[i] = static_cast<std::uint8_t> (i * 7);
  ASSERT_TRUE (writer.write_all (tr::const_bytes_t (sent), deadline_t (5000))
                   .has_value ());

  std::vector<std::uint8_t> received;
  while (received.size () < sent.size ())
    {
      std::vector<std::uint8_t> chunk (64);
      tr::io_result_t const got
          = reader.read_some (tr::mutable_bytes_t (chunk), deadline_t (5000));
      ASSERT_TRUE (got.has_value ()) << got.error ().message ();
      received.insert (received.end (), chunk.begin (),
                       chunk.begin ()
                           + static_cast<std::ptrdiff_t> (got.value ()));
    }
  EXPECT_EQ (received, sent);
}