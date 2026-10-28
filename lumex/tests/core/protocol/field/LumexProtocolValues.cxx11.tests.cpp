// The value types of decoded fields
// (lumex/core/protocol/field/LumexProtocolValues.hpp and
// LumexProtocolRecord.hpp): fixed_point, text_view, fixed_string,
// bounded_array, alternatives, records and their visit.

#include <cstdint>
#include <ratio>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"

namespace
{
namespace field = lumex::core::protocol::field;
namespace tables = lumex_tests::field_tables;

struct name_collector
{
  std::vector<std::string> *kinds;

  template <typename F, typename Value>
  void
  operator() (std::size_t index, field::field_type_tag<F>, Value const &)
  {
    kinds->push_back (std::to_string (index) + ":"
                      + std::to_string (static_cast<int> (
                          field::impl_of<F>::type::info ().kind)));
  }
};
} // namespace

TEST (LumexProtocolValuesTest,
      GivenAFixedPoint_WhenRead_ThenTheValueIsTheRawTimesTheRatio)
{
  field::fixed_point<std::int16_t, std::ratio<1, 4>> const value = { -6 };
  EXPECT_DOUBLE_EQ (value.value (), -1.5);
  field::fixed_point<std::int16_t, std::ratio<1, 4>> const same = { -6 };
  field::fixed_point<std::int16_t, std::ratio<1, 4>> const other = { 6 };
  EXPECT_TRUE (value == same);
  EXPECT_TRUE (value != other);
  EXPECT_TRUE ((std::is_trivially_copyable<decltype (value)>::value));
}

TEST (LumexProtocolValuesTest,
      GivenTextsAndViews_WhenCompared_ThenTheCharactersDecide)
{
  EXPECT_TRUE (field::text_view ("abc") == field::text_view ("abc"));
  EXPECT_TRUE (field::text_view ("abc") != field::text_view ("abd"));
  EXPECT_TRUE (field::text_view () == field::text_view (""));
  EXPECT_TRUE (
      field::text_view (static_cast<char const *> (nullptr)).empty ());
  field::fixed_string<4> text = field::fixed_string<4> ();
  EXPECT_TRUE (text.assign ("abcd"));
  EXPECT_FALSE (text.assign ("abcde"));
  EXPECT_TRUE (text.view () == field::text_view ("abcd"));
  EXPECT_TRUE (text.assign ("x"));
  EXPECT_EQ (text.chars[1], '\0');
  field::fixed_string<4> copy = text;
  EXPECT_TRUE (copy == text);
}

TEST (LumexProtocolValuesTest,
      GivenABoundedArray_WhenFilled_ThenItStopsAtItsCapacity)
{
  field::bounded_array<int, 2> values = field::bounded_array<int, 2> ();
  EXPECT_TRUE (values.push_back (1));
  EXPECT_TRUE (values.push_back (2));
  EXPECT_FALSE (values.push_back (3));
  EXPECT_EQ (values.size (), 2u);
  EXPECT_EQ (values.capacity (), 2u);
  int sum = 0;
  for (int value : values)
    sum += value;
  EXPECT_EQ (sum, 3);
  field::bounded_array<int, 2> other = values;
  EXPECT_TRUE (other == values);
  other[1] = 5;
  EXPECT_TRUE (other != values);
}

TEST (LumexProtocolValuesTest,
      GivenAlternatives_WhenSwitched_ThenTheIndexAndTheValueFollow)
{
  using three = field::alternatives<std::uint8_t, field::fixed_string<3>,
                                    std::uint64_t>;
  three value;
  EXPECT_EQ (value.index (), 0u);
  EXPECT_EQ (value.get<0> (), 0u);
  value.emplace<1> ().assign ("ab");
  EXPECT_EQ (value.index (), 1u);
  EXPECT_TRUE (value.get<1> ().view () == field::text_view ("ab"));
  three copy = value;
  EXPECT_TRUE (copy == value);
  value.set<2> (0x1122334455667788ull);
  EXPECT_EQ (value.index (), 2u);
  EXPECT_EQ (value.get<2> (), 0x1122334455667788ull);
  EXPECT_TRUE (copy != value);
  EXPECT_TRUE ((std::is_trivially_copyable<three>::value));
}

TEST (LumexProtocolValuesTest,
      GivenARecord_WhenVisited_ThenEveryFieldInOrderWithItsType)
{
  using point = field::record_of<tables::point_t::fields_type>::type;
  point value = point ();
  field::get<0> (value) = -3;
  field::get<1> (value) = 9;
  EXPECT_EQ (point::size::value, 2u);
  std::vector<std::string> kinds;
  field::visit_fields (value, name_collector{ &kinds });
  ASSERT_EQ (kinds.size (), 2u);
  EXPECT_EQ (kinds[0], "0:0");
  EXPECT_EQ (kinds[1], "1:0");
  point copy = value;
  EXPECT_TRUE (copy == value);
  field::get<1> (copy) = 8;
  EXPECT_TRUE (copy != value);
}
