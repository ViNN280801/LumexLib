// The error codes of the protocol layer (lumex/core/protocol/error): the
// category, the message of every value and the conversion to std::error_code
// and std::error_condition.

#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

namespace
{
using lumex::core::protocol::error::protocol_category;
using lumex::core::protocol::error::protocol_errc;

protocol_errc const kEveryValue[]
    = { protocol_errc::truncated,      protocol_errc::trailing_bytes,
        protocol_errc::invalid_value,  protocol_errc::unknown_frame,
        protocol_errc::limit_exceeded, protocol_errc::unknown_case,
        protocol_errc::access_denied,  protocol_errc::not_supported,
        protocol_errc::duplicate_key,  protocol_errc::out_of_limits };
} // namespace

TEST (LumexProtocolErrcTest, GivenTheCategory_WhenNamed_ThenItIsLumexProtocol)
{
  EXPECT_STREQ (protocol_category ().name (), "lumex.protocol");
}

TEST (LumexProtocolErrcTest,
      GivenTheCategory_WhenAskedTwice_ThenItIsTheSameObject)
{
  EXPECT_EQ (&protocol_category (), &protocol_category ());
}

TEST (LumexProtocolErrcTest,
      GivenEveryValue_WhenConverted_ThenTheCodeHasTheValueAndTheCategory)
{
  for (protocol_errc value : kEveryValue)
    {
      std::error_code const code = value;
      EXPECT_EQ (code.value (), static_cast<int> (value));
      EXPECT_EQ (&code.category (), &protocol_category ());
      EXPECT_TRUE (code == value);
      EXPECT_TRUE (static_cast<bool> (code));
    }
}

TEST (LumexProtocolErrcTest,
      GivenTheValues_WhenNumbered_ThenTheyStartAtOneInDeclarationOrder)
{
  EXPECT_EQ (static_cast<int> (protocol_errc::truncated), 1);
  EXPECT_EQ (static_cast<int> (protocol_errc::trailing_bytes), 2);
  EXPECT_EQ (static_cast<int> (protocol_errc::invalid_value), 3);
  EXPECT_EQ (static_cast<int> (protocol_errc::unknown_frame), 4);
  EXPECT_EQ (static_cast<int> (protocol_errc::limit_exceeded), 5);
  EXPECT_EQ (static_cast<int> (protocol_errc::unknown_case), 6);
  EXPECT_EQ (static_cast<int> (protocol_errc::access_denied), 7);
  EXPECT_EQ (static_cast<int> (protocol_errc::not_supported), 8);
  EXPECT_EQ (static_cast<int> (protocol_errc::duplicate_key), 9);
  EXPECT_EQ (static_cast<int> (protocol_errc::out_of_limits), 10);
}

// The text of every code is part of the log lines and the reports a user
// reads (REVIEW41 A11: a changed text went unnoticed).
TEST (LumexProtocolErrcTest,
      GivenEveryValue_WhenMessaged_ThenTheTextIsTheDocumentedOne)
{
  EXPECT_EQ (std::error_code (protocol_errc::truncated).message (),
             "message shorter than its fields");
  EXPECT_EQ (std::error_code (protocol_errc::trailing_bytes).message (),
             "bytes after the last field of a message");
  EXPECT_EQ (std::error_code (protocol_errc::invalid_value).message (),
             "field value the field type cannot represent");
  EXPECT_EQ (std::error_code (protocol_errc::unknown_frame).message (),
             "frame matches no row of the protocol");
  EXPECT_EQ (std::error_code (protocol_errc::limit_exceeded).message (),
             "count larger than the capacity of its field");
  EXPECT_EQ (std::error_code (protocol_errc::unknown_case).message (),
             "discriminant matches no case of the union");
  EXPECT_EQ (std::error_code (protocol_errc::access_denied).message (),
             "the registry entry does not allow this access");
  EXPECT_EQ (std::error_code (protocol_errc::not_supported).message (),
             "not available in the firmware or model of the session");
  EXPECT_EQ (std::error_code (protocol_errc::duplicate_key).message (),
             "two entries of a table have the same key");
  EXPECT_EQ (std::error_code (protocol_errc::out_of_limits).message (),
             "value outside the limits of the registry entry");
}

TEST (LumexProtocolErrcTest,
      GivenEveryValue_WhenMessaged_ThenTheTextIsDistinct)
{
  for (protocol_errc first : kEveryValue)
    {
      std::string const text = std::error_code (first).message ();
      EXPECT_FALSE (text.empty ());
      EXPECT_NE (text, "unknown protocol error");
      for (protocol_errc second : kEveryValue)
        {
          if (first != second)
            {
              EXPECT_NE (text, std::error_code (second).message ());
            }
        }
    }
}

TEST (LumexProtocolErrcTest,
      GivenAValueOutsideTheEnum_WhenMessaged_ThenTheTextSaysUnknown)
{
  EXPECT_EQ (protocol_category ().message (0), "unknown protocol error");
  EXPECT_EQ (protocol_category ().message (1000), "unknown protocol error");
}

TEST (LumexProtocolErrcTest,
      GivenAValue_WhenMadeACondition_ThenItMatchesTheCode)
{
  std::error_condition const condition
      = make_error_condition (protocol_errc::invalid_value);
  EXPECT_EQ (&condition.category (), &protocol_category ());
  EXPECT_TRUE (std::error_code (protocol_errc::invalid_value) == condition);
}

TEST (LumexProtocolErrcTest,
      GivenACodeOfAnotherCategory_WhenComparedWithAValue_ThenTheyDiffer)
{
  std::error_code const generic
      = std::make_error_code (std::errc::invalid_argument);
  EXPECT_FALSE (generic == protocol_errc::truncated);
}
