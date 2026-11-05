// compile_time.cpp
// One translation unit per size of a `protocol::client<P>`, for
// compile_time.py. BENCH_TRANSPORT_CLIENT selects what is instantiated:
//
//   0  the headers of the transport, the framing and the protocol only (the
//      baseline: nothing of a table);
//   1  a client over the invented 7-row table of bench_transport_tables.hpp
//      with one method called (PING);
//   2  a client over layer device_m1 of the largest test table (device M, a
//      tokenized text protocol, 20 rows) with ONE method instantiated per row
//      of the layer (what the generated method forwards to);
//   3  as 2 over layer device_m2 of the same table (the binary protocol, 15
//      rows);
//   4  as 2 with the synchronous form of each method (no completion
//      handler), the form that blocks the caller.
//
// The test table of the device is de-identified (lumex/tests/core/protocol/
// devices); only its row count and the sizes below are reported.
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <type_traits>
#include <utility>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/framing/LumexFraming"
#include "lumex/core/protocol/LumexProtocol"

#ifndef BENCH_TRANSPORT_CLIENT
#define BENCH_TRANSPORT_CLIENT 0
#endif

#if BENCH_TRANSPORT_CLIENT == 1
#include "benchmarks/transport/bench_transport_tables.hpp"
#elif BENCH_TRANSPORT_CLIENT >= 2
#include "lumex/tests/core/protocol/devices/device_m/LumexProtocolDeviceM.hpp"
#endif

// The completion handler that follows the fields of a call; the synchronous
// form (variant 4) has none.
#if BENCH_TRANSPORT_CLIENT == 4
#define BENCH_COMPLETION
#else
#define BENCH_COMPLETION , [] (std::error_code, pr::reply_of<Row>) {}
#endif

namespace
{
namespace pr = lumex::applied::transport::protocol;
namespace layout = lumex::core::protocol::layout;
namespace field = lumex::core::protocol::field;
namespace dialogue = lumex::applied::transport::protocol::dialogue;

template <std::size_t... I> struct seq
{
};

template <std::size_t N, std::size_t... I>
struct make_seq : make_seq<N - 1, N - 1, I...>
{
};

template <std::size_t... I> struct make_seq<0, I...>
{
  using type = seq<I...>;
};

template <typename Row, typename Caller, typename Client, typename Request,
          std::size_t... I>
void
apply (Client &client, Request const &sample, seq<I...>, std::false_type)
{
  Caller::go (client, field::get<I> (sample)... BENCH_COMPLETION);
}

template <typename Row, typename Caller, typename Client, typename Request,
          std::size_t... I>
void
apply (Client &client, Request const &sample, seq<I...>, std::true_type)
{
  Caller::go (client, pr::confirm, field::get<I> (sample)... BENCH_COMPLETION);
}

// One call of the generated method of `Row`, with the sample of its fields.
template <typename Row, typename Caller, typename Client>
void
call_row (Client &client)
{
  using request = typename layout::request_of<Row>::type;
  request const sample = field::sample_value<typename Row::fields_type> ();
  apply<Row, Caller> (
      client, sample, typename make_seq<request::size::value>::type (),
      typename dialogue::detail::row_dangerous_t<Row>::type ());
}

#define BENCH_CALLER(id_, key_, desc_, fields_, names_, reply_, options_)     \
  struct call_##id_                                                           \
  {                                                                           \
    template <typename C, typename... A>                                      \
    static auto                                                               \
    go (C &c, A &&...a) -> decltype (c.id_ (std::forward<A> (a)...))          \
    {                                                                         \
      return c.id_ (std::forward<A> (a)...);                                  \
    }                                                                         \
  };
#define BENCH_USE(id_, key_, desc_, fields_, names_, reply_, options_)        \
  call_row<P::id_##_t, call_##id_> (client);
} // namespace

#if BENCH_TRANSPORT_CLIENT == 1
namespace
{
using P = bench_transport::bench_protocol_t;
struct call_PING
{
  template <typename C, typename... A>
  static auto
  go (C &c, A &&...a) -> decltype (c.PING (std::forward<A> (a)...))
  {
    return c.PING (std::forward<A> (a)...);
  }
};
} // namespace

void
use_client (pr::client<P> &client)
{
  call_row<P::PING_t, call_PING> (client);
}
#elif BENCH_TRANSPORT_CLIENT == 2 || BENCH_TRANSPORT_CLIENT == 4
namespace
{
using P = lumex_tests::device_m::device_m1;
DEVICE_M1_ROWS (BENCH_CALLER, BENCH_CALLER)
} // namespace

void
use_client (pr::client<P> &client)
{
  DEVICE_M1_ROWS (BENCH_USE, BENCH_USE)
}
#elif BENCH_TRANSPORT_CLIENT == 3
namespace
{
using P = lumex_tests::device_m::device_m2;
DEVICE_M2_ROWS (BENCH_CALLER, BENCH_CALLER)
} // namespace

void
use_client (pr::client<P> &client)
{
  DEVICE_M2_ROWS (BENCH_USE, BENCH_USE)
}
#endif
