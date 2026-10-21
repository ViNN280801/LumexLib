// The error codes of the frame codecs (lumex/core/framing/codec): the
// category, the message of every value and the conversion to std::error_code
// and std::error_condition.

#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/framing/LumexFraming"

namespace
{
using lumex::core::framing::codec::framing_category;
using lumex::core::framing::codec::framing_errc;

framing_errc const kEveryValue[]
    = { framing_errc::frame_too_large, framing_errc::bad_checksum,
        framing_errc::bad_escape,      framing_errc::bad_header,
        framing_errc::truncated,       framing_errc::payload_too_large };
} // namespace

TEST (LumexFramingErrcTest, GivenTheCategory_WhenNamed_ThenItIsLumexFraming)
{
  EXPECT_STREQ (framing_category ().name (), "lumex.framing");
}

TEST (LumexFramingErrcTest,
      GivenTheCategory_WhenAskedTwice_ThenItIsTheSameObject)
{
  EXPECT_EQ (&framing_category (), &framing_category ());
}

TEST (LumexFramingErrcTest,
      GivenEveryValue_WhenConverted_ThenTheCodeHasTheValueAndTheCategory)
{
  for (framing_errc value : kEveryValue)
    {
      std::error_code const code = value;
      EXPECT_EQ (code.value (), static_cast<int> (value));
      EXPECT_EQ (&code.category (), &framing_category ());
      EXPECT_TRUE (code == value);
      EXPECT_TRUE (static_cast<bool> (code));
    }
}

TEST (LumexFramingErrcTest, GivenEveryValue_WhenMessaged_ThenTheTextIsDistinct)
{
  for (framing_errc first : kEveryValue)
    {
      std::string const text = std::error_code (first).message ();
      EXPECT_FALSE (text.empty ());
      EXPECT_NE (text, "unknown framing error");
      for (framing_errc second : kEveryValue)
        if (first != second)
          EXPECT_NE (text, std::error_code (second).message ());
    }
}

TEST (LumexFramingErrcTest,
      GivenAValueOutsideTheEnum_WhenMessaged_ThenTheTextSaysUnknown)
{
  EXPECT_EQ (framing_category ().message (0), "unknown framing error");
  EXPECT_EQ (framing_category ().message (1000), "unknown framing error");
}

TEST (LumexFramingErrcTest,
      GivenTwoDifferentValues_WhenCompared_ThenTheCodesDiffer)
{
  std::error_code const checksum = framing_errc::bad_checksum;
  EXPECT_FALSE (checksum == framing_errc::bad_header);
  EXPECT_TRUE (checksum != framing_errc::bad_header);
}

TEST (LumexFramingErrcTest,
      GivenAValue_WhenMadeACondition_ThenItMatchesTheCode)
{
  std::error_condition const condition
      = make_error_condition (framing_errc::truncated);
  EXPECT_EQ (&condition.category (), &framing_category ());
  EXPECT_TRUE (std::error_code (framing_errc::truncated) == condition);
}

TEST (LumexFramingErrcTest,
      GivenACodeOfAnotherCategory_WhenComparedWithAValue_ThenTheyDiffer)
{
  std::error_code const generic
      = std::make_error_code (std::errc::invalid_argument);
  EXPECT_FALSE (generic == framing_errc::frame_too_large);
}
