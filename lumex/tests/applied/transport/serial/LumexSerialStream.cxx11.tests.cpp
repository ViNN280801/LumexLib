// The serial stream (lumex/applied/transport/serial): the open of a port that
// does not exist, the path of a COM name, the settings a platform cannot
// apply, the path resolver that finds the port at each open on the open's
// worker thread, and, with a real port named in LUMEX_TEST_SERIAL_PORT
// (skipped
// without it), the regression of a blocked synchronous read that close () from
// another thread must end within 200 ms (a close that hangs).

#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstddef>
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

#if defined(__linux__)
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#endif

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

#if defined(__linux__)
namespace
{
// A pseudoterminal: its slave is a port the open reaches without hardware.
class test_pty
{
public:
  test_pty () : master_ (::posix_openpt (O_RDWR | O_NOCTTY))
  {
    if (master_ < 0 || ::grantpt (master_) != 0 || ::unlockpt (master_) != 0)
      return;
    char name[256];
    if (::ptsname_r (master_, name, sizeof name) == 0)
      slave_ = name;
  }
  ~test_pty ()
  {
    if (master_ >= 0)
      ::close (master_);
  }

  test_pty (test_pty const &) = delete;
  test_pty &operator= (test_pty const &) = delete;

  bool
  ok () const
  {
    return !slave_.empty ();
  }
  std::string const &
  slave () const
  {
    return slave_;
  }

private:
  int master_;
  std::string slave_;
};

// The descriptors of this process that lead to `path`.
std::size_t
descriptors_to (std::string const &path)
{
  std::size_t count = 0;
  DIR *const directory = ::opendir ("/proc/self/fd");
  if (directory == nullptr)
    return 0;
  while (dirent const *const entry = ::readdir (directory))
    {
      char target[PATH_MAX] = {};
      std::string const link = std::string ("/proc/self/fd/") + entry->d_name;
      ssize_t const size
          = ::readlink (link.c_str (), target, sizeof target - 1);
      if (size > 0 && path == target)
        ++count;
    }
  ::closedir (directory);
  return count;
}

// The path resolver of the tests below: it reports that it runs, waits for
// `release` and answers with the port. The options and the open worker hold
// copies of it, so its use count tells when the worker is gone.
class blocking_resolver
{
public:
  explicit blocking_resolver (std::string const &port)
      : port_ (port), entered_ (false), entries_ (0), released_ (false)
  {
  }

  std::string
  resolve ()
  {
    std::unique_lock<std::mutex> lock (mutex_);
    entered_ = true;
    ++entries_;
    changed_.notify_all ();
    changed_.wait (lock, [this] () { return released_; });
    return port_;
  }

  std::size_t
  entries ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return entries_;
  }

  // Waits until `count` calls have entered the resolver.
  bool
  wait_entries (std::size_t count)
  {
    std::unique_lock<std::mutex> lock (mutex_);
    return changed_.wait_for (lock, std::chrono::seconds (5),
                              [this, count] () { return entries_ >= count; });
  }

  bool
  wait_entered ()
  {
    std::unique_lock<std::mutex> lock (mutex_);
    return changed_.wait_for (lock, std::chrono::seconds (5),
                              [this] () { return entered_; });
  }

  void
  release ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    released_ = true;
    changed_.notify_all ();
  }

private:
  std::string const port_;
  std::mutex mutex_;
  std::condition_variable changed_;
  bool entered_;
  std::size_t entries_;
  bool released_;
};

// Lets the resolver go when the test leaves, whatever its checks said.
struct release_on_exit
{
  blocking_resolver &resolver;
  ~release_on_exit () { resolver.release (); }
};

template <typename Predicate>
bool
eventually (Predicate predicate)
{
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + std::chrono::seconds (5);
  while (!predicate ())
    {
      if (std::chrono::steady_clock::now () > end)
        return false;
      std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }
  return true;
}

// A stream on a manually driven context (nothing runs it but `poll`), with
// the resolver it opens through.
struct open_fixture
{
  explicit open_fixture (std::string const &port)
      : resolver (std::make_shared<blocking_resolver> (port)),
        context (new asio::io_context)
  {
    serial_stream_options_t options;
    options.path = "by resolver";
    std::shared_ptr<blocking_resolver> const resolver_copy = resolver;
    options.resolve_path
        = [resolver_copy] () { return resolver_copy->resolve (); };
    stream.reset (new tr::stream::byte_stream (
        tr::serial::make_serial_stream (context->get_executor (), options)));
    impl = (*stream).impl ();
  }

