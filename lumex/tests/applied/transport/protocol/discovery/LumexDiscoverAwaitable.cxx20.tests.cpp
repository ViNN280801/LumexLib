// async_discover with a coroutine token (gated as every awaitable test).

#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientReconnectSupport.hpp"

#if defined(ASIO_HAS_CO_AWAIT)
namespace
{
using namespace lumex_client_test;
using results_t = std::vector<pr::discovery_result_t>;

struct wire_t
{
  static pr::frame_link
  link (pr::discovery_candidate_t const &, tr::stream::byte_stream opened)
  {
    return pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
        std::move (opened), fr::slip::slip_codec ()));
  }
};

TEST (LumexDiscoverAwaitable, Token_UseAwaitable)
{
  asio::io_context context;
  tr::testing::virtual_clock clock;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor ());
  tr::testing::device_simulator<gauge_a> device (
      mock.control, fr::slip::slip_codec (), clock);
  device.set_reply<gauge_a::WHO_t> (gauge_who ("A1", 1, 1));
  std::shared_ptr<tr::stream::stream_impl> const impl = mock.stream.impl ();
  std::vector<pr::discovery_candidate_t> candidates (1);
  candidates[0].kind = pr::discovery_candidate_t::candidate_kind::stream;
  candidates[0].stream_factory = [impl] (tr::executor_type const &)
    { return tr::stream::byte_stream (impl); };
  results_t results;
  asio::co_spawn (
      context,
      [&] () -> asio::awaitable<void>
        {
          results = co_await pr::async_discover<gauge, wire_t> (
              context.get_executor (), candidates, pr::discovery_options_t (),
              asio::use_awaitable);
        },
      asio::detached);
  context.run_for (std::chrono::seconds (10));
  ASSERT_EQ (results.size (), 1u);
  EXPECT_TRUE (results[0].found);
  EXPECT_EQ (results[0].identity.model, "A1");
}
} // namespace
#endif
