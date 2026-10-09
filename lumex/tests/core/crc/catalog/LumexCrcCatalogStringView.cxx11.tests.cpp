// CRC catalogue tests of the string overloads (every standard the module
// builds at): compute_crc_catalog and compute_crc_with_rev_eng_params take a
// text_view_t, which is the lumex_string_view of lumex::string_view in every
// standard (never std::string_view), so a program passes a literal, a
// char const *, a std::string and a lumex_string_view the same way at C++11
// and at C++23; a std::string_view converts to it (the C++17 file). The
// C++17 and C++20 suites compile this file too, so the overload set stays
// unambiguous next to the vector, pointer and span overloads
// (LumexCrcCatalog.cxx17.tests.cpp holds the tests that name
// std::string_view). It also tests
// append_crc_least_significant_byte_first, which exists in every standard.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/crc/LumexCrc"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/string_view/view/LumexWStringView.hpp"

using lumex::core::span::view::span;
using namespace lumex::core::crc::catalog;
using namespace lumex::core::crc::parametric;

namespace
{
// The RevEng check message: every catalogue entry documents its CRC of it.
char const kCheckMessage[] = "123456789";
std::size_t const kCheckLength = 9;

template <typename Spec>
crc_params_t
params_of ()
{
  crc_params_t params{};
  params.widthBits = Spec::kWidth;
  params.poly = Spec::kPoly;
  params.init = Spec::kInit;
  params.refIn = Spec::kRefIn;
  params.refOut = Spec::kRefOut;
  params.xorOut = Spec::kXorOut;
  return params;
}

std::uint8_t const *
bytes_of (std::string const &text)
{
  return reinterpret_cast<std::uint8_t const *> (text.data ());
}

std::string
payload_of_length (std::size_t length)
{
  std::string text;
  for (std::size_t i = 0; i < length; ++i)
    text.push_back (static_cast<char> ((i * 37 + 11) & 0xFF));
  return text;
}
} // namespace

static_assert (
    std::is_same<text_view_t,
                 lumex::core::string_view::view::lumex_string_view>::value,
    "the string overloads take the lumex_string_view in every standard");

namespace
{
// Taking the address picks the one overload whose parameters are exactly
// these: the lines do not compile when the text parameter is another type
// (std::string_view, std::string const &) in any standard.
std::uint64_t (*const kCatalogText) (std::uint32_t, lumex_string_view)
    = &compute_crc_catalog;
std::uint64_t (*const kRevEngText) (crc_params_t const &, lumex_string_view)
    = &compute_crc_with_rev_eng_params;

template <class T, class = void> struct catalog_accepts : std::false_type
{
};
template <class T>
struct catalog_accepts<T, decltype (void (compute_crc_catalog (
                              0u, std::declval<T> ())))> : std::true_type
{
};
template <class T, class = void> struct rev_eng_accepts : std::false_type
{
};
template <class T>
struct rev_eng_accepts<T, decltype (void (compute_crc_with_rev_eng_params (
                              std::declval<crc_params_t const &> (),
                              std::declval<T> ())))> : std::true_type
{
};
} // namespace

// Every argument form is accepted without ambiguity next to the vector, span
// and pointer overloads, in every standard.
static_assert (catalog_accepts<char const (&)[4]>::value, "a literal");
static_assert (catalog_accepts<char (&)[4]>::value, "a char array");
static_assert (catalog_accepts<char const *>::value, "a char const *");
static_assert (catalog_accepts<char *>::value, "a char *");
static_assert (catalog_accepts<std::string>::value, "a std::string");
static_assert (catalog_accepts<std::string const &>::value, "a std::string &");
static_assert (catalog_accepts<lumex_string_view>::value, "the view");
static_assert (catalog_accepts<lumex_string_view const &>::value,
               "the view &");
static_assert (catalog_accepts<std::nullptr_t>::value, "nullptr: empty text");
static_assert (!catalog_accepts<int>::value, "a number is not text");
static_assert (!catalog_accepts<wchar_t const *>::value, "a wide text");
static_assert (!catalog_accepts<lumex_wstring_view>::value, "a wide view");
static_assert (rev_eng_accepts<char const (&)[4]>::value, "a literal");
static_assert (rev_eng_accepts<char const *>::value, "a char const *");
static_assert (rev_eng_accepts<std::string>::value, "a std::string");
static_assert (rev_eng_accepts<lumex_string_view>::value, "the view");
static_assert (rev_eng_accepts<std::nullptr_t>::value, "nullptr: empty text");
static_assert (!rev_eng_accepts<int>::value, "a number is not text");
static_assert (!rev_eng_accepts<lumex_wstring_view>::value, "a wide view");

