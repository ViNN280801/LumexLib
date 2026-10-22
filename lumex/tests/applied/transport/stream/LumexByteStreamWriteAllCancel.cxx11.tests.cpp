// write_all ended by a cancel, a close or a terminal cancellation that lands
// between two of its writes: the device completed one write (its completion
// is queued on the strand) when the cancel runs, so no device write is
// pending and do_cancel ends nothing. The write_all must still end with
// `cancelled` and the bytes written so far instead of starting its next
// write. The device of the test (a stream_impl) keeps every write pending
// until the test completes it, as an operating system does. Regression of the
// review of WP12b (RV1, RV1b): the next write started and waited forever.
// Also (N1..N3 of that review): a cancel after the device wrote the last
// bytes loses to the success; a close during the last write counts the
// bytes written and one cancellation; the deadline of a synchronous
// write_all during its second write gives `timeout` and counts the bytes.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <ostream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::error::transport_errc;

struct seen
{
  int calls = 0;
  std::error_code error;
  std::size_t bytes = 0;
};

struct status_recorder
{
  seen *where;
  void
  operator() (std::error_code error)
  {
    ++where->calls;
    where->error = error;
  }
};

struct io_recorder
{
  seen *where;
  void
  operator() (std::error_code error, std::size_t bytes)
  {
    ++where->calls;
    where->error = error;
    where->bytes = bytes;
  }
};

// A device whose writes stay pending until the test completes them, or
// cancel / close ends them.
class held_impl final : public tr::stream::stream_impl
{
public:
  explicit held_impl (tr::executor_type const &executor)
      : stream_impl (executor, "held")
  {
  }

  // Read by the test thread while the strand writes it.
  std::atomic<int> write_calls{ 0 };

  // The device finished the oldest pending write with `bytes`: its
  // completion is queued on the strand, as an Asio socket's would be.
  void
  device_completes (std::size_t bytes)
  {
    std::shared_ptr<held_impl> const self
        = std::static_pointer_cast<held_impl> (shared_from_this ());
    asio::post (
        strand (),
        [self, bytes] ()
          {
            tr::io_handler_t handler = std::move (self->writes_.front ());
            self->writes_.erase (self->writes_.begin ());
            self->complete (std::move (handler), std::error_code (), bytes);
          });
  }

protected:
  void
  do_open (tr::status_handler_t handler) override
  {
    set_open (true);
    complete (std::move (handler), std::error_code ());
  }

  void
  do_read_some (tr::mutable_bytes_t, tr::io_handler_t handler) override
  {
    complete (std::move (handler), make_error_code (transport_errc::not_open),
              0);
  }

  void
  do_write_some (tr::const_bytes_t, tr::io_handler_t handler) override
  {
    ++write_calls;
    writes_.push_back (std::move (handler));
  }

  void
  do_cancel () override
  {
    std::vector<tr::io_handler_t> pending;
    pending.swap (writes_);
    for (std::size_t i = 0; i < pending.size (); ++i)
      complete (std::move (pending[i]),
                make_error_code (transport_errc::cancelled), 0);
  }

  void
  do_close () override
  {
    do_cancel ();
  }

private:
  std::vector<tr::io_handler_t> writes_;
};

enum class ending
{
  signal,
  cancel,
  close
};

class LumexByteStreamWriteAllCancelTest
    : public ::testing::TestWithParam<ending>
{
protected:
  void
  settle ()
  {
    context_.restart ();
    context_.poll ();
  }

  asio::io_context context_;
  std::shared_ptr<held_impl> impl_
      = std::make_shared<held_impl> (context_.get_executor ());
  tr::stream::byte_stream stream_{ impl_ };
};

std::string
name_of (ending value)
{
  switch (value)
    {
    case ending::signal:
      return "TerminalSignal";
    case ending::cancel:
      return "Cancel";
    case ending::close:
      return "Close";
    }
  return "Unknown";
}

std::string
ending_name (::testing::TestParamInfo<ending> const &info)
{
  return name_of (info.param);
}

// For the messages of GoogleTest.
void
PrintTo (ending value, std::ostream *out)
{
  *out << name_of (value);
}
} // namespace

TEST_P (
    LumexByteStreamWriteAllCancelTest,
    GivenAWriteAllBetweenTwoWrites_WhenItIsCancelled_ThenItEndsCancelledWithTheBytesWritten)
{
  seen open;
  stream_.async_open (status_recorder{ &open });
  settle ();
  ASSERT_EQ (open.calls, 1);

  std::vector<std::uint8_t> const data (3, std::uint8_t (7));
  asio::cancellation_signal signal;
  seen write;
  stream_.async_write_all (
      tr::const_bytes_t (data),
      asio::bind_cancellation_slot (signal.slot (), io_recorder{ &write }));
  settle ();
  ASSERT_EQ (impl_->write_calls.load (), 1);
  impl_->device_completes (1);
  switch (GetParam ())
    {
    case ending::signal:
      signal.emit (asio::cancellation_type::terminal);
      break;
    case ending::cancel:
      stream_.cancel ();
      break;
    case ending::close:
      stream_.close ();
      break;
    }
  settle ();
  EXPECT_EQ (impl_->write_calls.load (), 1) << "the next write started";
  ASSERT_EQ (write.calls, 1) << "the write_all did not end";
  EXPECT_TRUE (write.error == transport_errc::cancelled);
  EXPECT_EQ (write.bytes, 1u);
  stream_.close ();
  settle ();
}

