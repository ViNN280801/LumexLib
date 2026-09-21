// LumexSafeNumericComparator.tests.cpp
// Merged from DChannel SafeNumericComparator.tests.cpp plus Lumex-only
// fits_in_type, free-function, and ordering-helper cases.

#include <cmath>
#include <cstddef>
#include <limits>
#include <thread>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#if __cplusplus >= 202002L
#include <compare>
#endif

#include <gtest/gtest.h>

#include "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

using namespace lumex::core::utility::numeric;

using AllArithmeticTypes
    = ::testing::Types<char, signed char, unsigned char, short, unsigned short,
                       int, unsigned int, long, unsigned long, long long,
                       unsigned long long, float, double, long double>;

class SafeComparatorTestBase : public ::testing::Test
{
protected:
  template <typename T>
  static T
  get_min_value ()
  {
    return std::numeric_limits<T>::min ();
  }

  template <typename T>
  static T
  get_max_value ()
  {
    return std::numeric_limits<T>::max ();
  }

  template <typename T>
  static bool
  is_signed_type ()
  {
    return std::is_signed<T>::value;
  }

  template <typename T>
  static bool
  is_floating_point_type ()
  {
    return std::is_floating_point<T>::value;
  }
};

template <typename T>
class SafeComparatorSameTypeTest : public SafeComparatorTestBase
{
protected:
  using Type = T;
};