TEST (CrcCatalogStringView,
      GivenTextParameter_WhenCallThroughPointer_ThenCheck)
{
  EXPECT_EQ (kCatalogText (0, "123456789"),
             static_cast<std::uint64_t> (crc3_gsm_spec_t::kCatalogCheck));
  EXPECT_EQ (kCatalogText (0, lumex_string_view ("123456789!", 9)),
             kCatalogText (0, std::string ("123456789")));
  EXPECT_EQ (kRevEngText (params_of<crc16_modbus_spec_t> (), "123456789"),
             static_cast<std::uint64_t> (crc16_modbus_spec_t::kCatalogCheck));
  EXPECT_EQ (kRevEngText (params_of<crc16_modbus_spec_t> (), nullptr), 0U);
  EXPECT_EQ (kCatalogText (0, nullptr), 0U);
}

TEST (CrcCatalogStringView, GivenStringLiteral_WhenCatalogCrc_ThenCheckValue)
{
  EXPECT_EQ (compute_crc_catalog (0, "123456789"),
             static_cast<std::uint64_t> (crc3_gsm_spec_t::kCatalogCheck));
  EXPECT_EQ (compute_crc_catalog (0, kCheckMessage),
             static_cast<std::uint64_t> (crc3_gsm_spec_t::kCatalogCheck));
  EXPECT_EQ (
      compute_crc_catalog (0, static_cast<char const *> (kCheckMessage)),
      static_cast<std::uint64_t> (crc3_gsm_spec_t::kCatalogCheck));
}

TEST (CrcCatalogStringView,
      GivenEveryCatalogIndex_WhenStringForms_ThenSameAsThePointerForm)
{
  std::string const text = payload_of_length (300);
  std::vector<std::uint8_t> const as_vector (text.begin (), text.end ());
  std::uint32_t const count = get_crc_catalog_entry_count ();
  ASSERT_GT (count, 100U);
  for (std::uint32_t entry = 0; entry < count; ++entry)
    {
      std::uint64_t const expected
          = compute_crc_catalog (entry, bytes_of (text), text.size ());
      EXPECT_EQ (compute_crc_catalog (entry, text), expected)
          << "std::string, catalog index " << entry;
      EXPECT_EQ (compute_crc_catalog (
                     entry, text_view_t (text.data (), text.size ())),
                 expected)
          << "sized view, catalog index " << entry;
      EXPECT_EQ (compute_crc_catalog (entry, as_vector), expected)
          << "vector, catalog index " << entry;
      EXPECT_EQ (
          compute_crc_catalog (entry, text.c_str ()),
          compute_crc_catalog (entry, bytes_of (text),
                               std::char_traits<char>::length (text.c_str ())))
          << "char const *, catalog index " << entry;
      // A sub-range of the text.
      EXPECT_EQ (
          compute_crc_catalog (entry, text_view_t (text.data () + 10, 100)),
          compute_crc_catalog (entry, bytes_of (text) + 10, 100))
          << "sub-range, catalog index " << entry;
    }
}

TEST (CrcCatalogStringView,
      GivenEmbeddedZeros_WhenStringForm_ThenAllBytesCount)
{
  // The std::string is sized: its NUL characters are bytes, not the end.
  std::string const text ("a\0b\0", 4);
  for (std::uint32_t entry = 0; entry < get_crc_catalog_entry_count ();
       ++entry)
    {
      EXPECT_EQ (compute_crc_catalog (entry, text),
                 compute_crc_catalog (entry, bytes_of (text), 4))
          << "catalog index " << entry;
    }
  // A char const * is NUL-terminated: "a\0b" is the one byte "a".
  EXPECT_EQ (compute_crc_catalog (get_crc_catalog_entry_count () - 1, "a\0b"),
             compute_crc_catalog (get_crc_catalog_entry_count () - 1,
                                  bytes_of (text), 1));
  crc_params_t const params = params_of<crc16_modbus_spec_t> ();
  EXPECT_EQ (compute_crc_with_rev_eng_params (params, text),
             compute_crc_with_rev_eng_params (params, bytes_of (text), 4));
  EXPECT_NE (compute_crc_with_rev_eng_params (params, text),
             compute_crc_with_rev_eng_params (params, "a\0b\0"));
}

