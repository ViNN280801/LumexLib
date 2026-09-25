// LumexFormatFloat.tests.cpp
// Floating-point output: shortest round trip for `{}`, the e / f / g / a
// presentations, precision and rounding, nan / inf, long double. Cases
// ported from fmt's format-test.cc (format_float, format_double,
// precision_rounding, prettify_float, format_nan, format_infinity,
// format_long_double, precision, exponent_range). Where fmt and std::format
// disagree the std::format text is expected: `{}` picks the shorter of the
// fixed and scientific forms (1e-4 -> "1e-04", 123456789.0f ->
// "123456792"), and `a` has no "0x" prefix.
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormat.hpp"
#include "lumex/tests/core/fmt/LumexFormatTestHelpers.hpp"

namespace fmt = lumex::core::fmt;

using format_test_helpers::format_error;

namespace
{
template <typename T>
void
expect_unknown_types_rejected (T value, char const *types)
{
  char const *const special = ".0123456789L?}{";
  for (int code = 0x21; code < 0x7f; ++code)
    {
      char const type = static_cast<char> (code);
      if (std::string (types).find (type) != std::string::npos
          || std::string (special).find (type) != std::string::npos)
        continue;
      std::string const text = std::string ("{0:10") + type + "}";
      EXPECT_EQ (format_error (text, value), "invalid format specifier")
          << text;
    }
}

double
nan_value ()
{
  return std::numeric_limits<double>::quiet_NaN ();
}

double
inf_value ()
{
  return std::numeric_limits<double>::infinity ();
}
} // namespace

TEST (LumexFormatFloatTest, GivenZero_WhenFormat_ThenShortestText)
{
  EXPECT_EQ (fmt::format ("{}", 0.0f), "0");
  EXPECT_EQ (fmt::format ("{}", 0.0), "0");
  EXPECT_EQ (fmt::format ("{:}", 0.0), "0");
  EXPECT_EQ (fmt::format ("{}", -0.0), "-0");
  EXPECT_EQ (fmt::format ("{:f}", 0.0), "0.000000");
  EXPECT_EQ (fmt::format ("{:g}", 0.0), "0");
  EXPECT_EQ (fmt::format ("{:e}", 0.0), "0.000000e+00");
}

TEST (LumexFormatFloatTest, GivenUnknownType_WhenFormat_ThenFormatError)
{
  expect_unknown_types_rejected (1.2, "eEfFgGaA");
  expect_unknown_types_rejected (1.2f, "eEfFgGaA");
}

TEST (LumexFormatFloatTest, GivenPresentationTypes_WhenFormat_ThenPrintfLike)
{
  EXPECT_EQ (fmt::format ("{0:f}", 392.5f), "392.500000");
  EXPECT_EQ (fmt::format ("{:}", 392.65), "392.65");
  EXPECT_EQ (fmt::format ("{:g}", 392.65), "392.65");
  EXPECT_EQ (fmt::format ("{:G}", 392.65), "392.65");
  EXPECT_EQ (fmt::format ("{:g}", 4.9014e6), "4.9014e+06");
  EXPECT_EQ (fmt::format ("{:G}", 4.9014e6), "4.9014E+06");
  EXPECT_EQ (fmt::format ("{:f}", 392.65), "392.650000");
  EXPECT_EQ (fmt::format ("{:F}", 392.65), "392.650000");
  EXPECT_EQ (fmt::format ("{0:e}", 392.65), "3.926500e+02");
  EXPECT_EQ (fmt::format ("{0:E}", 392.65), "3.926500E+02");
  EXPECT_EQ (fmt::format ("{0:+010.4g}", 392.65), "+0000392.6");
  EXPECT_EQ (fmt::format ("{:f}", 9223372036854775807.0),
             "9223372036854775808.000000");
  EXPECT_EQ (fmt::format ("{:e}", 1234.5), "1.234500e+03");
  EXPECT_EQ (fmt::format ("{:g}", 1e-5), "1e-05");
  EXPECT_EQ (fmt::format ("{:g}", 123456789.0), "1.23457e+08");
  EXPECT_EQ (fmt::format ("{:g}", 0.0001), "0.0001");
}

TEST (LumexFormatFloatTest, GivenLocalizedFloat_WhenClassicLocale_ThenPlain)
{
  EXPECT_EQ (fmt::format ("{:L}", 42.0), "42");
  EXPECT_EQ (fmt::format ("{:L}", 1234.5), "1234.5");
  EXPECT_EQ (fmt::format ("{:.2Lf}", 1234.5), "1234.50");
}