  std::shared_ptr<blocking_resolver> resolver;
  std::unique_ptr<asio::io_context> context;
  std::unique_ptr<tr::stream::byte_stream> stream;
  std::weak_ptr<tr::stream::stream_impl> impl;
};
} // namespace

// The open runs on a worker thread that may outlive the context: the context
// goes while the worker still resolves the path, the stream too. The worker
// then finds no stream, posts nothing and closes the port it opened.
TEST (
    LumexSerialStreamTest,
    GivenAnOpenInProgress_WhenTheContextIsDestroyed_ThenTheLateDescriptorIsClosed)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  open_fixture fixture (pty.slave ());
  release_on_exit const leave = { *fixture.resolver };
  fixture.stream->async_open ([] (std::error_code)
                                { ADD_FAILURE () << "the open handler ran"; });
  fixture.context->poll ();
  ASSERT_TRUE (fixture.resolver->wait_entered ());

  fixture.stream.reset ();
  fixture.context.reset ();
  // The shutdown let the stream go: nothing keeps it for the worker.
  EXPECT_TRUE (fixture.impl.expired ());

  fixture.resolver->release ();
  // Only this test holds the resolver once the worker has ended.
  EXPECT_TRUE (eventually ([&fixture] ()
                             { return fixture.resolver.use_count () == 1; }));
  EXPECT_EQ (descriptors_to (pty.slave ()), before);
}

// The worker posted the step and the context never runs it again: the step
// owns the opened port and closes it when the context destroys it.
TEST (
    LumexSerialStreamTest,
    GivenAPostedOpenStepThatNeverRuns_WhenTheContextIsDestroyed_ThenNoDescriptorIsLeft)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  open_fixture fixture (pty.slave ());
  fixture.resolver->release ();
  fixture.stream->async_open ([] (std::error_code)
                                { ADD_FAILURE () << "the open handler ran"; });
  fixture.context->poll ();
  // The stream holds one reference of the resolver (its options) and the test
  // one; the worker's goes when it has posted its step.
  ASSERT_TRUE (eventually ([&fixture] ()
                             { return fixture.resolver.use_count () == 2; }));
  ASSERT_TRUE (eventually (
      [&pty, before] () { return descriptors_to (pty.slave ()) > before; }));
  EXPECT_EQ (descriptors_to (pty.slave ()), before + 1);
  // The stream, the gate and the posted step; the worker holds none.
  EXPECT_EQ (fixture.impl.use_count (), 3);

  fixture.stream.reset ();
  fixture.context.reset ();
  EXPECT_TRUE (fixture.impl.expired ());
  EXPECT_EQ (descriptors_to (pty.slave ()), before);
}

// The reference the worker used lives in the gate until the open finishes:
// finishing lets it go, so a stream that was opened and is destroyed is freed.
TEST (LumexSerialStreamTest,
      GivenAnOpenedStream_WhenItIsDestroyed_ThenTheImplementationIsFreed)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  tr::stream::io_runner runner;
  std::weak_ptr<tr::stream::stream_impl> impl;
  {
    serial_stream_options_t options;
    options.path = pty.slave ();
    tr::stream::byte_stream stream
        = tr::serial::make_serial_stream (runner.get_executor (), options);
    impl = stream.impl ();
    ASSERT_TRUE (open_error (stream) == std::error_code ());
    EXPECT_TRUE (stream.is_open ());
    EXPECT_EQ (descriptors_to (pty.slave ()), before + 1);
  }
  EXPECT_TRUE (eventually ([&impl] () { return impl.expired (); }));
  EXPECT_EQ (descriptors_to (pty.slave ()), before);
}
namespace
{
long long const KSLACK_MS = 400;
long long const KBACKSTOP_MS = 1500;

long long
elapsed_ms (std::chrono::steady_clock::time_point start)
{
  return static_cast<long long> (
      std::chrono::duration_cast<std::chrono::milliseconds> (
          std::chrono::steady_clock::now () - start)
          .count ());
}

// Lets the resolver go after `milliseconds` (or when the test ends): a
// defective open that waits for the resolver then shows as a late return and
// not as a hang.
class delayed_release
{
public:
  delayed_release (blocking_resolver &resolver, long long milliseconds)
      : resolver_ (resolver), cancelled_ (false),
        thread_ ([this, milliseconds] () { run (milliseconds); })
  {
  }

