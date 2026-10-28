// The vocabulary of a table
// (lumex/core/protocol/dsl/LumexProtocolVocabulary.hpp) and the identifier
// guard (LumexProtocolPlatformMacros.hpp): field names with and without <>,
// every field kind usable without a namespace, the words that are option
// types, the keyword list and the check of a row identifier against the macros
// of the translation unit. The refusals themselves (a keyword, a macro, a
// vocabulary word where a field belongs, a reply naming the row) are in
// cmake.protocol_compile_checks.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ratio>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

namespace
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;

struct codec
{
};

struct header
{
};

enum class shade : std::uint8_t
{
  dark,
  light
};

LUMEX_PROTOCOL_LAYOUT (plain_t, (u8_, i24_be_, f32_le_, char_),
                       ("a", "b", "c", "d"));
LUMEX_PROTOCOL_LAYOUT (pair_t, (u8_, u8_), ("a", "b"));

// clang-format off
#define VOCABULARY_ROWS(X, X_AS) \
  X (PLAIN, 1, "Names without <>", (u8_, u16_le_, i24_be_, token_), \
     ("a", "b", "c", "d"), (plain_t), ()) \
  X (MIXED, 2, "Names with <>", (u8_<>, u16_be_<scale<std::ratio<1, 10> > >), \
     ("a", "b"), (u32_le_), ()) \
  X (EVERY, 3, "Every kind", \
     (sign_magnitude_<i16_be_>, ascii_hex_<2>, dec_text_<2, 1>, text_<3>, \
      bits_<u8_, 0, 3>, mask_<u8_, 0x0C>, split_<u8_, bits_<u8_, 0, 4>, \
      bits_<u8_, 4, 4> >, enum_<u8_, shade>, array_<u8_, 2>, \
      counted_<u8_, u8_, 3>, u8_, computed_count_<u8_, count_from_<10>, 3>, \
      union_by_<field_<10>, case_<0, u8_>, case_<1, pair_t> >, \
      optional_tail_<u8_, i16_le_<unit::millivolt> >), \
     ("a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n"), \
     (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (vocabulary_protocol, codec, header, VOCABULARY_ROWS);

using plain_row = vocabulary_protocol::PLAIN_t;
} // namespace

TEST (
    LumexProtocolVocabularyTest,
    GivenFieldNamesWithoutAngleBrackets_WhenARowIsMade_ThenTheyAreThePlainFields)
{
  EXPECT_TRUE (
      (std::is_same<plain_row::fields_type,
                    dsl::field_list<field::u8_<>, field::u16_le_<>,
                                    field::i24_be_<>, field::token_>>::value));
  EXPECT_TRUE (
      (std::is_same<plain_t::fields_type,
                    dsl::field_list<field::u8_<>, field::i24_be_<>,
                                    field::f32_le_<>, field::char_>>::value));
  EXPECT_TRUE (
      (std::is_same<
          vocabulary_protocol::MIXED_t::fields_type,
          dsl::field_list<field::u8_<>,
                          field::u16_be_<field::scale<std::ratio<1, 10>>>>>::
           value));
  EXPECT_TRUE ((std::is_same<vocabulary_protocol::MIXED_t::reply_type,
                             field::u32_le_<>>::value));
  EXPECT_TRUE ((std::is_same<field::resolve<dsl::plain::u8_<>>::type,
                             field::u8_<>>::value));
  EXPECT_TRUE (field::is_field<dsl::plain::char_<>>::value);
  EXPECT_EQ (vocabulary_protocol::EVERY_t::fields_type::count::value, 14u);
}

TEST (LumexProtocolVocabularyTest,
      GivenTheOptionWords_WhenAsked_ThenTheyAreWordsAndFieldsAreNot)
{
  EXPECT_TRUE (dsl::is_vocabulary_word<dsl::timeout_t>::value);
  EXPECT_TRUE (dsl::is_vocabulary_word<dsl::retries_t>::value);
  EXPECT_TRUE (dsl::is_vocabulary_word<dsl::stream_t>::value);
  EXPECT_TRUE (dsl::is_vocabulary_word<dsl::log_level_t>::value);
  EXPECT_TRUE (dsl::is_vocabulary_word<dsl::firmware_t>::value);
  EXPECT_TRUE (dsl::is_vocabulary_word<dsl::level>::value);
  EXPECT_TRUE (dsl::is_vocabulary_word<field::unit>::value);
  EXPECT_FALSE (dsl::is_vocabulary_word<field::u8_<>>::value);
  EXPECT_FALSE (dsl::is_vocabulary_word<plain_t>::value);
  EXPECT_FALSE (dsl::is_vocabulary_word<int>::value);
  EXPECT_TRUE (
      (std::is_same<dsl::vocabulary::timeout, dsl::timeout_t>::value));
  EXPECT_EQ (dsl::vocabulary::timeout (250).milliseconds, 250u);
  EXPECT_EQ (dsl::vocabulary::firmware (3, 9).to, 9u);
}

TEST (LumexProtocolVocabularyTest,
      GivenTheKeywordList_WhenRead_ThenItIsSortedAndHoldsEveryKeyword)
{
  ASSERT_GT (dsl::keyword_count (), 80u);
  for (std::size_t index = 1; index < dsl::keyword_count (); ++index)
    EXPECT_LT (
        std::strcmp (dsl::keyword_name (index - 1), dsl::keyword_name (index)),
        0)
        << dsl::keyword_name (index);
  char const *const keywords[]
      = { "delete",        "int",     "and",      "xor_eq",
          "co_await",      "char8_t", "requires", "consteval",
          "static_assert", "alignas", "this",     "typename" };
  for (char const *name : keywords)
    EXPECT_TRUE (dsl::is_cpp_keyword (name)) << name;
  char const *const others[] = { "PING",  "Delete",   "deleted", "integer",
                                 "final", "override", "module",  "" };
  for (char const *name : others)
    EXPECT_FALSE (dsl::is_cpp_keyword (name)) << name;
  static_assert (dsl::is_cpp_keyword ("delete"), "a constant expression");
}

TEST (
    LumexProtocolVocabularyTest,
    GivenAnIdentifier_WhenStringizedBeforeAndAfterExpansion_ThenOnlyAMacroDiffers)
{
#define LUMEX_TEST_EXPANDS_TO_SOMETHING (0x10 + 1)
  static_assert (
      dsl::same_text ("PING", LUMEX_PROTOCOL_DETAIL_EXPANDED (PING)),
      "an identifier is itself");
  static_assert (!dsl::same_text ("LUMEX_TEST_EXPANDS_TO_SOMETHING",
                                  LUMEX_PROTOCOL_DETAIL_EXPANDED (
                                      LUMEX_TEST_EXPANDS_TO_SOMETHING)),
                 "a macro expands");
#undef LUMEX_TEST_EXPANDS_TO_SOMETHING
  EXPECT_TRUE (dsl::same_text ("", ""));
  EXPECT_FALSE (dsl::same_text ("a", "ab"));
}