TEST (LumexFormatFloatTest, GivenHexType_WhenFormat_ThenStdStyleWithoutPrefix)
{
  EXPECT_EQ (fmt::format ("{:a}", 1.0), "1p+0");
  EXPECT_EQ (fmt::format ("{:24a}", 4.2f), "             1.0cccccp+2");
  EXPECT_EQ (fmt::format ("{:24a}", 4.2), "      1.0cccccccccccdp+2");
  EXPECT_EQ (fmt::format ("{:<24a}", 4.2), "1.0cccccccccccdp+2      ");
  EXPECT_EQ (fmt::format ("{:.10a}", 4.2), "1.0ccccccccdp+2");
  EXPECT_EQ (fmt::format ("{:a}", -42.0), "-1.5p+5");
  EXPECT_EQ (fmt::format ("{:A}", -42.0), "-1.5P+5");
  EXPECT_EQ (fmt::format ("{:.10a}", 0x1.ffffffffffp+2), "1.ffffffffffp+2");
  EXPECT_EQ (fmt::format ("{:.9a}", 0x1.ffffffffffp+2), "2.000000000p+2");
}

TEST (LumexFormatFloatTest, GivenHexTypeAtLimits_WhenFormat_ThenExactBits)
{
  double const min = (std::numeric_limits<double>::min) ();
  EXPECT_EQ (fmt::format ("{:a}", min), "1p-1022");
  EXPECT_EQ (fmt::format ("{:#a}", min), "1.p-1022");
  EXPECT_EQ (fmt::format ("{:a}", (std::numeric_limits<double>::max) ()),
             "1.fffffffffffffp+1023");
  EXPECT_EQ (fmt::format ("{:a}", std::numeric_limits<double>::denorm_min ()),
             "0.0000000000001p-1022");
}

TEST (LumexFormatFloatTest, GivenPrecision_WhenFormat_ThenRoundedHalfEven)
{
  EXPECT_EQ (fmt::format ("{:.0f}", 0.0), "0");
  EXPECT_EQ (fmt::format ("{:.0f}", 0.01), "0");
  EXPECT_EQ (fmt::format ("{:.0f}", 0.1), "0");
  EXPECT_EQ (fmt::format ("{:.3f}", 0.00049), "0.000");
  EXPECT_EQ (fmt::format ("{:.3f}", 0.0005), "0.001");
  EXPECT_EQ (fmt::format ("{:.3f}", 0.00149), "0.001");
  EXPECT_EQ (fmt::format ("{:.3f}", 0.0015), "0.002");
  EXPECT_EQ (fmt::format ("{:.3f}", 0.9999), "1.000");
  EXPECT_EQ (fmt::format ("{:.3}", 0.00123), "0.00123");
  EXPECT_EQ (fmt::format ("{:.16g}", 0.1), "0.1");
  EXPECT_EQ (fmt::format ("{:.0}", 1.0), "1");
  EXPECT_EQ (fmt::format ("{:.17f}", 225.51575035152064),
             "225.51575035152063720");
  EXPECT_EQ (fmt::format ("{:.1f}", -761519619559038.2), "-761519619559038.2");
  EXPECT_EQ (fmt::format ("{:.4f}", 7.2809479766055470e-15), "0.0000");
  EXPECT_EQ (fmt::format ("{:.3}", 3.14159), "3.14");
  EXPECT_EQ (fmt::format ("{:03.2f}", -1.2), "-1.20");
}

TEST (LumexFormatFloatTest, GivenLargePrecision_WhenFormat_ThenExactDigits)
{
  EXPECT_EQ (
      fmt::format ("{:.494}", 4.9406564584124654E-324),
      "4."
      "9406564584124654417656879286822137236505980261432476442558568250067550"
      "72702087518652998363616359923797965646954457177309266567103559397963987"
      "7"
      "47960107818781263007131903114045278458171678489821036887186360569987307"
      "2"
      "30500063874091535649843873124733972731696151400317153853980741262385655"
      "9"
      "11710266585566867681870395603106249319452715914924553293054565444011274"
      "8"
      "01297099995419319894090804165633245247571478690147267801593552386115501"
      "3"
      "480352649347201937902681071074917033322268447533357208324319361e-324");
  EXPECT_EQ (
      fmt::format ("{:.1074f}", 1.1125369292536e-308),
      "0."
      "0000000000000000000000000000000000000000000000000000000000000000000000"
      "00000000000000000000000000000000000000000000000000000000000000000000000"
      "0"
      "00000000000000000000000000000000000000000000000000000000000000000000000"
      "0"
      "00000000000000000000000000000000000000000000000000000000000000000000000"
      "0"
      "00000000000000000000011125369292536001974794705174196578555408151220097"
      "9"
      "35502168610941188377918212765972516343092975036449821973082295255257060"
      "1"
      "15216350589991277712958367490630117905929859841230389390918834098872901"
      "9"
      "01436146744891481783855515684045945852790730869510920249999085073508530"
      "4"
      "47847699191207220144923697506364091346191991439687709317412516750986976"
      "2"
      "48236963110036026612374264815950891959274661955324658603957152278824769"
      "7"
      "15636076627184299166723835546449645510774971693438713638053647253122439"
      "8"
      "55983379480721317237125449221625555807852490014795730938283082752410423"
      "4"
      "53096175678781984785030237967235773880780838466700475216341692176261952"
      "7"
      "46284764203742099143200565744025992819599676261037554186719805929421244"
      "6"
      "81962777939941034720757232455434770912461317493580281734466552734375");
}