  ~delayed_release ()
  {
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      cancelled_ = true;
    }
    changed_.notify_all ();
    thread_.join ();
  }

  delayed_release (delayed_release const &) = delete;
  delayed_release &operator= (delayed_release const &) = delete;

private:
  void
  run (long long milliseconds)
  {
    std::unique_lock<std::mutex> lock (mutex_);
    if (!changed_.wait_for (lock, std::chrono::milliseconds (milliseconds),
                            [this] () { return cancelled_; }))
      resolver_.release ();
  }

  blocking_resolver &resolver_;
  std::mutex mutex_;
  std::condition_variable changed_;
  bool cancelled_;
  std::thread thread_;
};

// A stream made on the runner, opening through `resolver`.
tr::stream::byte_stream
make_resolving_stream (tr::stream::io_runner &runner,
                       std::shared_ptr<blocking_resolver> const &resolver)
{
  serial_stream_options_t options;
  options.path = "by resolver";
  options.resolve_path = [resolver] () { return resolver->resolve (); };
  return tr::serial::make_serial_stream (runner.get_executor (), options);
}

// The error an open ends with, as a future of the code (no exception travels
// between the threads: the exception_ptr of libstdc++ counts its references
// where ThreadSanitizer cannot see it).
std::future<std::error_code>
open_async (tr::stream::byte_stream &stream)
{
  std::shared_ptr<std::promise<std::error_code>> const done
      = std::make_shared<std::promise<std::error_code>> ();
  stream.async_open ([done] (std::error_code error)
                       { done->set_value (error); });
  return done->get_future ();
}

std::error_code
error_of (std::future<std::error_code> &future)
{
  return future.get ();
}

// The worker has ended: it held a copy of the resolver (and so does the
// stream's options).
bool
worker_ended (std::shared_ptr<blocking_resolver> const &resolver)
{
  return eventually ([&resolver] () { return resolver.use_count () == 2; });
}
} // namespace

// A synchronous open returns by its deadline also when the worker of the
// open is held up (here by the finder of the port, which the open's own
// deadline does not bound): the late port is closed and nothing else
// happens.
TEST (
    LumexSerialStreamTest,
    GivenAnOpenWaitingForTheResolver_WhenTheDeadlinePasses_ThenTimeoutAndTheLatePortIsClosed)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  std::shared_ptr<blocking_resolver> const resolver
      = std::make_shared<blocking_resolver> (pty.slave ());
  tr::stream::io_runner runner;
  std::weak_ptr<tr::stream::stream_impl> impl;
  {
    tr::stream::byte_stream stream = make_resolving_stream (runner, resolver);
    impl = stream.impl ();
    delayed_release const guard (*resolver, KBACKSTOP_MS);

    std::chrono::steady_clock::time_point const start
        = std::chrono::steady_clock::now ();
    tr::status_result_t const result = stream.open (deadline_t (100));
    long long const took = elapsed_ms (start);

    ASSERT_FALSE (result.has_value ());
    EXPECT_TRUE (result.error () == transport_errc::timeout)
        << result.error ();
    EXPECT_LT (took, 100 + KSLACK_MS);
    EXPECT_EQ (stream.statistics ().timeouts, 1u);
    EXPECT_FALSE (stream.is_open ());

    resolver->release ();
    ASSERT_TRUE (worker_ended (resolver));
    EXPECT_EQ (descriptors_to (pty.slave ()), before);
    EXPECT_FALSE (stream.is_open ());
    // The stream opens normally afterwards.
    EXPECT_TRUE (open_error (stream) == std::error_code ());
    EXPECT_TRUE (stream.is_open ());
    EXPECT_EQ (descriptors_to (pty.slave ()), before + 1);
  }
  EXPECT_TRUE (eventually ([&impl] () { return impl.expired (); }));
  EXPECT_EQ (descriptors_to (pty.slave ()), before);
}

