// The type lists and keys of the DSL (lumex/core/protocol/dsl/
// LumexProtocolTypeList.hpp, LumexProtocolKey.hpp): LUMEX_PP_UNWRAP,
// list_of, list_size, list_at, list_contains, concat, append_unique,
// make_key and key_value.

#include <cstdint>
#include <ratio>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

namespace
{
namespace dsl = lumex::core::protocol::dsl;

struct a
{
};
struct b
{
};
struct c
{
};
struct d
{
};

enum class colour : std::uint8_t
{
  red = 7
};

// What a row macro does with its parenthesized group argument.
#define TYPE_LIST_TEST_LIST(group) dsl::type_list<LUMEX_PP_UNWRAP group>
} // namespace

TEST (LumexProtocolTypeListTest,
      GivenAGroupWithCommas_WhenUnwrapped_ThenEveryElementIsAnArgument)
{
  typedef TYPE_LIST_TEST_LIST ((a, std::ratio<1, 100>, b)) list;
  typedef TYPE_LIST_TEST_LIST (()) empty;
  EXPECT_EQ (dsl::list_size<empty>::value, 0u);
  EXPECT_EQ (dsl::list_size<list>::value, 3u);
  EXPECT_TRUE (
      (std::is_same<dsl::list_at<1, list>::type, std::ratio<1, 100>>::value));
}

TEST (LumexProtocolTypeListTest,
      GivenASentinelFirst_WhenListOf_ThenTheSentinelIsDropped)
{
  EXPECT_TRUE ((std::is_same<dsl::list_of<void, a, b>::type,
                             dsl::type_list<a, b>>::value));
  EXPECT_TRUE (
      (std::is_same<dsl::list_of<void>::type, dsl::type_list<>>::value));
}

TEST (LumexProtocolTypeListTest,
      GivenLists_WhenConcatenated_ThenTheOrderIsKept)
{
  EXPECT_TRUE (
      (std::is_same<dsl::concat<dsl::type_list<a>, dsl::type_list<b, c>,
                                dsl::type_list<>>::type,
                    dsl::type_list<a, b, c>>::value));
  EXPECT_TRUE ((std::is_same<dsl::concat<>::type, dsl::type_list<>>::value));
}

TEST (LumexProtocolTypeListTest,
      GivenAList_WhenAskedForMembers_ThenOnlyItsTypesAreContained)
{
  typedef dsl::type_list<a, b> list;
  EXPECT_TRUE ((dsl::list_contains<list, a>::value));
  EXPECT_TRUE ((dsl::list_contains<list, b>::value));
  EXPECT_FALSE ((dsl::list_contains<list, c>::value));
  EXPECT_FALSE ((dsl::list_contains<dsl::type_list<>, a>::value));
}

TEST (LumexProtocolTypeListTest,
      GivenABaseAndAddedTypes_WhenAppendedUniquely_ThenOnlyNewTypesFollow)
{
  EXPECT_TRUE ((std::is_same<dsl::append_unique<dsl::type_list<a, b, c>,
                                                dsl::type_list<b, d, a>>::type,
                             dsl::type_list<a, b, c, d>>::value));
  EXPECT_TRUE ((std::is_same<
                dsl::append_unique<dsl::type_list<>, dsl::type_list<a>>::type,
                dsl::type_list<a>>::value));
  EXPECT_TRUE ((std::is_same<
                dsl::append_unique<dsl::type_list<a>, dsl::type_list<>>::type,
                dsl::type_list<a>>::value));
}

TEST (LumexProtocolTypeListTest,
      GivenKeysOfEveryKind_WhenMade_ThenTheyKeepTheirTypeAndValue)
{
  EXPECT_TRUE ((std::is_same<decltype (dsl::make_key (0x1D)), int>::value));
  EXPECT_TRUE ((std::is_same<decltype (dsl::make_key ('A')), char>::value));
  EXPECT_TRUE (
      (std::is_same<decltype (dsl::make_key (colour::red)), colour>::value));
  EXPECT_TRUE (
      (std::is_same<decltype (dsl::make_key ("AZ")), dsl::text_key>::value));
  static_assert (dsl::make_key ("AZ").size == 2, "text key size");
  static_assert (dsl::make_key ("AZ").text[1] == 'Z', "text key text");
}

TEST (LumexProtocolTypeListTest,
      GivenKeys_WhenMadeRunTimeValues_ThenKindAndValueAreKept)
{
  dsl::key_value_t const code = dsl::key_value (dsl::make_key (0x1D));
  EXPECT_TRUE (code.kind == dsl::key_kind::integer);
  EXPECT_EQ (code.integer, 0x1Du);
  dsl::key_value_t const enumerator
      = dsl::key_value (dsl::make_key (colour::red));
  EXPECT_TRUE (enumerator.kind == dsl::key_kind::integer);
  EXPECT_EQ (enumerator.integer, 7u);
  dsl::key_value_t const mnemonic = dsl::key_value (dsl::make_key ("AZ"));
  EXPECT_TRUE (mnemonic.kind == dsl::key_kind::text);
  EXPECT_EQ (std::string (mnemonic.text, mnemonic.text_size), "AZ");
  dsl::key_value_t const other = dsl::key_value (dsl::make_key (a ()));
  EXPECT_TRUE (other.kind == dsl::key_kind::other);
  EXPECT_EQ (other.text, nullptr);
}
