// The fixture of the completion token suites (LumexTransportTokens.cxx11 and
// LumexTransportAwaitable.cxx20): one byte_stream per test, the mock stream
// (fed the reply beforehand) or a TCP stream to a loopback echo peer. Every
// style sends "ping" and reads back "ping"; for the mock, TearDown also
// checks what was written.

#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_TOKEN_FIXTURE_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_TOKEN_FIXTURE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace lumex_transport_tokens
{
namespace tr = lumex::applied::transport;

enum class target_kind
{
  mock,
  tcp
};

// The CTest name of a parameter (gtest_discover_tests prints the value).
inline void
PrintTo (target_kind kind, std::ostream *out)
{
  *out << (kind == target_kind::mock ? "mock" : "tcp");
}

inline std::vector<std::uint8_t>
ping ()
{
  return std::vector<std::uint8_t> ({ 'p', 'i', 'n', 'g' });
}

// Echoes every byte of one connection, on its own context and thread.
class tcp_echo_peer
{
public:
  tcp_echo_peer ()
      : acceptor_ (context_, asio::ip::tcp::endpoint (
                                 asio::ip::address_v4::loopback (), 0)),
        socket_ (context_), buffer_ (256)
  {
    acceptor_.async_accept (socket_,
                            [this] (asio::error_code const &error)
                              {
                                if (!error)
                                  read ();
                              });
    thread_ = std::thread ([this] { context_.run (); });
  }

  ~tcp_echo_peer ()
  {
    context_.stop ();
    thread_.join ();
  }

  tcp_echo_peer (tcp_echo_peer const &) = delete;
  tcp_echo_peer &operator= (tcp_echo_peer const &) = delete;

  std::uint16_t
  port () const
  {
    return acceptor_.local_endpoint ().port ();
  }

private:
  void
  read ()
  {
    socket_.async_read_some (
        asio::buffer (buffer_),
        [this] (asio::error_code const &error, std::size_t bytes)
          {
            if (error)
              return;
            asio::async_write (
                socket_, asio::buffer (buffer_.data (), bytes),
                [this] (asio::error_code const &written, std::size_t)
                  {
                    if (!written)
                      read ();
                  });
          });
  }

  asio::io_context context_;
  asio::ip::tcp::acceptor acceptor_;
  asio::ip::tcp::socket socket_;
  std::vector<std::uint8_t> buffer_;
  std::thread thread_;
};

class LumexTransportTokenTest : public ::testing::TestWithParam<target_kind>
{
protected:
  LumexTransportTokenTest () : runner_ (2) {}

  void
  SetUp () override
  {
    if (GetParam () == target_kind::mock)
      {
        mock_.reset (new tr::testing::mock_stream (
            tr::testing::make_mock_stream (runner_.get_executor (), "mock")));
        mock_->control.feed (ping ());
        return;
      }
    peer_.reset (new tcp_echo_peer ());
    tr::net::tcp_options_t options;
    options.host = "127.0.0.1";
    options.port = peer_->port ();
    tcp_.reset (new tr::stream::byte_stream (
        tr::net::make_tcp_stream (runner_.get_executor (), options)));
  }

  void
  TearDown () override
  {
    if (mock_ && !IsSkipped ())
      EXPECT_EQ (mock_->control.written (), ping ());
  }

  tr::stream::byte_stream &
  stream ()
  {
    return mock_ ? mock_->stream : *tcp_;
  }

  tr::stream::io_runner runner_;
  std::unique_ptr<tcp_echo_peer> peer_;
  std::unique_ptr<tr::testing::mock_stream> mock_;
  std::unique_ptr<tr::stream::byte_stream> tcp_;
};

inline std::string
target_name (::testing::TestParamInfo<target_kind> const &info)
{
  return info.param == target_kind::mock ? "Mock" : "Tcp";
}
} // namespace lumex_transport_tokens

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_TOKEN_FIXTURE_HPP