TEST (
    LumexSerialStreamTest,
    GivenAnOpenWaitingForTheResolver_WhenCancelled_ThenItEndsCancelledAtOnceAndTheDeviceIsFreed)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  std::shared_ptr<blocking_resolver> const resolver
      = std::make_shared<blocking_resolver> (pty.slave ());
  tr::stream::io_runner runner;
  std::weak_ptr<tr::stream::stream_impl> impl;
  {
    tr::stream::byte_stream stream = make_resolving_stream (runner, resolver);
    impl = stream.impl ();
    delayed_release const guard (*resolver, KBACKSTOP_MS);
    std::future<std::error_code> opened = open_async (stream);
    ASSERT_TRUE (resolver->wait_entered ());
    stream.cancel ();
    ASSERT_EQ (opened.wait_for (std::chrono::milliseconds (1000)),
               std::future_status::ready);
    EXPECT_TRUE (error_of (opened) == transport_errc::cancelled);
    EXPECT_FALSE (stream.is_open ());
    resolver->release ();
    ASSERT_TRUE (worker_ended (resolver));
    EXPECT_EQ (descriptors_to (pty.slave ()), before);
    EXPECT_FALSE (stream.is_open ());
  }
  EXPECT_TRUE (eventually ([&impl] () { return impl.expired (); }));
}

TEST (
    LumexSerialStreamTest,
    GivenAnOpenWaitingForTheResolver_WhenClosed_ThenItEndsCancelledAtOnceAndTheLatePortIsClosed)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  std::shared_ptr<blocking_resolver> const resolver
      = std::make_shared<blocking_resolver> (pty.slave ());
  tr::stream::io_runner runner;
  tr::stream::byte_stream stream = make_resolving_stream (runner, resolver);
  delayed_release const guard (*resolver, KBACKSTOP_MS);
  std::future<std::error_code> opened = open_async (stream);
  ASSERT_TRUE (resolver->wait_entered ());
  stream.close ();
  ASSERT_EQ (opened.wait_for (std::chrono::milliseconds (1000)),
             std::future_status::ready);
  EXPECT_TRUE (error_of (opened) == transport_errc::cancelled);
  resolver->release ();
  ASSERT_TRUE (worker_ended (resolver));
  EXPECT_EQ (descriptors_to (pty.slave ()), before);
  EXPECT_FALSE (stream.is_open ());
}

// The decision for an open that starts while an abandoned one still runs: it
// waits (parked on the strand, no second worker thread) for the old worker to
// return and then starts.
TEST (
    LumexSerialStreamTest,
    GivenAnAbandonedOpenStillRunning_WhenTheStreamOpensAgain_ThenTheNewOpenWaitsForItAndThenRuns)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  std::shared_ptr<blocking_resolver> const resolver
      = std::make_shared<blocking_resolver> (pty.slave ());
  tr::stream::io_runner runner;
  tr::stream::byte_stream stream = make_resolving_stream (runner, resolver);
  delayed_release const guard (*resolver, KBACKSTOP_MS * 2);
  std::future<std::error_code> first = open_async (stream);
  ASSERT_TRUE (resolver->wait_entries (1));
  stream.cancel ();
  ASSERT_EQ (first.wait_for (std::chrono::milliseconds (1000)),
             std::future_status::ready);
  EXPECT_TRUE (error_of (first) == transport_errc::cancelled);

  std::future<std::error_code> second = open_async (stream);
  EXPECT_EQ (second.wait_for (std::chrono::milliseconds (150)),
             std::future_status::timeout);
  EXPECT_EQ (resolver->entries (), 1u);
  resolver->release ();
  ASSERT_EQ (second.wait_for (std::chrono::seconds (5)),
             std::future_status::ready);
  EXPECT_FALSE (error_of (second));
  EXPECT_TRUE (stream.is_open ());
  EXPECT_EQ (resolver->entries (), 2u);
  // Both workers ended: the old one closed its port, the new one's is open.
  ASSERT_TRUE (
      eventually ([&resolver] () { return resolver.use_count () == 2; }));
  EXPECT_EQ (descriptors_to (pty.slave ()), before + 1);
  EXPECT_TRUE (stream.is_open ());
}

