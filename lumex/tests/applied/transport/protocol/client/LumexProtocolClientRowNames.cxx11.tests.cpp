// The names a row may take on the generated client: a row named like a member
// of client<P> or client_layer<Owner, L> would hide its own method, so the
// check refuses it (the compile checks hold the refusals); a row of another
// identifier whose specification name is that of a member is no clash.

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
