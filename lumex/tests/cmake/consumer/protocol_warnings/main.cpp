// Every macro, option, field and query of lumex/core/protocol in one
// translation unit of a strict consumer: a synthetic table of an invented
// device (no real protocol) with a layout, a layer, a framed layer, every row
// option, the entries, the client methods and the error codes, and a layout
// of every field kind encoded and decoded in the three layouts. Exit code 0
// when every value read back is the one the table wrote.

#include <cstdint>
#include <cstdio>
#include <ratio>
#include <string>
#include <system_error>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace fixture
{
namespace dsl = lumex::core::protocol::dsl;

enum class mode
{
  idle,
  busy
};

struct codec
{
};

struct other_codec
{
};

struct header
{
};

LUMEX_PROTOCOL_LAYOUT (state_t,
                       (u8_<>, u16_le_<scale<std::ratio<1, 10>>>, u16_be_<>),
                       ("mode", "level", "counter"));

// clang-format off
#define FIXTURE_ROWS(X, X_AS)                                                 \
  X (PING, 0x01, "Liveness", (), (), (), (idempotent))                        \
  X (SET_LEVEL, 0x02, "Set the level",                                        \
     (u16_le_<scale<std::ratio<1, 100> > >, u8_<>), ("level", "channel"),     \
     (state_t),                                                               \
     (timeout (100), retries (2), accepted_in (mode::idle, mode::busy),       \
      log_level (level::info)))                                               \
  X_AS (ERASE, "DELETE", 0x03, "Erase", (), (), (state_t),                    \
        (dangerous, firmware (1, 9)))                                         \
  X_AS (READ_NAME, "read-name", "RN", "Read the name", (), (), (),            \
        (model ("A", "B"), deprecated))                                       \
  X (STREAM, 'S', "Stream", (), (), (state_t), (stream (50)))

#define FIXTURE_LAYER_ROWS(X, X_AS)                                           \
  X (SET_LEVEL, 0x12, "Set the level (wide)", (u16_be_<>), ("level"),         \
     (state_t), ())                                                           \
  X (RESET, 0x20, "Reset", (), (), (), (dangerous))
// clang-format on

LUMEX_PROTOCOL_DEFINE (fixture_protocol, codec, header, FIXTURE_ROWS);
LUMEX_PROTOCOL_EXTEND (fixture_layer, fixture_protocol, FIXTURE_LAYER_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (fixture_framed, fixture_layer, other_codec,
                              header, FIXTURE_LAYER_ROWS);
LUMEX_PROTOCOL_CHECK_IDENTIFIERS (FIXTURE_ROWS);

namespace layout = lumex::core::protocol::layout;
namespace field = lumex::core::protocol::field;

// The reflected enumeration macro itself is not clean under -Weverything (a
// note for core/reflection); the protocol headers are what this fixture
// checks.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
LUMEX_DEFINE_REFLECTED_ENUM (lamp, std::uint8_t, (off, 0), (on, 1))
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

LUMEX_PROTOCOL_CONTEXT_KEY (unit_type);

LUMEX_PROTOCOL_LAYOUT (pair_t, (u8_, i16_le_), ("a", "b"));

// clang-format off
#define INNER_ROWS(X, X_AS) \
  X (LAMP, 'L', "Lamp", (u8_), ("level"), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (inner_protocol, codec, header, INNER_ROWS);

// Every field kind once (the floating ones in their own layout: the decimal
// fixed-width layout has none).
// clang-format off
LUMEX_PROTOCOL_LAYOUT (kinds_t,
  (u8_, i8_<unit::percent>, u16_be_<scale<std::ratio<1, 100> > >, i24_le_,
   u32_be_, i64_le_, sign_magnitude_<i32_be_>, ascii_hex_<4>,
   dec_text_<4, 2>, char_, text_<4>, bits_<u8_, 2, 3>, mask_<u8_, 0x81>,
   split_<u16_le_, bits_<u16_le_, 0, 8>, bits_<u16_le_, 8, 8> >,
   enum_<u8_, lamp>, array_<u16_le_, 2>, counted_<u8_, u8_, 4>, u8_,
   computed_count_<i16_be_, count_from_<17>, 3>,
   union_by_<field_<17>, case_<0, u8_>, case_<1, pair_t>, case_<2, u16_le_>,
             case_<3, u8_> >,
   union_by_<u8_, case_<5, pair_t>, case_<6, u32_le_> >,
   union_by_context_<unit_type, case_<1, u8_>, case_<2, pair_t> >,
   optional_tail_<u16_le_, union_by_<at_end_<u8_>, case_<9, pair_t> > >),
  ("a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
   "p", "q", "r", "s", "t", "u", "v", "w"));
LUMEX_PROTOCOL_LAYOUT (floats_t, (f32_le_<unit::volt>, f64_be_, by_length_<2, u8_, u16_le_>),
                       ("a", "b", "c"));
LUMEX_PROTOCOL_LAYOUT (tunnel_t, (u8_, nested_<inner_protocol, char_>),
                       ("a", "b"));
LUMEX_PROTOCOL_LAYOUT (word_t, (u8_, token_), ("a", "b"));
// clang-format on

using kinds = field::record_of<kinds_t::fields_type>::type;

namespace
{
// A failed check counts one; a function, so that the compiler does not see
// a check whose result is known at compile time as dead code.
int
failed (bool ok)
{
  return ok ? 0 : 1;
}

bool
lookup_unit (void const *, char const *key, std::int64_t &value)
{
  value = 2;
  return std::string (key) == "unit_type";
}

kinds
make_kinds ()
{
  kinds value = kinds ();
  field::get<0> (value) = 1;
  field::get<1> (value) = -2;
  field::get<2> (value).raw = 1234;
  field::get<3> (value) = -70000;
  field::get<4> (value) = 0x01020304u;
  field::get<5> (value) = -5;
  field::get<6> (value) = -6;
  field::get<7> (value) = 0xBEEF;
  field::get<8> (value).raw = -1234;
  field::get<9> (value) = 'Q';
  field::get<10> (value).assign ("ab");
  field::get<11> (value) = 5;
  field::get<12> (value) = 2;
  field::get<13> (value).raw = 0x2211;
  field::get<0> (field::get<13> (value).parts) = 0x11;
  field::get<1> (field::get<13> (value).parts) = 0x22;
  field::get<14> (value) = lamp::on;
  field::get<15> (value)[1] = 7;
  field::get<16> (value).push_back (9);
  field::get<17> (value) = 1;
  field::get<18> (value).push_back (-3);
  field::get<19> (value).discriminant = 1;
  field::get<1> (field::get<19> (value).cases.emplace<1> ()) = -9;
  field::get<20> (value).discriminant = 6;
  field::get<20> (value).cases.set<1> (0xA0B0C0D0u);
  field::get<21> (value).discriminant = 2;
  field::get<0> (field::get<21> (value).cases.emplace<1> ()) = 4;
  field::get<22> (value).present = true;
  field::get<0> (field::get<22> (value).fields) = 0x0102;
  auto &tail = field::get<1> (field::get<22> (value).fields);
  tail.discriminant = 9;
  field::get<0> (tail.cases.emplace<0> ()) = 3;
  return value;
}

template <typename Layout>
int
round_trip_kinds (field::decode_context_t const &context)
{
  kinds const value = make_kinds ();
  layout::bytes_t bytes;
  if (!layout::encode<Layout, kinds_t> (bytes, value, &context))
    return 1;
  layout::decode_result_t<kinds> const back = layout::decode<Layout, kinds_t> (
      layout::bytes_view_t (bytes.data (), bytes.size ()), &context);
  return back.has_value () && *back == value ? 0 : 1;
}

int
check_layouts ()
{
  field::decode_context_t const context = { nullptr, &lookup_unit };
  int failures
      = round_trip_kinds<layout::binary_layout<>> (context)
        + round_trip_kinds<layout::fixed_width_text_layout<16>> (context)
        + round_trip_kinds<layout::fixed_width_text_layout<10>> (context)
        + round_trip_kinds<layout::tokenized_text_layout<';'>> (context);

  field::record_of<floats_t::fields_type>::type floats
      = field::record_of<floats_t::fields_type>::type ();
  field::get<0> (floats) = 1.5f;
  field::get<1> (floats) = -0.25;
  field::get<2> (floats).emplace<0> () = 5;
  layout::bytes_t bytes;
  failures += layout::encode<layout::tokenized_text_layout<','>, floats_t> (
                  bytes, floats)
                  ? 0
                  : 1;
  failures += layout::decode<layout::tokenized_text_layout<','>, floats_t> (
                  layout::bytes_view_t (bytes.data (), bytes.size ()))
                  ? 0
                  : 1;

  field::record_of<tunnel_t::fields_type>::type tunnel
      = field::record_of<tunnel_t::fields_type>::type ();
  std::uint8_t const payload[] = { 7 };
  field::get<1> (tunnel).key = 'L';
  field::get<1> (tunnel).payload = field::bytes_view_t (payload, 1);
  bytes.clear ();
  failures += layout::encode<layout::binary_layout<>, tunnel_t> (bytes, tunnel)
                  ? 0
                  : 1;
  failures += layout::decode_nested<inner_protocol, inner_protocol::LAMP_t> (
                  field::get<1> (tunnel))
                  ? 0
                  : 1;

  bytes.clear ();
  failures
      += layout::encode_request<fixture_protocol,
                                fixture_protocol::SET_LEVEL_t> (bytes, 1.25, 3)
             ? 0
             : 1;
  failures += layout::decode_request<fixture_protocol,
                                     fixture_protocol::SET_LEVEL_t> (
                  layout::bytes_view_t (bytes.data (), bytes.size ()))
                  ? 0
                  : 1;
  field::record_of<word_t::fields_type>::type word
      = field::record_of<word_t::fields_type>::type ();
  field::get<1> (word) = field::text_view ("rest");
  bytes.clear ();
  failures
      += layout::encode<layout::binary_layout<>, word_t> (bytes, word) ? 0 : 1;
  failures += failed (layout::is_aligned<fixture_protocol>::value);
  failures += failed (std::string (field::enum_name (lamp::on)) == "on");
  return failures;
}
} // namespace

struct client : fixture_layer::lumex_methods<client>
{
  template <typename Row, typename... Args>
  int
  lumex_invoke (Args... args)
  {
    int sum = static_cast<int> (dsl::make_entry<Row> ().key.integer);
    int const values[] = { 0, args... };
    for (int value : values)
      sum += value;
    return sum;
  }
};

namespace
{

int
check ()
{
  int failures = 0;
  using set_level = fixture_protocol::SET_LEVEL_t;
  failures += failed (
      dsl::get_option<dsl::kind::timeout> (set_level::options ()).milliseconds
      == 100);
  failures += failed (
      dsl::get_option<dsl::kind::retries> (set_level::options ()).count == 2);
  failures += failed (
      dsl::get_option<dsl::kind::accepted_in> (set_level::options ()).states[1]
      == mode::busy);
  failures += failed (
      dsl::get_option<dsl::kind::log_level> (set_level::options ()).value
      == dsl::level::info);
  failures += failed ((dsl::flags_of (fixture_protocol::ERASE_t::options ())
                       & dsl::row_flags::dangerous)
                      != 0);
  failures += failed (!dsl::has_option<dsl::kind::stream> (
      fixture_protocol::PING_t::options ()));
  failures
      += failed (std::string (fixture_protocol::ERASE_t::specification_name ())
                 == "DELETE");
  failures += failed (fixture_protocol::READ_NAME_t::key ().size == 2);
  failures += failed (std::string (state_t::field_names ()[2]) == "counter");

  failures += failed (dsl::command_count<fixture_layer>::value == 6
                      && dsl::command_count<fixture_framed>::value == 6);
  lumex::core::span::view::span<dsl::command_entry_t const> const entries
      = dsl::command_entries<fixture_layer> ();
  failures
      += failed (entries.size () == 6 && entries[1].key.integer == 0x12
                 && std::string (entries[5].identifier) == "RESET"
                 && entries[3].options.models.size () == 2
                 && entries[0].options.flags == dsl::row_flags::idempotent);

  client user;
  failures += failed (user.PING () == 0x01 && user.SET_LEVEL (3) == 0x12 + 3
                      && user.RESET () == 0x20);

  std::error_code const code
      = lumex::core::protocol::error::protocol_errc::truncated;
  failures += failed (!code.message ().empty ());
  failures += check_layouts ();
  return failures;
}
} // namespace
} // namespace fixture

int
main ()
{
  int const failures = fixture::check ();
  std::printf ("protocol_warnings: %d failures\n", failures);
  return failures == 0 ? 0 : 1;
}