TEST (
    LumexSerialStreamTest,
    GivenAnOpenWaitingForAnAbandonedOne_WhenItsDeadlinePassesOrItIsCancelled_ThenItEndsAtOnceAndNeverRuns)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  std::shared_ptr<blocking_resolver> const resolver
      = std::make_shared<blocking_resolver> (pty.slave ());
  tr::stream::io_runner runner;
  tr::stream::byte_stream stream = make_resolving_stream (runner, resolver);
  delayed_release const guard (*resolver, KBACKSTOP_MS * 2);
  std::future<std::error_code> first = open_async (stream);
  ASSERT_TRUE (resolver->wait_entries (1));
  stream.cancel ();
  ASSERT_EQ (first.wait_for (std::chrono::milliseconds (1000)),
             std::future_status::ready);

  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  tr::status_result_t const result = stream.open (deadline_t (100));
  long long const took = elapsed_ms (start);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::timeout) << result.error ();
  EXPECT_LT (took, 100 + KSLACK_MS);

  std::future<std::error_code> cancelled = open_async (stream);
  EXPECT_EQ (cancelled.wait_for (std::chrono::milliseconds (50)),
             std::future_status::timeout);
  stream.cancel ();
  ASSERT_EQ (cancelled.wait_for (std::chrono::milliseconds (1000)),
             std::future_status::ready);
  EXPECT_TRUE (error_of (cancelled) == transport_errc::cancelled);

  std::future<std::error_code> closed = open_async (stream);
  EXPECT_EQ (closed.wait_for (std::chrono::milliseconds (50)),
             std::future_status::timeout);
  stream.close ();
  ASSERT_EQ (closed.wait_for (std::chrono::milliseconds (1000)),
             std::future_status::ready);
  EXPECT_TRUE (error_of (closed) == transport_errc::cancelled);

  resolver->release ();
  ASSERT_TRUE (
      eventually ([&resolver] () { return resolver.use_count () == 2; }));
  // Only the abandoned worker ever ran; the parked opens never started.
  EXPECT_EQ (resolver->entries (), 1u);
  EXPECT_EQ (descriptors_to (pty.slave ()), before);
  EXPECT_FALSE (stream.is_open ());
}

// The worker of an abandoned open owns nothing of the stream: the stream is
// freed while the worker still blocks (the worker's reference was let go).
TEST (
    LumexSerialStreamTest,
    GivenAnAbandonedOpen_WhenTheStreamIsDestroyedWhileTheWorkerBlocks_ThenTheStreamIsFreed)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  open_fixture fixture (pty.slave ());
  release_on_exit const leave = { *fixture.resolver };
  fixture.stream->async_open ([] (std::error_code) {});
  fixture.context->poll ();
  ASSERT_TRUE (fixture.resolver->wait_entered ());
  fixture.stream->cancel ();
  fixture.context->restart ();
  fixture.context->poll ();
  fixture.stream.reset ();
  fixture.context->restart ();
  fixture.context->poll ();
  EXPECT_TRUE (fixture.impl.expired ())
      << "the abandoned worker still owns the stream";
  fixture.resolver->release ();
  ASSERT_TRUE (fixture.resolver->wait_entered ());
  EXPECT_TRUE (eventually ([&fixture] ()
                             { return fixture.resolver.use_count () == 1; }));
  EXPECT_EQ (descriptors_to (pty.slave ()), before);
}

TEST (
    LumexSerialStreamTest,
    GivenAnAbandonedOpen_WhenTheContextIsDestroyedWhileTheWorkerBlocks_ThenTheStreamIsFreedAndNoPortIsLeft)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  open_fixture fixture (pty.slave ());
  release_on_exit const leave = { *fixture.resolver };
  fixture.stream->async_open ([] (std::error_code) {});
  fixture.context->poll ();
  ASSERT_TRUE (fixture.resolver->wait_entered ());
  fixture.stream->cancel ();
  fixture.context->restart ();
  fixture.context->poll ();
  fixture.stream.reset ();
  fixture.context.reset ();
  EXPECT_TRUE (fixture.impl.expired ());
  fixture.resolver->release ();
  EXPECT_TRUE (eventually ([&fixture] ()
                             { return fixture.resolver.use_count () == 1; }));
  EXPECT_EQ (descriptors_to (pty.slave ()), before);
}

