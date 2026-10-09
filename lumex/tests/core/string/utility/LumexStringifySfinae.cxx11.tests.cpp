// LumexStringifySfinae.cxx11.tests.cpp
//
// stringify is constrained with std::enable_if in every standard: an argument
// list with a type that cannot be streamed finds no overload, so a call is
// detected with SFINAE and never stops in a static_assert of the body. The
// detector below asks the compiler instead of compiling a rejected call.
// The accepted lists keep their text (LumexString.cxx11.tests.cpp has the
// bulk of those tests). The C++20 file ties the constraint to the concept
// AllStringifiable.
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/string/LumexString"

#include "lumex/tests/core/string/LumexStringTestFixtures.hpp"
#include "lumex/tests/support/LumexOstreamProbe.hpp"

namespace utility_traits = lumex::core::utility::traits;
using lumex::core::string::utility::stringify;
using lumex_tests_support::ostream_accepts;
using lumex_tests_support::streamed;

namespace
{
// stringify (declval<Args> ()...) is well-formed.
template <typename Void, typename... Args>
struct can_stringify_impl : std::false_type
{
};

template <typename... Args>
struct can_stringify_impl<utility_traits::meta::void_t<decltype (stringify (
                              std::declval<Args> ()...))>,
                          Args...> : std::true_type
{
};

template <typename... Args>
struct can_stringify : can_stringify_impl<void, Args...>
{
};

struct adl_streamable
{
  int value;
};

std::ostream &
operator<< (std::ostream &stream, adl_streamable const &object)
{
  return stream << "adl(" << object.value << ")";
}

// Writes its tag to a log when it is streamed, to see the order of the pack.
struct logging_streamable_t
{
  char tag;
};

std::string &
stream_log ()
{
  static std::string log;
  return log;
}

std::ostream &
operator<< (std::ostream &stream, logging_streamable_t const &item)
{
  stream_log () += item.tag;
  return stream << item.tag;
}

enum class scoped_t
{
  one = 1
};

enum plain_t
{
  plain_one = 1
};

// Two overloads of a user function that choose by what stringify accepts: a
// hard error in the body of stringify would stop the build instead.
template <typename T>
auto
describe_impl (T const &value, int) -> decltype (stringify (value, '|'))
{
  return stringify (value, '|');
}

template <typename T>
std::string
describe_impl (T const &, long)
{
  return "?";
}

template <typename T>
std::string
describe (T const &value)
{
  return describe_impl (value, 0);
}
} // namespace

// --- the text of the accepted lists is unchanged ---------------------------

TEST (LumexStringifySfinaeTest, GivenAcceptedArguments_WhenCalled_ThenTheText)
{
  EXPECT_EQ (stringify (), "");
  EXPECT_EQ (stringify (42), "42");
  EXPECT_EQ (stringify ("channel=", 2, " flow=", 1.5), "channel=2 flow=1.5");
  EXPECT_EQ (stringify (std::string ("a"), 'b', "c", 1.25F, 7U, 8L, 9ULL),
             "abc1.25789");
  EXPECT_EQ (stringify (CustomStreamable (5), adl_streamable{ 6 }),
             "CustomStreamable(5)adl(6)");
  EXPECT_EQ (stringify (plain_one), "1");
}

TEST (LumexStringifySfinaeTest, GivenExplicitEmptyPack_WhenCalled_ThenEmpty)
{
  // stringify<> () cannot take the non-template overload: the template with
  // an empty pack is the only candidate and has to be viable and empty.
  EXPECT_EQ (stringify<> (), "");
}

TEST (LumexStringifySfinaeTest, GivenNoArguments_WhenCalled_ThenNoexcept)
{
  // The overload without arguments does not make a stream.
  static_assert (noexcept (stringify ()), "stringify () is noexcept");
  static_assert (!noexcept (stringify (1)), "stringify (1) can throw");
  SUCCEED ();
}

TEST (LumexStringifySfinaeTest, GivenArguments_WhenDecltype_ThenAString)
{
  static_assert (
      std::is_same<decltype (stringify (1, "a", 2.0)), std::string>::value,
      "");
  static_assert (std::is_same<decltype (stringify ()), std::string>::value,
                 "");
  SUCCEED ();
}