TEST (CrcCatalogStringView, GivenEmptyOrUnfound_WhenCatalogCrc_ThenZero)
{
  std::uint32_t const count = get_crc_catalog_entry_count ();
  for (std::uint32_t entry = 0; entry < count; ++entry)
    {
      EXPECT_EQ (compute_crc_catalog (entry, ""), 0U)
          << "catalog index " << entry;
      EXPECT_EQ (compute_crc_catalog (entry, std::string ()), 0U)
          << "catalog index " << entry;
      EXPECT_EQ (compute_crc_catalog (entry, text_view_t ()), 0U)
          << "catalog index " << entry;
      EXPECT_EQ (compute_crc_catalog (entry, text_view_t (kCheckMessage, 0)),
                 0U)
          << "catalog index " << entry;
    }
  EXPECT_EQ (compute_crc_catalog (count, kCheckMessage), 0U);
  EXPECT_EQ (compute_crc_catalog (count, std::string (kCheckMessage)), 0U);
}

TEST (CrcCatalogStringView, GivenRevEngParams_WhenStringForms_ThenCheckValue)
{
  crc_params_t const maxim = params_of<crc8_maxim_dow_spec_t> ();
  std::uint64_t const maxim_check
      = static_cast<std::uint64_t> (crc8_maxim_dow_spec_t::kCatalogCheck);
  EXPECT_EQ (compute_crc_with_rev_eng_params (maxim, "123456789"),
             maxim_check);
  EXPECT_EQ (
      compute_crc_with_rev_eng_params (maxim, std::string (kCheckMessage)),
      maxim_check);
  EXPECT_EQ (compute_crc_with_rev_eng_params (
                 maxim, text_view_t (kCheckMessage, kCheckLength)),
             maxim_check);
  EXPECT_EQ (compute_crc_with_rev_eng_params (
                 maxim, static_cast<char const *> (kCheckMessage)),
             maxim_check);
  // A shorter view is another message.
  EXPECT_NE (
      compute_crc_with_rev_eng_params (maxim, text_view_t (kCheckMessage, 8)),
      maxim_check);
  EXPECT_EQ (
      compute_crc_with_rev_eng_params (maxim, text_view_t (kCheckMessage, 8)),
      compute_crc_with_rev_eng_params (
          maxim, reinterpret_cast<std::uint8_t const *> (kCheckMessage), 8));

  crc_params_t const modbus = params_of<crc16_modbus_spec_t> ();
  EXPECT_EQ (compute_crc_with_rev_eng_params (modbus, "123456789"),
             static_cast<std::uint64_t> (crc16_modbus_spec_t::kCatalogCheck));

  EXPECT_EQ (compute_crc_with_rev_eng_params (modbus, ""), 0U);
  EXPECT_EQ (compute_crc_with_rev_eng_params (modbus, std::string ()), 0U);
  EXPECT_EQ (compute_crc_with_rev_eng_params (modbus, text_view_t ()), 0U);
  // Invalid parameters give zero, whatever the text.
  crc_params_t invalid = modbus;
  invalid.widthBits = 0;
  EXPECT_EQ (compute_crc_with_rev_eng_params (invalid, "123456789"), 0U);
}