// An open parked behind an abandoned one dies with the context.
TEST (
    LumexSerialStreamTest,
    GivenAnOpenParkedBehindAnAbandonedOne_WhenTheContextIsDestroyed_ThenItNeverRunsAndNothingLeaks)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  open_fixture fixture (pty.slave ());
  release_on_exit const leave = { *fixture.resolver };
  int calls = 0;
  fixture.stream->async_open ([&calls] (std::error_code) { ++calls; });
  fixture.context->poll ();
  ASSERT_TRUE (fixture.resolver->wait_entered ());
  fixture.stream->cancel ();
  fixture.context->restart ();
  fixture.context->poll ();
  EXPECT_EQ (calls, 1);
  fixture.stream->async_open ([&calls] (std::error_code) { ++calls; });
  fixture.context->restart ();
  fixture.context->poll ();
  fixture.stream.reset ();
  fixture.context.reset ();
  EXPECT_TRUE (fixture.impl.expired ());
  fixture.resolver->release ();
  EXPECT_TRUE (eventually ([&fixture] ()
                             { return fixture.resolver.use_count () == 1; }));
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (fixture.resolver->entries (), 1u);
  EXPECT_EQ (descriptors_to (pty.slave ()), before);
}

// The cancel is on the strand before the step that carries the result of the
// open (the worker posts it while the cancel waits): the step finds the open
// ended and closes the port.
TEST (
    LumexSerialStreamTest,
    GivenAFinishedOpenWhoseStepFollowsACancel_WhenTheStrandRuns_ThenTheStepIsDroppedAndThePortClosed)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  std::size_t const before = descriptors_to (pty.slave ());
  open_fixture fixture (pty.slave ());
  release_on_exit const leave = { *fixture.resolver };
  std::error_code seen;
  int calls = 0;
  fixture.stream->async_open (
      [&seen, &calls] (std::error_code error)
        {
          seen = error;
          ++calls;
        });
  fixture.context->poll ();
  ASSERT_TRUE (fixture.resolver->wait_entered ());
  // Queued on the strand, not run: the context is driven by the test.
  fixture.stream->cancel ();
  fixture.resolver->release ();
  ASSERT_TRUE (eventually (
      [&pty, before] () { return descriptors_to (pty.slave ()) > before; }));
  ASSERT_TRUE (eventually ([&fixture] ()
                             { return fixture.resolver.use_count () == 2; }));
  EXPECT_EQ (calls, 0);

  fixture.context->restart ();
  fixture.context->poll ();
  EXPECT_EQ (calls, 1);
  EXPECT_TRUE (seen == transport_errc::cancelled) << seen;
  EXPECT_FALSE (fixture.stream->is_open ());
  EXPECT_EQ (descriptors_to (pty.slave ()), before);
}
// The audit of the synchronous calls: a read and a write on an open port end
// by their deadline, a read ends when another thread closes the stream.
TEST (
    LumexSerialStreamTest,
    GivenAnOpenPortWithNoBytes_WhenTheReadDeadlinePasses_ThenTimeoutWithinTheSlack)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  tr::stream::io_runner runner;
  serial_stream_options_t options;
  options.path = pty.slave ();
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  ASSERT_TRUE (stream.open (deadline_t (2000)).has_value ());
  std::vector<std::uint8_t> buffer (16);
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  tr::io_result_t const got
      = stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (100));
  long long const took = elapsed_ms (start);
  ASSERT_FALSE (got.has_value ());
  EXPECT_TRUE (got.error () == transport_errc::timeout) << got.error ();
  EXPECT_LT (took, 100 + KSLACK_MS);
  EXPECT_TRUE (stream.is_open ());
}

TEST (
    LumexSerialStreamTest,
    GivenABlockedRead_WhenAnotherThreadCloses_ThenItEndsCancelledWithinTheSlack)
{
  test_pty pty;
  if (!pty.ok ())
    GTEST_SKIP () << "no pseudoterminal";
  tr::stream::io_runner runner;
  serial_stream_options_t options;
  options.path = pty.slave ();
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  ASSERT_TRUE (stream.open (deadline_t (2000)).has_value ());
  std::vector<std::uint8_t> buffer (16);
  std::thread closer (
      [&stream] ()
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (50));
          stream.close ();
        });
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  tr::io_result_t const got
      = stream.read_some (tr::mutable_bytes_t (buffer), deadline_t (5000));
  long long const took = elapsed_ms (start);
  closer.join ();
  ASSERT_FALSE (got.has_value ());
  EXPECT_TRUE (got.error () == transport_errc::cancelled) << got.error ();
  EXPECT_LT (took, 50 + KSLACK_MS);
  EXPECT_FALSE (stream.is_open ());
}
#endif
