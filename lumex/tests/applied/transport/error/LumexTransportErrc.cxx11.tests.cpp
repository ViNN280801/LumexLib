// The error codes of the transports (lumex/applied/transport/error): the
// category, a message per value, is_transport_error for a code of this binary
// and for one of another binary (a category of the same name), and the
// translation of Asio's own codes.

#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexAsioErrors.hpp"

namespace
{
using lumex::applied::transport::error::is_transport_error;
using lumex::applied::transport::error::transport_category;
using lumex::applied::transport::error::transport_errc;

transport_errc const kEveryValue[] = { transport_errc::timeout,
                                       transport_errc::cancelled,
                                       transport_errc::closed,
                                       transport_errc::not_open,
                                       transport_errc::disconnected,
                                       transport_errc::would_deadlock,
                                       transport_errc::invalid_endpoint,
                                       transport_errc::report_too_large,
                                       transport_errc::bad_report_descriptor,
                                       transport_errc::permission_denied,
                                       transport_errc::unknown_report_id };

// A category of the same name, as another binary that links the static
// transport library has it.
class other_binary_category final : public std::error_category
{
public:
  char const *
  name () const noexcept override
  {
    return "lumex.transport";
  }
  std::string
  message (int) const override
  {
    return "other binary";
  }
};

class unrelated_category final : public std::error_category
{
public:
  char const *
  name () const noexcept override
  {
    return "unrelated";
  }
  std::string
  message (int) const override
  {
    return "unrelated";
  }
};
} // namespace

TEST (LumexTransportErrcTest,
      GivenTheCategory_WhenNamed_ThenItIsLumexTransport)
{
  EXPECT_STREQ (transport_category ().name (), "lumex.transport");
  EXPECT_EQ (&transport_category (), &transport_category ());
}

TEST (LumexTransportErrcTest,
      GivenEveryValue_WhenMessaged_ThenTheTextIsDistinct)
{
  for (transport_errc first : kEveryValue)
    {
      std::string const text = std::error_code (first).message ();
      EXPECT_NE (text, "unknown transport error");
      for (transport_errc second : kEveryValue)
        if (first != second)
          EXPECT_NE (text, std::error_code (second).message ());
    }
  EXPECT_EQ (transport_category ().message (0), "unknown transport error");
}

TEST (LumexTransportErrcTest, GivenACodeOfThisBinary_WhenChecked_ThenItMatches)
{
  std::error_code const code = transport_errc::timeout;
  EXPECT_TRUE (code == transport_errc::timeout);
  EXPECT_TRUE (is_transport_error (code, transport_errc::timeout));
  EXPECT_FALSE (is_transport_error (code, transport_errc::cancelled));
}

TEST (LumexTransportErrcTest,
      GivenPermissionDenied_WhenComparedWithTheStandardCondition_ThenItMatches)
{
  std::error_code const code = transport_errc::permission_denied;
  EXPECT_TRUE (code == std::errc::permission_denied);
  EXPECT_TRUE (std::errc::permission_denied == code);
  EXPECT_FALSE (code == std::errc::timed_out);
  EXPECT_TRUE (transport_category ().default_error_condition (
                   static_cast<int> (transport_errc::permission_denied))
               == std::errc::permission_denied);
}

TEST (
    LumexTransportErrcTest,
    GivenTimeoutAndCancelled_WhenComparedWithTheStandardConditions_ThenTheyMatch)
{
  std::error_code const timeout = transport_errc::timeout;
  std::error_code const cancelled = transport_errc::cancelled;
  EXPECT_TRUE (timeout == std::errc::timed_out);
  EXPECT_FALSE (timeout == std::errc::operation_canceled);
  EXPECT_TRUE (cancelled == std::errc::operation_canceled);
  EXPECT_FALSE (cancelled == std::errc::timed_out);
}

TEST (
    LumexTransportErrcTest,
    GivenValuesWithoutAPortableMeaning_WhenCompared_ThenNoStandardConditionMatches)
{
  std::errc const standard[]
      = { std::errc::permission_denied,  std::errc::timed_out,
          std::errc::operation_canceled, std::errc::not_connected,
          std::errc::connection_reset,   std::errc::bad_file_descriptor,
          std::errc::invalid_argument };
  transport_errc const unmapped[] = { transport_errc::closed,
                                      transport_errc::not_open,
                                      transport_errc::disconnected,
                                      transport_errc::would_deadlock,
                                      transport_errc::invalid_endpoint,
                                      transport_errc::report_too_large,
                                      transport_errc::bad_report_descriptor };
  for (transport_errc value : unmapped)
    for (std::errc condition : standard)
      EXPECT_FALSE (std::error_code (value) == condition)
          << std::error_code (value).message ();
}

TEST (LumexTransportErrcTest,
      GivenACode_WhenComparedWithTheConditionOfItsOwnValue_ThenOnlyThatMatches)
{
  for (transport_errc first : kEveryValue)
    for (transport_errc second : kEveryValue)
      {
        std::error_code const code = first;
        std::error_condition const condition = make_error_condition (second);
        EXPECT_EQ (code == condition, first == second);
        EXPECT_EQ (condition == code, first == second);
      }
}

TEST (LumexTransportErrcTest,
      GivenACodeOfAnotherBinary_WhenChecked_ThenOnlyTheHelperMatches)
{
  static other_binary_category const other;
  std::error_code const code (static_cast<int> (transport_errc::closed),
                              other);
  EXPECT_FALSE (code == transport_errc::closed);
  EXPECT_TRUE (is_transport_error (code, transport_errc::closed));
  EXPECT_FALSE (is_transport_error (code, transport_errc::timeout));
}

TEST (LumexTransportErrcTest,
      GivenACodeOfAnUnrelatedCategory_WhenChecked_ThenItDoesNotMatch)
{
  static unrelated_category const unrelated;
  std::error_code const code (static_cast<int> (transport_errc::closed),
                              unrelated);
  EXPECT_FALSE (is_transport_error (code, transport_errc::closed));
}

TEST (LumexTransportErrcTest,
      GivenAsioCodes_WhenTranslated_ThenTheyBecomeTransportCodes)
{
  using lumex::applied::transport::detail::from_asio;
  EXPECT_FALSE (from_asio (std::error_code ()));
  EXPECT_TRUE (from_asio (asio::error::operation_aborted)
               == transport_errc::cancelled);
  EXPECT_TRUE (from_asio (asio::error::eof) == transport_errc::disconnected);
  EXPECT_TRUE (from_asio (asio::error::host_not_found)
               == transport_errc::invalid_endpoint);
  // A system error keeps its value and leaves Asio's own system category.
  std::error_code const refused = asio::error::connection_refused;
  std::error_code const translated = from_asio (refused);
  EXPECT_EQ (translated.value (), refused.value ());
  // Compared with ==, not by address: the MSVC STL gives a test built at
  // C++20 another system_category object than the library built at C++14,
  // and its operator== compares the category identity.
  EXPECT_TRUE (translated.category () == std::system_category ());
}