TYPED_TEST_SUITE (SafeComparatorSameTypeTest, AllArithmeticTypes);

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenEqualValues_WhenSafeEqual_ThenReturnsTrue)
{
  using Type = typename TestFixture::Type;

  Type const test_value = static_cast<Type> (42);
  SafeComparator<Type> comparator (test_value);

  EXPECT_TRUE (comparator.safe_equal (test_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should return true for equal values";
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenDifferentValues_WhenSafeNotEqual_ThenReturnsTrue)
{
  using Type = typename TestFixture::Type;

  Type const test_value = static_cast<Type> (42);
  Type const different_value = static_cast<Type> (100);
  SafeComparator<Type> comparator (test_value);

  EXPECT_TRUE (comparator.safe_not_equal (different_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should return true for different values";
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenLesserValue_WhenSafeLess_ThenReturnsTrue)
{
  using Type = typename TestFixture::Type;

  Type const smaller_value = static_cast<Type> (10);
  Type const larger_value = static_cast<Type> (20);
  SafeComparator<Type> comparator (smaller_value);

  EXPECT_TRUE (comparator.safe_less (larger_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should return true when first value is less";
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenGreaterValue_WhenSafeGreater_ThenReturnsTrue)
{
  using Type = typename TestFixture::Type;

  Type const larger_value = static_cast<Type> (20);
  Type const smaller_value = static_cast<Type> (10);
  SafeComparator<Type> comparator (larger_value);

  EXPECT_TRUE (comparator.safe_greater (smaller_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should return true when first value is greater";
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenLessOrEqualValue_WhenSafeLessEqual_ThenReturnsTrue)
{
  using Type = typename TestFixture::Type;

  Type const test_value = static_cast<Type> (15);
  SafeComparator<Type> comparator (test_value);

  EXPECT_TRUE (comparator.safe_less_equal (static_cast<Type> (20)))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should return true for less-than case";

  EXPECT_TRUE (comparator.safe_less_equal (test_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should return true for equal case";
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenGreaterOrEqualValue_WhenSafeGreaterEqual_ThenReturnsTrue)
{
  using Type = typename TestFixture::Type;

  Type const test_value = static_cast<Type> (15);
  SafeComparator<Type> comparator (test_value);

  EXPECT_TRUE (comparator.safe_greater_equal (static_cast<Type> (10)))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should return true for greater-than case";

  EXPECT_TRUE (comparator.safe_greater_equal (test_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should return true for equal case";
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenSameValue_WhenSafeCompare_ThenReturnsTrue)
{
  using Type = typename TestFixture::Type;

  Type const test_value = static_cast<Type> (42);
  SafeComparator<Type> comparator (test_value);

  EXPECT_TRUE (comparator.safe_compare (test_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> legacy safe_compare should return true for equal values";
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenMinValue_WhenCompare_ThenHandlesCorrectly)
{
  using Type = typename TestFixture::Type;

  Type const min_value = this->template get_min_value<Type> ();
  SafeComparator<Type> comparator (min_value);

  EXPECT_TRUE (comparator.safe_equal (min_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should handle minimum value correctly";

  Type const larger_value = static_cast<Type> (min_value + 1);
  EXPECT_TRUE (comparator.safe_less (larger_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should correctly compare minimum value as less";
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenMaxValue_WhenCompare_ThenHandlesCorrectly)
{
  using Type = typename TestFixture::Type;

  Type const max_value = this->template get_max_value<Type> ();
  SafeComparator<Type> comparator (max_value);

  EXPECT_TRUE (comparator.safe_equal (max_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should handle maximum value correctly";

  Type smaller_value;
  LUMEX_CONSTEXPR_IF (std::is_floating_point<Type>::value)
  {
    smaller_value = std::nextafter (max_value, static_cast<Type> (0));
  }
  else
  {
    smaller_value = static_cast<Type> (max_value - static_cast<Type> (1));
  }
  EXPECT_TRUE (comparator.safe_greater (smaller_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should correctly compare maximum value as greater";
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenZeroValue_WhenCompare_ThenHandlesCorrectly)
{
  using Type = typename TestFixture::Type;

  Type const zero_value = static_cast<Type> (0);
  SafeComparator<Type> comparator (zero_value);

  EXPECT_TRUE (comparator.safe_equal (zero_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should handle zero equality correctly";

  Type const positive_value = static_cast<Type> (1);
  EXPECT_TRUE (comparator.safe_less (positive_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should correctly compare zero as less than positive";

  if (this->template is_signed_type<Type> ())
    {
      Type const negative_value = static_cast<Type> (-1);
      EXPECT_TRUE (comparator.safe_greater (negative_value))
          << "SafeComparator<" << typeid (Type).name ()
          << "> should correctly compare zero as greater than negative";
    }
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenNegativeValue_WhenCompare_ThenHandlesCorrectly)
{
  using Type = typename TestFixture::Type;

  if (!this->template is_signed_type<Type> ())
    {
      GTEST_SKIP () << "Skipping negative value test for unsigned type";
    }

  Type const negative_value = static_cast<Type> (-10);
  SafeComparator<Type> comparator (negative_value);

  EXPECT_TRUE (comparator.safe_equal (negative_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should handle negative value equality correctly";

  Type const positive_value = static_cast<Type> (10);
  EXPECT_TRUE (comparator.safe_less (positive_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should correctly compare negative as less than positive";

  Type const more_negative_value = static_cast<Type> (-20);
  EXPECT_TRUE (comparator.safe_greater (more_negative_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should correctly compare negative as greater than more negative";
}

TYPED_TEST (SafeComparatorSameTypeTest,
            GivenPositiveValue_WhenCompare_ThenHandlesCorrectly)
{
  using Type = typename TestFixture::Type;

  Type const positive_value = static_cast<Type> (25);
  SafeComparator<Type> comparator (positive_value);

  EXPECT_TRUE (comparator.safe_equal (positive_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should handle positive value equality correctly";

  Type const larger_value = static_cast<Type> (50);
  EXPECT_TRUE (comparator.safe_less (larger_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should correctly compare positive as less than larger";

  Type const smaller_value = static_cast<Type> (10);
  EXPECT_TRUE (comparator.safe_greater (smaller_value))
      << "SafeComparator<" << typeid (Type).name ()
      << "> should correctly compare positive as greater than smaller";
}

template <typename T, typename U> struct TypePair
{
  using first_type = T;
  using second_type = U;
};

template <typename PairT>
class SafeComparatorMixedTypesTest : public SafeComparatorTestBase
{
protected:
  using FirstType = typename PairT::first_type;
  using SecondType = typename PairT::second_type;
};

using AllTypePairs = ::testing::Types<
    TypePair<char, char>, TypePair<char, signed char>,
    TypePair<char, unsigned char>, TypePair<char, short>,
    TypePair<char, unsigned short>, TypePair<char, int>,
    TypePair<char, unsigned int>, TypePair<char, long>,
    TypePair<char, unsigned long>, TypePair<char, long long>,
    TypePair<char, unsigned long long>, TypePair<char, float>,
    TypePair<char, double>,

    TypePair<signed char, char>, TypePair<signed char, signed char>,
    TypePair<signed char, unsigned char>, TypePair<signed char, short>,
    TypePair<signed char, unsigned short>, TypePair<signed char, int>,
    TypePair<signed char, unsigned int>, TypePair<signed char, long>,
    TypePair<signed char, unsigned long>, TypePair<signed char, long long>,
    TypePair<signed char, unsigned long long>, TypePair<signed char, float>,
    TypePair<signed char, double>,

    TypePair<unsigned char, char>, TypePair<unsigned char, signed char>,
    TypePair<unsigned char, unsigned char>, TypePair<unsigned char, short>,
    TypePair<unsigned char, unsigned short>, TypePair<unsigned char, int>,
    TypePair<unsigned char, unsigned int>, TypePair<unsigned char, long>,
    TypePair<unsigned char, unsigned long>, TypePair<unsigned char, long long>,
    TypePair<unsigned char, unsigned long long>,
    TypePair<unsigned char, float>, TypePair<unsigned char, double>,

    TypePair<short, char>, TypePair<short, signed char>,
    TypePair<short, unsigned char>, TypePair<short, short>,
    TypePair<short, unsigned short>, TypePair<short, int>,
    TypePair<short, unsigned int>, TypePair<short, long>,
    TypePair<short, unsigned long>, TypePair<short, long long>,
    TypePair<short, unsigned long long>, TypePair<short, float>,
    TypePair<short, double>,

    TypePair<unsigned short, char>, TypePair<unsigned short, signed char>,
    TypePair<unsigned short, unsigned char>, TypePair<unsigned short, short>,
    TypePair<unsigned short, unsigned short>, TypePair<unsigned short, int>,
    TypePair<unsigned short, unsigned int>, TypePair<unsigned short, long>,
    TypePair<unsigned short, unsigned long>,
    TypePair<unsigned short, long long>,
    TypePair<unsigned short, unsigned long long>,
    TypePair<unsigned short, float>, TypePair<unsigned short, double>,

    TypePair<int, char>, TypePair<int, signed char>,
    TypePair<int, unsigned char>, TypePair<int, short>,
    TypePair<int, unsigned short>, TypePair<int, int>,
    TypePair<int, unsigned int>, TypePair<int, long>,
    TypePair<int, unsigned long>, TypePair<int, long long>,
    TypePair<int, unsigned long long>, TypePair<int, float>,
    TypePair<int, double>,

    TypePair<unsigned int, char>, TypePair<unsigned int, signed char>,
    TypePair<unsigned int, unsigned char>, TypePair<unsigned int, short>,
    TypePair<unsigned int, unsigned short>, TypePair<unsigned int, int>,
    TypePair<unsigned int, unsigned int>, TypePair<unsigned int, long>,
    TypePair<unsigned int, unsigned long>, TypePair<unsigned int, long long>,
    TypePair<unsigned int, unsigned long long>, TypePair<unsigned int, float>,
    TypePair<unsigned int, double>,

    TypePair<long, char>, TypePair<long, signed char>,
    TypePair<long, unsigned char>, TypePair<long, short>,
    TypePair<long, unsigned short>, TypePair<long, int>,
    TypePair<long, unsigned int>, TypePair<long, long>,
    TypePair<long, unsigned long>, TypePair<long, long long>,
    TypePair<long, unsigned long long>, TypePair<long, float>,
    TypePair<long, double>,

    TypePair<unsigned long, char>, TypePair<unsigned long, signed char>,
    TypePair<unsigned long, unsigned char>, TypePair<unsigned long, short>,
    TypePair<unsigned long, unsigned short>, TypePair<unsigned long, int>,
    TypePair<unsigned long, unsigned int>, TypePair<unsigned long, long>,
    TypePair<unsigned long, unsigned long>, TypePair<unsigned long, long long>,
    TypePair<unsigned long, unsigned long long>,
    TypePair<unsigned long, float>, TypePair<unsigned long, double>,

    TypePair<long long, char>, TypePair<long long, signed char>,
    TypePair<long long, unsigned char>, TypePair<long long, short>,
    TypePair<long long, unsigned short>, TypePair<long long, int>,
    TypePair<long long, unsigned int>, TypePair<long long, long>,
    TypePair<long long, unsigned long>, TypePair<long long, long long>,
    TypePair<long long, unsigned long long>, TypePair<long long, float>,
    TypePair<long long, double>,

    TypePair<unsigned long long, char>,
    TypePair<unsigned long long, signed char>,
    TypePair<unsigned long long, unsigned char>,
    TypePair<unsigned long long, short>,
    TypePair<unsigned long long, unsigned short>,
    TypePair<unsigned long long, int>,
    TypePair<unsigned long long, unsigned int>,
    TypePair<unsigned long long, long>,
    TypePair<unsigned long long, unsigned long>,
    TypePair<unsigned long long, long long>,
    TypePair<unsigned long long, unsigned long long>,
    TypePair<unsigned long long, float>, TypePair<unsigned long long, double>,

    TypePair<float, char>, TypePair<float, signed char>,
    TypePair<float, unsigned char>, TypePair<float, short>,
    TypePair<float, unsigned short>, TypePair<float, int>,
    TypePair<float, unsigned int>, TypePair<float, long>,
    TypePair<float, unsigned long>, TypePair<float, long long>,
    TypePair<float, unsigned long long>, TypePair<float, float>,
    TypePair<float, double>,

    TypePair<double, char>, TypePair<double, signed char>,
    TypePair<double, unsigned char>, TypePair<double, short>,
    TypePair<double, unsigned short>, TypePair<double, int>,
    TypePair<double, unsigned int>, TypePair<double, long>,
    TypePair<double, unsigned long>, TypePair<double, long long>,
    TypePair<double, unsigned long long>, TypePair<double, float>,
    TypePair<double, double>>;

TYPED_TEST_SUITE (SafeComparatorMixedTypesTest, AllTypePairs);

TYPED_TEST (SafeComparatorMixedTypesTest,
            GivenEqualValues_WhenSafeEqual_ThenReturnsTrue)
{
  using FirstType = typename TestFixture::FirstType;
  using SecondType = typename TestFixture::SecondType;

  FirstType const first_value = static_cast<FirstType> (42);
  SecondType const second_value = static_cast<SecondType> (42);

  SafeComparator<FirstType> comparator (first_value);

  EXPECT_TRUE (comparator.safe_equal (second_value))
      << "SafeComparator<" << typeid (FirstType).name ()
      << "> should return true for equal values with "
      << typeid (SecondType).name ();
}

TYPED_TEST (SafeComparatorMixedTypesTest,
            GivenDifferentValues_WhenSafeNotEqual_ThenReturnsTrue)
{
  using FirstType = typename TestFixture::FirstType;
  using SecondType = typename TestFixture::SecondType;

  FirstType const first_value = static_cast<FirstType> (10);
  SecondType const second_value = static_cast<SecondType> (20);

  SafeComparator<FirstType> comparator (first_value);

  EXPECT_TRUE (comparator.safe_not_equal (second_value))
      << "SafeComparator<" << typeid (FirstType).name ()
      << "> should return true for different values with "
      << typeid (SecondType).name ();
}

TYPED_TEST (SafeComparatorMixedTypesTest,
            GivenLesserValue_WhenSafeLess_ThenReturnsTrue)
{
  using FirstType = typename TestFixture::FirstType;
  using SecondType = typename TestFixture::SecondType;

  FirstType const first_value = static_cast<FirstType> (10);
  SecondType const second_value = static_cast<SecondType> (20);

  SafeComparator<FirstType> comparator (first_value);

  EXPECT_TRUE (comparator.safe_less (second_value))
      << "SafeComparator<" << typeid (FirstType).name ()
      << "> should return true for less-than with "
      << typeid (SecondType).name ();
}

TYPED_TEST (SafeComparatorMixedTypesTest,
            GivenGreaterValue_WhenSafeGreater_ThenReturnsTrue)
{
  using FirstType = typename TestFixture::FirstType;
  using SecondType = typename TestFixture::SecondType;

  FirstType const first_value = static_cast<FirstType> (30);
  SecondType const second_value = static_cast<SecondType> (20);

  SafeComparator<FirstType> comparator (first_value);

  EXPECT_TRUE (comparator.safe_greater (second_value))
      << "SafeComparator<" << typeid (FirstType).name ()
      << "> should return true for greater-than with "
      << typeid (SecondType).name ();
}

TYPED_TEST (SafeComparatorMixedTypesTest,
            GivenLessOrEqualValue_WhenSafeLessEqual_ThenReturnsTrue)
{
  using FirstType = typename TestFixture::FirstType;
  using SecondType = typename TestFixture::SecondType;

  FirstType const first_value = static_cast<FirstType> (15);
  SafeComparator<FirstType> comparator (first_value);

  SecondType const larger_value = static_cast<SecondType> (20);
  EXPECT_TRUE (comparator.safe_less_equal (larger_value))
      << "SafeComparator<" << typeid (FirstType).name ()
      << "> should return true for less-or-equal (less) with "
      << typeid (SecondType).name ();

  SecondType const equal_value = static_cast<SecondType> (15);
  EXPECT_TRUE (comparator.safe_less_equal (equal_value))
      << "SafeComparator<" << typeid (FirstType).name ()
      << "> should return true for less-or-equal (equal) with "
      << typeid (SecondType).name ();
}

TYPED_TEST (SafeComparatorMixedTypesTest,
            GivenGreaterOrEqualValue_WhenSafeGreaterEqual_ThenReturnsTrue)
{
  using FirstType = typename TestFixture::FirstType;
  using SecondType = typename TestFixture::SecondType;

  FirstType const first_value = static_cast<FirstType> (25);
  SafeComparator<FirstType> comparator (first_value);

  SecondType const smaller_value = static_cast<SecondType> (20);
  EXPECT_TRUE (comparator.safe_greater_equal (smaller_value))
      << "SafeComparator<" << typeid (FirstType).name ()
      << "> should return true for greater-or-equal (greater) with "
      << typeid (SecondType).name ();

  SecondType const equal_value = static_cast<SecondType> (25);
  EXPECT_TRUE (comparator.safe_greater_equal (equal_value))
      << "SafeComparator<" << typeid (FirstType).name ()
      << "> should return true for greater-or-equal (equal) with "
      << typeid (SecondType).name ();
}

TYPED_TEST (SafeComparatorMixedTypesTest,
            GivenLegacySafeCompare_WhenCalled_ThenWorksCorrectly)
{
  using FirstType = typename TestFixture::FirstType;
  using SecondType = typename TestFixture::SecondType;

  FirstType const first_value = static_cast<FirstType> (30);
  SecondType const second_value = static_cast<SecondType> (20);

  SafeComparator<FirstType> comparator (first_value);

  EXPECT_TRUE (comparator.safe_compare (second_value))
      << "SafeComparator<" << typeid (FirstType).name ()
      << "> legacy safe_compare should work correctly with "
      << typeid (SecondType).name ();
}

TEST (SafeComparatorDocumentedCases,
      GivenDocumentedOverflowCase_WhenUnsignedCharVsInt_ThenPreventsOverflow)
{
  unsigned char const block_size = 200;
  int const packet_size = 300;

  SafeUCharComparator safe_size (block_size);
  EXPECT_FALSE (safe_size.safe_compare (packet_size))
      << "unsigned char(200) >= int(300) should be false";
  EXPECT_TRUE (safe_size.safe_less (packet_size));
  EXPECT_FALSE (safe_size.safe_equal (packet_size));
  EXPECT_FALSE (safe_compare (block_size, packet_size));
}

TEST (SafeComparatorDocumentedCases,
      GivenDetailedAlgorithmCase_WhenUnsignedCharVsInt_ThenFollowsAlgorithm)
{
  unsigned char const block_size = 252;
  int const packet_size = 1652;

  SafeUCharComparator safe_size (block_size);
  EXPECT_FALSE (safe_size.safe_compare (packet_size))
      << "unsigned char(252) >= int(1652) should be false";
}

TEST (SafeComparatorDocumentedCases,
      GivenUnsignedCharVersusSmallerInt_WhenSafeCompare_ThenTrue)
{
  unsigned char const block_size = 200;
  int const threshold = 150;

  SafeUCharComparator const safe_size (block_size);
  EXPECT_TRUE (safe_size.safe_compare (threshold));
  EXPECT_FALSE (safe_size.safe_less (threshold));
}

TEST (SafeComparatorDocumentedCases,
      GivenSignMismatchCase_WhenSignedVsUnsigned_ThenHandlesCorrectly)
{
  int const signed_value = -1;
  unsigned int const unsigned_value = 1;

  SafeIntComparator safe_signed (signed_value);

  EXPECT_TRUE (safe_signed.safe_less (unsigned_value))
      << "int(-1) < unsigned int(1) should be true";

  EXPECT_FALSE (safe_signed.safe_greater_equal (unsigned_value))
      << "int(-1) >= unsigned int(1) should be false";
}

TEST (SafeComparatorDocumentedCases,
      GivenMixedTypesCase_WhenIntegralVsFloating_ThenHandlesCorrectly)
{
  float const float_value = 3.14f;
  int const int_value = 3;

  SafeFloatComparator safe_float (float_value);

  EXPECT_TRUE (safe_float.safe_greater (int_value))
      << "float(3.14) > int(3) should be true";

  EXPECT_FALSE (safe_float.safe_less_equal (int_value))
      << "float(3.14) <= int(3) should be false";
}

TEST (SafeComparatorDocumentedCases,
      GivenEqualMixedIntegralValues_WhenSafeEqual_ThenTrue)
{
  unsigned char const size = 42;
  int const threshold = 42;

  EXPECT_TRUE (safe_equal (size, threshold));
  EXPECT_FALSE (safe_less (size, threshold));
  EXPECT_TRUE (safe_compare (size, threshold));
}

TEST (SafeComparatorClassMethods,
      GivenDefaultConstructor_WhenCreated_ThenValueIsZero)
{
  SafeComparator<int> comparator;
  EXPECT_EQ (comparator.get (), 0);

  SafeComparator<float> float_comparator;
  EXPECT_FLOAT_EQ (float_comparator.get (), 0.0f);
}

TEST (SafeComparatorClassMethods,
      GivenValueConstructor_WhenCreated_ThenValueIsSet)
{
  int const test_value = 42;
  SafeComparator<int> comparator (test_value);
  EXPECT_EQ (comparator.get (), test_value);

  float const float_value = 3.14f;
  SafeComparator<float> float_comparator (float_value);
  EXPECT_FLOAT_EQ (float_comparator.get (), float_value);
}

TEST (SafeComparatorClassMethods,
      GivenCopyConstructor_WhenCopied_ThenValuesCopied)
{
  int const original_value = 100;
  SafeComparator<int> original (original_value);
  SafeComparator<int> copy (original);

  EXPECT_EQ (original.get (), original_value);
  EXPECT_EQ (copy.get (), original_value);
  EXPECT_EQ (original.get (), copy.get ());
}

TEST (SafeComparatorClassMethods,
      GivenMoveConstructor_WhenMoved_ThenStateIsTransferred)
{
  int const original_value = 200;
  SafeComparator<int> original (original_value);
  SafeComparator<int> moved (std::move (original));

  EXPECT_EQ (moved.get (), original_value);
  EXPECT_NO_THROW (original.get ());
}

TEST (SafeComparatorClassMethods,
      GivenCopyAssignment_WhenAssigned_ThenValuesCopied)
{
  int const source_value = 150;
  int const target_value = 50;

  SafeComparator<int> source (source_value);
  SafeComparator<int> target (target_value);

  target = source;

  EXPECT_EQ (source.get (), source_value);
  EXPECT_EQ (target.get (), source_value);
}

TEST (SafeComparatorClassMethods,
      GivenMoveAssignment_WhenMoved_ThenStateIsTransferred)
{
  int const source_value = 250;
  int const target_value = 75;

  SafeComparator<int> source (source_value);
  SafeComparator<int> target (target_value);

  target = std::move (source);

  EXPECT_EQ (target.get (), source_value);
  EXPECT_NO_THROW (source.get ());
}

TEST (SafeComparatorClassMethods,
      GivenSelfAssignment_WhenAssigned_ThenNoChange)
{
  int const test_value = 300;
  SafeComparator<int> comparator (test_value);

  SafeComparator<int> &ref = comparator;
  ref = comparator;

  EXPECT_EQ (comparator.get (), test_value);
}

TEST (SafeComparatorClassMethods, GivenUpdate_WhenCalled_ThenValueUpdated)
{
  int const initial_value = 10;
  int const new_value = 20;

  SafeComparator<int> comparator (initial_value);
  EXPECT_EQ (comparator.get (), initial_value);

  comparator.update (new_value);
  EXPECT_EQ (comparator.get (), new_value);
}

TEST (SafeComparatorClassMethods, GivenGet_WhenCalled_ThenReturnsCurrentValue)
{
  int const test_value = 42;
  SafeComparator<int> comparator (test_value);

  EXPECT_EQ (comparator.get (), test_value);
  EXPECT_EQ (comparator.get (), test_value);
}

TEST (SafeComparatorClassMethods,
      GivenCompareAndSet_WhenExpectedMatches_ThenSetsValue)
{
  int const initial_value = 100;
  int const expected_value = 100;
  int const desired_value = 200;

  SafeComparator<int> comparator (initial_value);

  bool const result
      = comparator.compare_and_set (expected_value, desired_value);

  EXPECT_TRUE (result);
  EXPECT_EQ (comparator.get (), desired_value);
}

TEST (SafeComparatorClassMethods,
      GivenCompareAndSet_WhenExpectedDiffers_ThenReturnsFalse)
{
  int const initial_value = 100;
  int const expected_value = 50;
  int const desired_value = 200;

  SafeComparator<int> comparator (initial_value);

  bool const result
      = comparator.compare_and_set (expected_value, desired_value);

  EXPECT_FALSE (result);
  EXPECT_EQ (comparator.get (), initial_value);
}

TEST (SafeComparatorClassMethods,
      GivenImplicitConversion_WhenUsed_ThenReturnsValue)
{
  int const test_value = 42;
  SafeComparator<int> comparator (test_value);

  int const direct_value = comparator;
  EXPECT_EQ (direct_value, test_value);

  auto const times_two = [] (int value) { return value * 2; };
  int const doubled_value = times_two (comparator);
  EXPECT_EQ (doubled_value, test_value * 2);
}

TEST (SafeComparatorClassMethods,
      GivenDirectAssignment_WhenAssigned_ThenValueUpdated)
{
  int const initial_value = 10;
  int const new_value = 30;

  SafeComparator<int> comparator (initial_value);
  EXPECT_EQ (comparator.get (), initial_value);

  comparator = new_value;
  EXPECT_EQ (comparator.get (), new_value);
}

TEST (SafeComparatorAtomicTests,
      GivenAtomicVsNonAtomic_WhenSameOperations_ThenIdenticalResults)
{
  int const test_value = 42;

  SafeComparator<int, false> non_atomic (test_value);
  SafeComparator<int, true> atomic (test_value);

  EXPECT_EQ (non_atomic.get (), atomic.get ());

  int const new_value = 100;
  non_atomic.update (new_value);
  atomic.update (new_value);

  EXPECT_EQ (non_atomic.get (), atomic.get ());

  int const compare_value = 50;
  EXPECT_EQ (non_atomic.safe_equal (compare_value),
             atomic.safe_equal (compare_value));
  EXPECT_EQ (non_atomic.safe_less (compare_value),
             atomic.safe_less (compare_value));
}

TEST (SafeComparatorAtomicTests,
      GivenAtomicComparator_WhenUsed_ThenDoesNotCrash)
{
  SafeComparator<int, true> atomic_comparator (0);

  int const num_threads = 4;
  int const operations_per_thread = 1000;
  std::vector<std::thread> threads;
  std::vector<int> results (static_cast<std::size_t> (num_threads));

  for (int i = 0; i < num_threads; ++i)
    {
      threads.emplace_back ([&, i] () {
        int thread_result = 0;
        for (int j = 0; j < operations_per_thread; ++j)
          {
            atomic_comparator.update (j);
            thread_result += atomic_comparator.get ();
            if (atomic_comparator.compare_and_set (j, j + 1))
              thread_result += 1;
          }
        results[static_cast<std::size_t> (i)] = thread_result;
      });
    }

  for (auto &thread : threads)
    thread.join ();

  EXPECT_EQ (results.size (), static_cast<std::size_t> (num_threads));
}

TEST (SafeComparatorFitsInType, GivenIntVersusNaN_WhenSafeCompare_ThenFalse)
{
  SafeIntComparator const safe_counter (3);
  float const nan_threshold = std::numeric_limits<float>::quiet_NaN ();

  EXPECT_FALSE (safe_counter.safe_compare (nan_threshold));
  EXPECT_FALSE (safe_equal (3, nan_threshold));
  EXPECT_FALSE (fits_in_type<int> (nan_threshold));
}

TEST (SafeComparatorFitsInType,
      GivenIntVersusInfinity_WhenFitsInType_ThenFalse)
{
  float const pos_inf = std::numeric_limits<float>::infinity ();
  float const neg_inf = -std::numeric_limits<float>::infinity ();

  EXPECT_FALSE (fits_in_type<int> (pos_inf));
  EXPECT_FALSE (fits_in_type<int> (neg_inf));
}

TEST (SafeComparatorFitsInType,
      GivenValueOutsideUnsignedChar_WhenFitsInType_ThenFalse)
{
  EXPECT_FALSE (fits_in_type<unsigned char> (300));
  EXPECT_TRUE (fits_in_type<unsigned char> (200));
  EXPECT_TRUE (fits_in_type<unsigned char> (0));
  EXPECT_TRUE (fits_in_type<unsigned char> (255));
}

TEST (SafeComparatorFitsInType,
      GivenRepresentableFloat_WhenFitsInTypeInt_ThenTrue)
{
  EXPECT_TRUE (fits_in_type<int> (3.14f));
}

#if __cplusplus >= 202002L

TEST (SafeComparatorCpp20Tests,
      GivenThreeWayCompare_WhenIntegral_ThenStrongOrdering)
{
  SafeComparator<int> comparator (42);
  int const other_value = 50;

  auto const result = comparator.safe_three_way_compare (other_value);

  EXPECT_TRUE (result == std::strong_ordering::less);
  EXPECT_FALSE (result == std::strong_ordering::equal);
  EXPECT_FALSE (result == std::strong_ordering::greater);
}

TEST (SafeComparatorCpp20Tests,
      GivenThreeWayCompare_WhenFloating_ThenPartialOrdering)
{
  SafeComparator<float> comparator (3.14f);
  float const other_value = 2.71f;

  auto const result = comparator.safe_three_way_compare (other_value);

  EXPECT_TRUE (result == std::partial_ordering::greater);
  EXPECT_FALSE (result == std::partial_ordering::equivalent);
  EXPECT_FALSE (result == std::partial_ordering::less);
}

TEST (SafeComparatorCpp20Tests, GivenThreeWayCompare_WhenNaN_ThenUnordered)
{
  float const nan_value = std::numeric_limits<float>::quiet_NaN ();
  SafeComparator<float> comparator (nan_value);
  float const normal_value = 1.0f;

  auto const result = comparator.safe_three_way_compare (normal_value);

  EXPECT_FALSE (result == std::partial_ordering::equivalent);
  EXPECT_FALSE (result == std::partial_ordering::less);
  EXPECT_FALSE (result == std::partial_ordering::greater);
}

TEST (SafeComparatorCpp20Tests,
      GivenUtilityFunctions_WhenCalled_ThenWorkCorrectly)
{
  SafeComparator<int> comparator (42);
  int const smaller_value = 30;
  int const larger_value = 50;
  int const equal_value = 42;

  auto const result1 = comparator.safe_three_way_compare (smaller_value);
  EXPECT_TRUE (result1 == std::strong_ordering::greater);
  EXPECT_FALSE (result1 == std::strong_ordering::less);
  EXPECT_FALSE (result1 == std::strong_ordering::equal);

  auto const result2 = comparator.safe_three_way_compare (larger_value);
  EXPECT_TRUE (result2 == std::strong_ordering::less);
  EXPECT_FALSE (result2 == std::strong_ordering::greater);
  EXPECT_FALSE (result2 == std::strong_ordering::equal);

  auto const result3 = comparator.safe_three_way_compare (equal_value);
  EXPECT_TRUE (result3 == std::strong_ordering::equal);
  EXPECT_FALSE (result3 == std::strong_ordering::less);
  EXPECT_FALSE (result3 == std::strong_ordering::greater);
}

TEST (SafeComparatorCpp20Tests,
      GivenUnsignedCharVersusLargerInt_WhenThreeWay_ThenLess)
{
  unsigned char const size = 200;
  int const threshold = 300;
  SafeUCharComparator const safe_size (size);

  auto const result = safe_size.safe_three_way_compare (threshold);
  EXPECT_TRUE (is_less (result));
  EXPECT_TRUE (is_less_equal (result));
  EXPECT_TRUE (is_not_equal (result));
  EXPECT_FALSE (is_equal (result));
  EXPECT_FALSE (is_greater (result));
  EXPECT_FALSE (is_greater_equal (result));
}

TEST (SafeComparatorCpp20Tests, GivenEqualMixedValues_WhenThreeWay_ThenEqual)
{
  auto const result
      = safe_three_way_compare (static_cast<unsigned char> (42), 42);
  EXPECT_TRUE (is_equal (result));
  EXPECT_TRUE (is_less_equal (result));
  EXPECT_TRUE (is_greater_equal (result));
  EXPECT_FALSE (is_less (result));
  EXPECT_FALSE (is_greater (result));
  EXPECT_FALSE (is_not_equal (result));
}

TEST (SafeComparatorCpp20Tests,
      GivenLargerUnsignedChar_WhenThreeWay_ThenGreater)
{
  auto const result
      = safe_three_way_compare (static_cast<unsigned char> (200), 150);
  EXPECT_TRUE (is_greater (result));
  EXPECT_TRUE (is_greater_equal (result));
  EXPECT_TRUE (is_not_equal (result));
  EXPECT_FALSE (is_less (result));
  EXPECT_FALSE (is_less_equal (result));
  EXPECT_FALSE (is_equal (result));
}

TEST (SafeComparatorCpp20Tests,
      GivenNamedOrderings_WhenHelpers_ThenMatchCategory)
{
  EXPECT_TRUE (is_less (std::strong_ordering::less));
  EXPECT_TRUE (is_equal (std::strong_ordering::equal));
  EXPECT_TRUE (is_greater (std::strong_ordering::greater));
  EXPECT_TRUE (is_less (std::partial_ordering::less));
  EXPECT_TRUE (is_equal (std::partial_ordering::equivalent));
  EXPECT_TRUE (is_greater (std::partial_ordering::greater));
  EXPECT_FALSE (is_less (std::partial_ordering::unordered));
  EXPECT_FALSE (is_greater (std::partial_ordering::unordered));
  EXPECT_FALSE (is_equal (std::partial_ordering::unordered));
  EXPECT_TRUE (is_less (std::weak_ordering::less));
  EXPECT_TRUE (is_equal (std::weak_ordering::equivalent));
  EXPECT_TRUE (is_greater (std::weak_ordering::greater));
}

#endif
