// The keys of the table at run time (lumex/core/protocol/dsl/
// LumexProtocolKey.hpp): the unsigned bits of the key type and their width
// (REVIEW41 F6: '\xFF' and -1 were one value), composite keys, the cut to the
// width of the wire and the comparison of two keys.

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

namespace
{
namespace dsl = lumex::core::protocol::dsl;

enum class small_code : std::uint8_t
{
  low = 3,
  high = 0xF0
};

enum class signed_code : std::int16_t
{
  minus = -2
};

struct two_part_key
{
  typedef dsl::composite_key_tag lumex_composite_key;
  std::uint8_t type;
  std::uint8_t number;

  static constexpr std::size_t
  lumex_key_width ()
  {
    return 2;
  }

  constexpr std::uint64_t
  lumex_key_bits () const
  {
    return static_cast<std::uint64_t> ((type << 8) | number);
  }
};
} // namespace

TEST (LumexProtocolKeyWidthTest,
      GivenACharacterAbove0x7F_WhenTurnedIntoAKey_ThenItIsNotSignExtended)
{
  dsl::key_value_t const key = dsl::key_value (dsl::make_key ('\xFF'));
  EXPECT_TRUE (key.kind == dsl::key_kind::integer);
  EXPECT_EQ (key.integer, 0xFFu);
  EXPECT_EQ (key.width, 1u);
}

TEST (LumexProtocolKeyWidthTest,
      GivenMinusOneAsAnInt_WhenTurnedIntoAKey_ThenItHasAllFourBytes)
{
  dsl::key_value_t const key = dsl::key_value (dsl::make_key (-1));
  EXPECT_EQ (key.integer, 0xFFFFFFFFu);
  EXPECT_EQ (key.width, 4u);
}

TEST (LumexProtocolKeyWidthTest,
      GivenTheCharacterAndTheInt_WhenComparedAsTheyAre_ThenTheyDiffer)
{
  dsl::key_value_t const character = dsl::key_value (dsl::make_key ('\xFF'));
  dsl::key_value_t const number = dsl::key_value (dsl::make_key (-1));
  EXPECT_FALSE (dsl::keys_equal (character, number));
  EXPECT_NE (character.integer, number.integer);
}

TEST (LumexProtocolKeyWidthTest,
      GivenTheCharacterAndTheInt_WhenCutToAOneByteWire_ThenTheyAreOneKey)
{
  dsl::key_value_t const character
      = dsl::normalize_key (dsl::key_value (dsl::make_key ('\xFF')), 1);
  dsl::key_value_t const number
      = dsl::normalize_key (dsl::key_value (dsl::make_key (-1)), 1);
  EXPECT_EQ (character.integer, 0xFFu);
  EXPECT_EQ (number.integer, 0xFFu);
  EXPECT_EQ (number.width, 1u);
  EXPECT_TRUE (dsl::keys_equal (character, number));
}

TEST (LumexProtocolKeyWidthTest,
      GivenAWireWiderThanTheKeyType_WhenNormalized_ThenTheBitsAreNotExtended)
{
  dsl::key_value_t const key
      = dsl::normalize_key (dsl::key_value (dsl::make_key ('\xFF')), 4);
  EXPECT_EQ (key.integer, 0xFFu);
  EXPECT_EQ (key.width, 4u);
  EXPECT_EQ (
      dsl::normalize_key (dsl::key_value (dsl::make_key (-1)), 8).integer,
      0xFFFFFFFFu);
}

TEST (LumexProtocolKeyWidthTest,
      GivenAZeroWireWidth_WhenNormalized_ThenTheKeyIsKept)
{
  dsl::key_value_t const key = dsl::key_value (dsl::make_key (-1));
  dsl::key_value_t const same = dsl::normalize_key (key, 0);
  EXPECT_EQ (same.integer, key.integer);
  EXPECT_EQ (same.width, key.width);
}