TEST (LumexFormatFloatTest, GivenHugeGeneralPrecision_WhenFormat_ThenCapped)
{
  EXPECT_EQ (fmt::format ("{:.2147483647}", 0.5), "0.5");
  EXPECT_EQ (fmt::format ("{:.{}g}", 1.0, 2147483647), "1");
  EXPECT_EQ (format_error ("{:.{}e}", 42.0, 2147483647), "number is too big");
}

TEST (LumexFormatFloatTest, GivenShortestFormat_WhenFormat_ThenFixedOrExponent)
{
  // std::format (to_chars plain) takes the shorter form and scientific on a
  // tie never wins, so 1e-4 is "1e-04" and 1e10 is "1e+10"; fmt prints
  // "0.0001" and "10000000000" there.
  EXPECT_EQ (fmt::format ("{}", 1e-4), "1e-04");
  EXPECT_EQ (fmt::format ("{}", 1e-5), "1e-05");
  EXPECT_EQ (fmt::format ("{}", 1e15), "1e+15");
  EXPECT_EQ (fmt::format ("{}", 1e16), "1e+16");
  EXPECT_EQ (fmt::format ("{}", 9.999e-5), "9.999e-05");
  EXPECT_EQ (fmt::format ("{}", 1e10), "1e+10");
  EXPECT_EQ (fmt::format ("{}", 1e11), "1e+11");
  EXPECT_EQ (fmt::format ("{}", 1234e7), "1.234e+10");
  EXPECT_EQ (fmt::format ("{}", 0.001), "0.001");
  EXPECT_EQ (fmt::format ("{}", 1000.0), "1000");
  EXPECT_EQ (fmt::format ("{}", 12345678.0), "12345678");
  EXPECT_EQ (fmt::format ("{}", 1234e-2), "12.34");
  EXPECT_EQ (fmt::format ("{}", 1234e-6), "0.001234");
  EXPECT_EQ (fmt::format ("{}", 0.1f), "0.1");
  EXPECT_EQ (fmt::format ("{}", 1.35631564e-19f), "1.3563156e-19");
  EXPECT_EQ (fmt::format ("{}", 1.9156918820264798e-56),
             "1.9156918820264798e-56");
  EXPECT_EQ (fmt::format ("{}", 0.1), "0.1");
  EXPECT_EQ (fmt::format ("{}", 1e20), "1e+20");
  EXPECT_EQ (fmt::format ("{}", 123456.0), "123456");
  EXPECT_EQ (fmt::format ("{}", 1.5f), "1.5");
  EXPECT_EQ (fmt::format ("{}", 5e-324), "5e-324");
  EXPECT_EQ (fmt::format ("{}", 1.7976931348623157e308),
             "1.7976931348623157e+308");
}

TEST (LumexFormatFloatTest, GivenFloatNeedingNineDigits_WhenFormat_ThenStdText)
{
  // fmt prints "1.2345679e+08" and "1.0196664e+09"; std::format picks the
  // shorter plain form and, as the value is an integer, its exact digits.
  EXPECT_EQ (fmt::format ("{}", 123456789.0f), "123456792");
  EXPECT_EQ (fmt::format ("{}", 1019666432.0f), "1019666432");
}

TEST (LumexFormatFloatTest, GivenAnyExponent_WhenShortest_ThenReadsBackExactly)
{
  for (int e = -1074; e <= 1023; ++e)
    {
      double const value = std::ldexp (1.0, e);
      std::string const text = fmt::format ("{}", value);
      EXPECT_EQ (std::strtod (text.c_str (), nullptr), value) << text;
    }
}

TEST (LumexFormatFloatTest, GivenFloatValues_WhenShortest_ThenReadsBackExactly)
{
  float const values[] = { 0.1f,     0.2f,          0.3f,
                           1.0f / 3, 3.4028235e38f, 1.17549435e-38f,
                           1e-45f,   16777216.0f,   16777217.0f };
  for (float value : values)
    {
      std::string const text = fmt::format ("{}", value);
      EXPECT_EQ (std::strtof (text.c_str (), nullptr), value) << text;
    }
}