// Control: a cancel before the write_all started does not concern it (as for
// every other operation), and the write_all ends normally.
TEST_P (
    LumexByteStreamWriteAllCancelTest,
    GivenACancelBeforeTheWriteAll_WhenTheDeviceCompletesEveryWrite_ThenItSucceeds)
{
  seen open;
  stream_.async_open (status_recorder{ &open });
  settle ();
  if (GetParam () == ending::close)
    return; // a close before the write_all ends it with not_open
  if (GetParam () == ending::cancel)
    stream_.cancel ();
  settle ();
  std::vector<std::uint8_t> const data (3, std::uint8_t (7));
  seen write;
  stream_.async_write_all (tr::const_bytes_t (data), io_recorder{ &write });
  settle ();
  for (int step = 0; step < 3 && write.calls == 0; ++step)
    {
      impl_->device_completes (1);
      settle ();
    }
  ASSERT_EQ (write.calls, 1);
  EXPECT_FALSE (write.error);
  EXPECT_EQ (write.bytes, 3u);
  EXPECT_EQ (impl_->write_calls.load (), 3);
}

INSTANTIATE_TEST_SUITE_P (Endings, LumexByteStreamWriteAllCancelTest,
                          ::testing::Values (ending::signal, ending::cancel,
                                             ending::close),
                          ending_name);

namespace
{
class LumexByteStreamWriteAllEndTest : public ::testing::Test
{
protected:
  void
  settle ()
  {
    context_.restart ();
    context_.poll ();
  }

  void
  open ()
  {
    seen opened;
    stream_.async_open (status_recorder{ &opened });
    settle ();
    ASSERT_EQ (opened.calls, 1);
  }

  asio::io_context context_;
  std::shared_ptr<held_impl> impl_
      = std::make_shared<held_impl> (context_.get_executor ());
  tr::stream::byte_stream stream_{ impl_ };
  std::vector<std::uint8_t> const data_
      = std::vector<std::uint8_t> (3, std::uint8_t (7));
};
} // namespace

// The device wrote the last bytes when the cancel runs (the completion is
// queued behind it): every byte is written, so the success wins.
TEST_F (
    LumexByteStreamWriteAllEndTest,
    GivenTheLastWriteDoneOnTheDevice_WhenACancelRunsBeforeItsCompletion_ThenTheWriteAllSucceeds)
{
  open ();
  seen write;
  stream_.async_write_all (tr::const_bytes_t (data_), io_recorder{ &write });
  settle ();
  impl_->device_completes (3);
  stream_.cancel ();
  settle ();
  ASSERT_EQ (write.calls, 1);
  EXPECT_FALSE (write.error);
  EXPECT_EQ (write.bytes, 3u);
  EXPECT_EQ (impl_->write_calls.load (), 1);
}

// A close while the last write is pending on the device: `cancelled`, the
// bytes written before it, and the statistics count those bytes and one
// cancellation (no completed write).
TEST_F (
    LumexByteStreamWriteAllEndTest,
    GivenTheLastWritePending_WhenTheStreamIsClosed_ThenCancelledWithTheBytesWrittenAndCounted)
{
  open ();
  seen write;
  stream_.async_write_all (tr::const_bytes_t (data_), io_recorder{ &write });
  settle ();
  impl_->device_completes (2);
  settle ();
  ASSERT_EQ (impl_->write_calls.load (), 2);
  stream_.close ();
  settle ();
  ASSERT_EQ (write.calls, 1);
  EXPECT_TRUE (write.error == transport_errc::cancelled);
  EXPECT_EQ (write.bytes, 2u);
  tr::statistics::statistics_t const counted = stream_.statistics ();
  EXPECT_EQ (counted.bytes_written, 2u);
  EXPECT_EQ (counted.cancellations, 1u);
  EXPECT_EQ (counted.writes, 0u);
}

// The synchronous write_all whose deadline passes during its second write:
// `timeout`, the bytes of the first write counted, the cancellation of the
// deadline counted as the timeout.
TEST (
    LumexByteStreamWriteAllDeadline,
    GivenASynchronousWriteAllWithItsSecondWritePending_WhenTheDeadlinePasses_ThenTimeoutAndTheBytesCounted)
{
  tr::stream::io_runner runner (1);
  std::shared_ptr<held_impl> const impl
      = std::make_shared<held_impl> (runner.get_executor ());
  tr::stream::byte_stream stream (impl);
  ASSERT_TRUE (stream.open (tr::deadline_t (2000)).has_value ());
  std::vector<std::uint8_t> const data (3, std::uint8_t (7));
  std::future<tr::status_result_t> result
      = std::async (std::launch::async,
                    [&stream, &data] ()
                      {
                        return stream.write_all (tr::const_bytes_t (data),
                                                 tr::deadline_t (400));
                      });
  for (int i = 0; i < 2000 && impl->write_calls.load () < 1; ++i)
    std::this_thread::sleep_for (std::chrono::milliseconds (1));
  impl->device_completes (1);
  for (int i = 0; i < 2000 && impl->write_calls.load () < 2; ++i)
    std::this_thread::sleep_for (std::chrono::milliseconds (1));
  ASSERT_EQ (impl->write_calls.load (), 2);
  ASSERT_EQ (result.wait_for (std::chrono::seconds (5)),
             std::future_status::ready);
  tr::status_result_t const got = result.get ();
  ASSERT_FALSE (got.has_value ());
  EXPECT_TRUE (got.error () == transport_errc::timeout);
  tr::statistics::statistics_t const counted = stream.statistics ();
  EXPECT_EQ (counted.bytes_written, 1u);
  EXPECT_EQ (counted.timeouts, 1u);
  EXPECT_EQ (counted.cancellations, 0u);
  EXPECT_EQ (counted.writes, 0u);
  stream.close ();
}