TEST (LumexProtocolKeyWidthTest,
      GivenEnumerators_WhenTurnedIntoKeys_ThenTheirUnderlyingTypeGivesTheBits)
{
  EXPECT_EQ (dsl::key_value (dsl::make_key (small_code::high)).integer, 0xF0u);
  EXPECT_EQ (dsl::key_value (dsl::make_key (small_code::high)).width, 1u);
  EXPECT_EQ (dsl::key_value (dsl::make_key (signed_code::minus)).integer,
             0xFFFEu);
  EXPECT_EQ (dsl::key_value (dsl::make_key (signed_code::minus)).width, 2u);
}

TEST (LumexProtocolKeyWidthTest,
      GivenALongKey_WhenTurnedIntoAKey_ThenAll64BitsAreKept)
{
  dsl::key_value_t const key
      = dsl::key_value (dsl::make_key (static_cast<std::int64_t> (-2)));
  EXPECT_EQ (key.integer, 0xFFFFFFFFFFFFFFFEull);
  EXPECT_EQ (key.width, 8u);
  EXPECT_EQ (dsl::normalize_key (key, 8).integer, key.integer);
}

TEST (LumexProtocolKeyWidthTest,
      GivenABoolKey_WhenTurnedIntoAKey_ThenItIsZeroOrOne)
{
  EXPECT_EQ (dsl::key_value (dsl::make_key (true)).integer, 1u);
  EXPECT_EQ (dsl::key_value (dsl::make_key (false)).integer, 0u);
}

TEST (LumexProtocolKeyWidthTest,
      GivenACompositeKey_WhenTurnedIntoAKey_ThenItHoldsItsPackedBitsAndWidth)
{
  two_part_key const value = { 3, 5 };
  dsl::key_value_t const key = dsl::key_value (dsl::make_key (value));
  EXPECT_TRUE (key.kind == dsl::key_kind::composite);
  EXPECT_EQ (key.integer, 0x0305u);
  EXPECT_EQ (key.width, 2u);
  dsl::key_value_t const cut = dsl::normalize_key (key, 1);
  EXPECT_EQ (cut.integer, 0x05u);
  EXPECT_TRUE (cut.kind == dsl::key_kind::composite);
}

TEST (LumexProtocolKeyWidthTest,
      GivenATextKey_WhenCutToAWireWidth_ThenItIsUnchanged)
{
  dsl::key_value_t const key = dsl::key_value (dsl::make_key ("AZ"));
  dsl::key_value_t const cut = dsl::normalize_key (key, 1);
  EXPECT_TRUE (cut.kind == dsl::key_kind::text);
  EXPECT_EQ (cut.text_size, 2u);
}

TEST (LumexProtocolKeyWidthTest,
      GivenKeysOfEveryKind_WhenCompared_ThenKindBitsWidthAndCharactersCount)
{
  dsl::key_value_t const az = dsl::key_value (dsl::make_key ("AZ"));
  dsl::key_value_t const az_again = dsl::key_value (dsl::make_key ("AZ"));
  dsl::key_value_t const ay = dsl::key_value (dsl::make_key ("AY"));
  dsl::key_value_t const a = dsl::key_value (dsl::make_key ("A"));
  EXPECT_TRUE (dsl::keys_equal (az, az_again));
  EXPECT_FALSE (dsl::keys_equal (az, ay));
  EXPECT_FALSE (dsl::keys_equal (az, a));
  EXPECT_FALSE (dsl::keys_equal (dsl::key_value (dsl::make_key (5)), az));
  EXPECT_TRUE (dsl::keys_equal (dsl::key_value (dsl::make_key (5)),
                                dsl::key_value (dsl::make_key (5))));
  EXPECT_FALSE (dsl::keys_equal (dsl::key_value (dsl::make_key (5)),
                                 dsl::key_value (dsl::make_key (6))));
  EXPECT_FALSE (dsl::keys_equal (
      dsl::key_value (dsl::make_key (static_cast<std::uint8_t> (5))),
      dsl::key_value (dsl::make_key (5))));
  // A key of another type has no run-time form to compare.
  EXPECT_FALSE (dsl::keys_equal (dsl::key_value (dsl::make_key (1.5)),
                                 dsl::key_value (dsl::make_key (1.5))));
}