TEST (LumexFormatFloatTest, GivenNan_WhenFormat_ThenNanText)
{
  double const nan = nan_value ();
  EXPECT_EQ (fmt::format ("{}", nan), "nan");
  EXPECT_EQ (fmt::format ("{:+}", nan), "+nan");
  EXPECT_EQ (fmt::format ("{:+06}", nan), "  +nan");
  EXPECT_EQ (fmt::format ("{:<+06}", nan), "+nan  ");
  EXPECT_EQ (fmt::format ("{:^+06}", nan), " +nan ");
  EXPECT_EQ (fmt::format ("{:>+06}", nan), "  +nan");
  if (std::signbit (-nan))
    {
      EXPECT_EQ (fmt::format ("{}", -nan), "-nan");
      EXPECT_EQ (fmt::format ("{:+06}", -nan), "  -nan");
    }
  EXPECT_EQ (fmt::format ("{: }", nan), " nan");
  EXPECT_EQ (fmt::format ("{:F}", nan), "NAN");
  EXPECT_EQ (fmt::format ("{:<7}", nan), "nan    ");
  EXPECT_EQ (fmt::format ("{:^7}", nan), "  nan  ");
  EXPECT_EQ (fmt::format ("{:>7}", nan), "    nan");
}

TEST (LumexFormatFloatTest, GivenInfinity_WhenFormat_ThenInfText)
{
  double const inf = inf_value ();
  EXPECT_EQ (fmt::format ("{}", inf), "inf");
  EXPECT_EQ (fmt::format ("{:+}", inf), "+inf");
  EXPECT_EQ (fmt::format ("{}", -inf), "-inf");
  EXPECT_EQ (fmt::format ("{:+06}", inf), "  +inf");
  EXPECT_EQ (fmt::format ("{:+06}", -inf), "  -inf");
  EXPECT_EQ (fmt::format ("{:<+06}", inf), "+inf  ");
  EXPECT_EQ (fmt::format ("{:^+06}", inf), " +inf ");
  EXPECT_EQ (fmt::format ("{:>+06}", inf), "  +inf");
  EXPECT_EQ (fmt::format ("{: }", inf), " inf");
  EXPECT_EQ (fmt::format ("{:F}", inf), "INF");
  EXPECT_EQ (fmt::format ("{:E}", inf), "INF");
  EXPECT_EQ (fmt::format ("{:<7}", inf), "inf    ");
  EXPECT_EQ (fmt::format ("{:^7}", inf), "  inf  ");
  EXPECT_EQ (fmt::format ("{:>7}", inf), "    inf");
}

TEST (LumexFormatFloatTest, GivenLongDouble_WhenFormat_ThenLikeDouble)
{
  EXPECT_EQ (fmt::format ("{0:}", 0.0l), "0");
  EXPECT_EQ (fmt::format ("{0:f}", 0.0l), "0.000000");
  EXPECT_EQ (fmt::format ("{:.1f}", 0.000000001l), "0.0");
  EXPECT_EQ (fmt::format ("{:.2f}", 0.099l), "0.10");
  EXPECT_EQ (fmt::format ("{0:}", 392.65l), "392.65");
  EXPECT_EQ (fmt::format ("{0:g}", 392.65l), "392.65");
  EXPECT_EQ (fmt::format ("{0:G}", 392.65l), "392.65");
  EXPECT_EQ (fmt::format ("{0:f}", 392.65l), "392.650000");
  EXPECT_EQ (fmt::format ("{0:F}", 392.65l), "392.650000");
  char buffer[64];
  std::snprintf (buffer, sizeof (buffer), "%Le", 392.65l);
  EXPECT_EQ (fmt::format ("{0:e}", 392.65l), buffer);
  EXPECT_EQ (fmt::format ("{0:+010.4g}", 392.64l), "+0000392.6");
}

TEST (LumexFormatFloatTest, GivenCLocaleWithComma_WhenFormat_ThenPointKept)
{
  // Without `L` the output never depends on the C locale, even where the
  // printf / strtod fallback is used.
  char const *const current = std::setlocale (LC_NUMERIC, nullptr);
  std::string const saved = current != nullptr ? current : "C";
  if (std::setlocale (LC_NUMERIC, "de_DE.UTF-8") == nullptr
      && std::setlocale (LC_NUMERIC, "German_Germany.1252") == nullptr
      && std::setlocale (LC_NUMERIC, "de-DE") == nullptr)
    GTEST_SKIP () << "no locale with a decimal comma installed";
  std::string const shortest = fmt::format ("{}", 1.5);
  std::string const fixed = fmt::format ("{:.2f}", 1.5);
  std::string const scientific = fmt::format ("{:e}", 1.5);
  std::string const general = fmt::format ("{}", 0.1);
  std::setlocale (LC_NUMERIC, saved.c_str ());
  EXPECT_EQ (shortest, "1.5");
  EXPECT_EQ (fixed, "1.50");
  EXPECT_EQ (scientific, "1.500000e+00");
  EXPECT_EQ (general, "0.1");
}
