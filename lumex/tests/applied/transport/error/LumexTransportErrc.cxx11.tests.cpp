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
                                       transport_errc::permission_denied };

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
