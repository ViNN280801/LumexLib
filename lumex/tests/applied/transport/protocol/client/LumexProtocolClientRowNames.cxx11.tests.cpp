// The names a row may take on the generated client: a row named like a member
// of client<P> or client_layer<Owner, L> would make the call ambiguous, so the
// check refuses it (the compile checks hold the refusals); a row of another
// identifier whose specification name is that of a member is no clash, and the
// names the members keep inside (they carry the prefix lumex_) are free.

#include <cstdint>
#include <memory>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp"

namespace
{
using namespace lumex_client_test;
namespace dsl = lumex::core::protocol::dsl;
namespace cdetail = lumex::applied::transport::protocol::detail;

using named = tables::spec_named_t;
using members = tables::members_t;
using inner = tables::inner_names_t;

/// The value a call gave, or why it did not.
template <typename Result>
void
expect_value (char const *row, Result const &result, int expected)
{
  ASSERT_TRUE (result.has_value ())
      << row << ": " << result.error ().message ();
  EXPECT_EQ (*result, expected) << row;
}

/// Whether the identifier of every row of the list is taken (`Taken`) or free
/// in `Own`.
template <typename List, typename Own, bool Taken> struct names_t;

template <typename... Rows, typename Own, bool Taken>
struct names_t<dsl::type_list<Rows...>, Own, Taken>
    : dialogue::detail::all_t<(Rows::template lumex_name_free_in_t<Own>::value
                               != Taken)...>
{
};
} // namespace

TEST (LumexProtocolClientRowNames, Client_ASpecificationNameOfAMemberIsNoClash)
{
  client_bench_t<named> bench;
  bench.device->set_reply<named::SHUT_t> (std::uint8_t (5));
  auto const shut = new_outcome<named::SHUT_t> ();
  bench.client->SHUT (into<named::SHUT_t> (shut));
  ASSERT_TRUE (bench.run_until ([&] () { return shut->calls != 0; }));
  EXPECT_FALSE (shut->error) << shut->error.message ();
  EXPECT_EQ (shut->value, 5);
  bench.device->set_reply<named::ATTACH_t> (std::uint8_t (9));
  auto const attach = new_outcome<named::ATTACH_t> ();
  bench.client->ATTACH (std::uint8_t (1), into<named::ATTACH_t> (attach));
  ASSERT_TRUE (bench.run_until ([&] () { return attach->calls != 0; }));
  EXPECT_FALSE (attach->error) << attach->error.message ();
  EXPECT_EQ (attach->value, 9);
  // close and on stay the members of the client.
  int heard = 0;
  pr::subscription_t const made = bench.client->on<named::ASK_t> (
      [&heard] (pr::reply_of<named::ASK_t> const &) { ++heard; });
  EXPECT_NE (made.id, 0u);
  bench.client->close ();
  bench.settle ();
  auto const after = new_outcome<named::SHUT_t> ();
  bench.client->SHUT (into<named::SHUT_t> (after));
  ASSERT_TRUE (bench.run_until ([&] () { return after->calls != 0; }));
  EXPECT_TRUE (after->error);
}

TEST (LumexProtocolClientRowNames, Client_EveryMemberNameIsTakenAndNoRowNameIs)
{
  // A table with a row for each member of client<P>, of every kind: a
  // function, an overloaded one, a template, a type, the class name.
  static_assert (names_t<dsl::commands_of<members>::type,
                         cdetail::client_names_t<members>, true>::value,
                 "every member of client<P> is seen by the lookup of a row of "
                 "its name");
  // The rows of the mixer name no member.
  static_assert (names_t<dsl::commands_of<mixer>::type,
                         cdetail::client_names_t<mixer>, false>::value,
                 "the identifiers of the mixer are free");
  SUCCEED ();
}

TEST (LumexProtocolClientRowNames, Layer_EveryMemberNameIsTakenAndNoRowNameIs)
{
  static_assert (
      names_t<dsl::commands_of<tables::layer_members_t>::type,
              cdetail::client_layer_names_t<tables::layer_members_t,
                                            tables::layer_members_t>,
              true>::value,
      "lumex_invoke, on and the class name are seen");
  static_assert (
      names_t<dsl::commands_of<mixer_v2>::type,
              cdetail::client_layer_names_t<mixer, mixer_v2>, false>::value,
      "the identifiers of a layer are free");
  SUCCEED ();
}

TEST (LumexProtocolClientRowNames, Client_TheInnerNamesAreFreeForRows)
{
  // core, traits, state_ and the others were once names of private members:
  // a row so named compiles on the client and on a layer of it, and works.
  static_assert (names_t<dsl::commands_of<inner>::type,
                         cdetail::client_names_t<inner>, false>::value,
                 "the inner names are free on client<P>");
  static_assert (
      names_t<dsl::commands_of<inner>::type,
              cdetail::client_layer_names_t<inner, inner>, false>::value,
      "the inner names are free on client_layer<Owner, L>");
  thread_bench_t<inner> bench;
  bench.device->set_reply<inner::core_t> (std::uint8_t (1));
  bench.device->set_reply<inner::traits_t> (std::uint8_t (2));
  bench.device->set_reply<inner::identify_signature_t> (std::uint8_t (3));
  bench.device->set_reply<inner::identify_sync_t> (std::uint8_t (4));
  bench.device->set_reply<inner::state__t> (std::uint8_t (5));
  bench.device->set_reply<inner::engine__t> (std::uint8_t (6));
  bench.device->set_reply<inner::core__t> (std::uint8_t (7));
  expect_value ("core", bench.client->core (), 1);
  expect_value ("traits", bench.client->traits (), 2);
  expect_value ("identify_signature", bench.client->identify_signature (), 3);
  expect_value ("identify_sync", bench.client->identify_sync (), 4);
  expect_value ("state_", bench.client->state_ (), 5);
  expect_value ("engine_", bench.client->engine_ (), 6);
  expect_value ("core_", bench.client->core_ (), 7);
  // A layer of a client that never identified answers not_supported: the
  // methods of the layer are there and were reached.
  pr::call_result_t<inner::core__t> const layer_core_
      = bench.client->as<inner> ().core_ ();
  ASSERT_FALSE (layer_core_.has_value ());
  EXPECT_EQ (layer_core_.error (), protocol_errc::not_supported);
  pr::call_result_t<inner::core_t> const layer_core
      = bench.client->as<inner> ().core ();
  ASSERT_FALSE (layer_core.has_value ());
  EXPECT_EQ (layer_core.error (), protocol_errc::not_supported);
}