TEST (
    CrcCatalogStringView,
    GivenOverloads_WhenCallWithLiteralStringVectorSpanPointer_ThenNoAmbiguity)
{
  // Each argument kind picks its overload; none of the calls is ambiguous and
  // all give the CRC of the same nine bytes.
  std::uint64_t const expected = compute_crc_catalog (
      0, reinterpret_cast<std::uint8_t const *> (kCheckMessage), kCheckLength);
  std::string const text = kCheckMessage;
  std::vector<std::uint8_t> const bytes (text.begin (), text.end ());
  std::array<std::uint8_t, 9> const fixed
      = { { '1', '2', '3', '4', '5', '6', '7', '8', '9' } };
  std::uint8_t const raw[9] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  char mutable_literal[] = "123456789";
  EXPECT_EQ (compute_crc_catalog (0, "123456789"), expected);
  EXPECT_EQ (compute_crc_catalog (0, mutable_literal), expected);
  EXPECT_EQ (compute_crc_catalog (0, text), expected);
  EXPECT_EQ (compute_crc_catalog (0, bytes), expected);
  EXPECT_EQ (compute_crc_catalog (0, fixed), expected);
  EXPECT_EQ (compute_crc_catalog (0, raw), expected);
  EXPECT_EQ (compute_crc_catalog (0, span<std::uint8_t const> (bytes)),
             expected);
  EXPECT_EQ (compute_crc_catalog (0, bytes.data (), bytes.size ()), expected);
  EXPECT_EQ (compute_crc_catalog (0, nullptr, 0), 0U);
  static_assert (
      !std::is_convertible<char const (&)[4], span<std::uint8_t const>>::value,
      "a literal is text, not an array of bytes");
  static_assert (
      !std::is_convertible<std::string &, span<std::uint8_t const>>::value,
      "a std::string is text, not a range of bytes");
  static_assert (std::is_convertible<char const (&)[4], text_view_t>::value,
                 "a literal is accepted as text");
  static_assert (std::is_convertible<std::string const &, text_view_t>::value,
                 "a std::string is accepted as text");
  static_assert (!std::is_convertible<std::vector<std::uint8_t> const &,
                                      text_view_t>::value,
                 "bytes are not text");
}

TEST (CrcCatalog, AppendCrcLeastSignificantByteFirst_AppendsTheCheckBytes)
{
  std::vector<std::uint8_t> frame8 (kCheckMessage,
                                    kCheckMessage + kCheckLength);
  append_crc_least_significant_byte_first (params_of<crc8_maxim_dow_spec_t> (),
                                           frame8);
  ASSERT_EQ (frame8.size (), kCheckLength + 1U);
  EXPECT_EQ (frame8.back (), crc8_maxim_dow_spec_t::kCatalogCheck);

  std::vector<std::uint8_t> frame16 (kCheckMessage,
                                     kCheckMessage + kCheckLength);
  append_crc_least_significant_byte_first (params_of<crc16_modbus_spec_t> (),
                                           frame16);
  ASSERT_EQ (frame16.size (), kCheckLength + 2U);
  EXPECT_EQ (
      frame16[kCheckLength],
      static_cast<std::uint8_t> (crc16_modbus_spec_t::kCatalogCheck & 0xFFU));
  EXPECT_EQ (
      frame16[kCheckLength + 1U],
      static_cast<std::uint8_t> (crc16_modbus_spec_t::kCatalogCheck >> 8));
}

TEST (CrcCatalogStringView,
      GivenParamsOfEveryWidth_WhenAppendCrc_ThenLittleEndianBytes)
{
  // The number of appended bytes is (width + 7) / 8; the first one is the low
  // byte of the CRC of the buffer as it was.
  for (int width = 1; width <= 64; width += 7)
    {
      crc_params_t params{};
      params.widthBits = width;
      params.poly = 1U;
      params.init = 0U;
      params.refIn = false;
      params.refOut = false;
      params.xorOut = 0U;
      std::vector<std::uint8_t> buffer (kCheckMessage,
                                        kCheckMessage + kCheckLength);
      std::uint64_t const crc = compute_crc_with_rev_eng_params (
          params, buffer.data (), buffer.size ());
      append_crc_least_significant_byte_first (params, buffer);
      std::size_t const appended = static_cast<std::size_t> ((width + 7) / 8);
      ASSERT_EQ (buffer.size (), kCheckLength + appended) << "width " << width;
      for (std::size_t i = 0; i < appended; ++i)
        EXPECT_EQ (buffer[kCheckLength + i],
                   static_cast<std::uint8_t> ((crc >> (8 * i)) & 0xFFU))
            << "width " << width << ", byte " << i;
    }
  // A width of 0 appends no byte.
  crc_params_t invalid{};
  invalid.widthBits = 0;
  std::vector<std::uint8_t> untouched (3, 0x5A);
  append_crc_least_significant_byte_first (invalid, untouched);
  EXPECT_EQ (untouched.size (), 3U);
}