TEST (LumexStringifySfinaeTest,
      GivenOrderOfArguments_WhenCalled_ThenStreamedLeftToRight)
{
  // The pack is expanded left to right, whatever the form of the expansion.
  stream_log ().clear ();
  EXPECT_EQ (stringify (logging_streamable_t{ 'a' },
                        logging_streamable_t{ 'b' },
                        logging_streamable_t{ 'c' }, 'd'),
             "abcd");
  EXPECT_EQ (stream_log (), "abc");
  EXPECT_EQ (stringify (1, 2, 3, 4, 5, 6, 7, 8, 9, 0), "1234567890");
}

// --- the detection
// ------------------------------------------------------------

TEST (LumexStringifySfinaeTest,
      GivenStreamableArguments_WhenDetecting_ThenExists)
{
  EXPECT_TRUE ((can_stringify<>::value));
  EXPECT_TRUE ((can_stringify<int>::value));
  EXPECT_TRUE ((can_stringify<int &>::value));
  EXPECT_TRUE ((can_stringify<int const &>::value));
  EXPECT_TRUE ((can_stringify<int &&>::value));
  EXPECT_TRUE ((can_stringify<bool, char, signed char, unsigned char>::value));
  EXPECT_TRUE (
      (can_stringify<short, unsigned short, long, unsigned long>::value));
  EXPECT_TRUE ((can_stringify<long long, unsigned long long>::value));
  EXPECT_TRUE ((can_stringify<float, double, long double>::value));
  EXPECT_TRUE ((can_stringify<std::string>::value));
  EXPECT_TRUE ((can_stringify<std::string const &>::value));
  EXPECT_TRUE ((can_stringify<char const (&)[4]>::value));
  EXPECT_TRUE ((can_stringify<char const *>::value));
  EXPECT_TRUE ((can_stringify<char *>::value));
  EXPECT_TRUE ((can_stringify<void *>::value));
  EXPECT_TRUE ((can_stringify<int *>::value));
  EXPECT_TRUE ((can_stringify<NonStreamable *>::value));
  EXPECT_TRUE ((can_stringify<plain_t>::value));
  EXPECT_TRUE ((can_stringify<CustomStreamable>::value));
  EXPECT_TRUE ((can_stringify<adl_streamable>::value));
  EXPECT_TRUE (
      (can_stringify<int, std::string, CustomStreamable, double>::value));
  EXPECT_TRUE ((can_stringify<std::unique_ptr<int>>::value));
  EXPECT_TRUE ((can_stringify<std::unique_ptr<int> const &>::value));
  EXPECT_TRUE ((can_stringify<std::unique_ptr<int[]>>::value));
  EXPECT_TRUE ((can_stringify<std::shared_ptr<double>>::value));
  EXPECT_TRUE ((can_stringify<std::shared_ptr<NonStreamable> &>::value));
}

TEST (LumexStringifySfinaeTest,
      GivenAnArgumentThatCannotBeStreamed_WhenDetecting_ThenNoOverload)
{
  EXPECT_FALSE ((can_stringify<NonStreamable>::value));
  EXPECT_FALSE ((can_stringify<NonStreamable &>::value));
  EXPECT_FALSE ((can_stringify<NonStreamable const &>::value));
  EXPECT_FALSE ((can_stringify<NonStreamable &&>::value));
  EXPECT_FALSE ((can_stringify<int, NonStreamable>::value));
  EXPECT_FALSE ((can_stringify<NonStreamable, int>::value));
  EXPECT_FALSE (
      (can_stringify<int, std::string, NonStreamable, double>::value));
  EXPECT_FALSE ((can_stringify<NonStreamable, NonStreamable>::value));
  EXPECT_FALSE ((can_stringify<std::vector<int>>::value));
  EXPECT_FALSE ((can_stringify<std::vector<int> &>::value));
  EXPECT_FALSE ((can_stringify<std::map<int, int>>::value));
  EXPECT_FALSE ((can_stringify<std::pair<int, int>>::value));
  EXPECT_FALSE ((can_stringify<scoped_t>::value));
  EXPECT_FALSE ((can_stringify<std::weak_ptr<int>>::value));
  EXPECT_FALSE ((can_stringify<std::function<void ()>>::value));
}

TEST (LumexStringifySfinaeTest, GivenWideStrings_WhenDetecting_ThenNoOverload)
{
  // A string of wide characters is never written to a narrow stream.
  EXPECT_FALSE ((can_stringify<std::wstring>::value));
  EXPECT_FALSE ((can_stringify<std::u16string>::value));
  EXPECT_FALSE ((can_stringify<std::u32string>::value));
  EXPECT_FALSE ((can_stringify<int, std::wstring>::value));
}

TEST (LumexStringifySfinaeTest,
      GivenUserOverloads_WhenChoosing_ThenStreamabilityDecides)
{
  EXPECT_EQ (describe (5), "5|");
  EXPECT_EQ (describe (std::string ("s")), "s|");
  EXPECT_EQ (describe (CustomStreamable (1)), "CustomStreamable(1)|");
  EXPECT_EQ (describe (NonStreamable (1)), "?");
  EXPECT_EQ (describe (std::vector<int>{ 1 }), "?");
  EXPECT_EQ (describe (scoped_t::one), "?");
}

// --- the character types follow the standard library of the build ----------

TEST (LumexStringifySfinaeTest,
      GivenCharacterTypes_WhenDetecting_ThenWhatTheStreamAccepts)
{
  // Up to C++17 these stream as numbers (or addresses); from C++20 the library
  // deletes the operators of a narrow stream, and the call has no overload.
  EXPECT_EQ ((can_stringify<wchar_t>::value),
             (ostream_accepts<wchar_t>::value));
  EXPECT_EQ ((can_stringify<char16_t>::value),
             (ostream_accepts<char16_t>::value));
  EXPECT_EQ ((can_stringify<char32_t>::value),
             (ostream_accepts<char32_t>::value));
  EXPECT_EQ ((can_stringify<wchar_t const *>::value),
             (ostream_accepts<wchar_t const *>::value));
  EXPECT_EQ ((can_stringify<wchar_t const (&)[3]>::value),
             (ostream_accepts<wchar_t const (&)[3]>::value));
  EXPECT_EQ ((can_stringify<char16_t const *>::value),
             (ostream_accepts<char16_t const *>::value));
  EXPECT_EQ ((can_stringify<char32_t const *>::value),
             (ostream_accepts<char32_t const *>::value));
  EXPECT_EQ ((can_stringify<int, wchar_t>::value),
             (ostream_accepts<wchar_t>::value));
  EXPECT_EQ ((can_stringify<wchar_t, int>::value),
             (ostream_accepts<wchar_t>::value));
#if defined(__cpp_char8_t)
  EXPECT_EQ ((can_stringify<char8_t>::value),
             (ostream_accepts<char8_t>::value));
#endif
}

#if __cplusplus < 202002L
TEST (LumexStringifySfinaeTest, GivenCxx11To17_WhenWideCharacter_ThenItsNumber)
{
  EXPECT_TRUE ((can_stringify<wchar_t>::value));
  EXPECT_TRUE ((can_stringify<char16_t>::value));
  EXPECT_TRUE ((can_stringify<char32_t>::value));
  EXPECT_EQ (stringify (L'A'), "65");
  EXPECT_EQ (stringify (u'A'), "65");
  EXPECT_EQ (stringify (U'A'), "65");
  EXPECT_EQ (stringify ("[", L'A', "]"), "[65]");
  EXPECT_EQ (stringify (L'A'), streamed (L'A'));
}
#endif

#if LUMEX_TEST_OSTREAM_DELETES_WIDE_CHARACTERS
TEST (LumexStringifySfinaeTest,
      GivenCxx20Library_WhenWideCharacter_ThenNoOverload)
{
  EXPECT_FALSE ((can_stringify<wchar_t>::value));
  EXPECT_FALSE ((can_stringify<char16_t>::value));
  EXPECT_FALSE ((can_stringify<char32_t>::value));
  EXPECT_FALSE ((can_stringify<wchar_t const *>::value));
  EXPECT_FALSE ((can_stringify<int, wchar_t, int>::value));
#if defined(__cpp_char8_t)
  EXPECT_FALSE ((can_stringify<char8_t>::value));
#endif
  // The narrow character types keep their overload.
  EXPECT_TRUE ((can_stringify<char, signed char, unsigned char>::value));
}
#endif
