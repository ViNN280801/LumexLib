// Compile checks of the ordering classes (LumexOrdering.hpp) and of the
// three-way part of the safe comparator, built by
// cmake.ordering_compile_checks. Exactly one of LUMEX_ORDERING_GOOD_CASE or
// LUMEX_ORDERING_BAD_CASE=<n> is defined; the fixture compiles with warnings
// as errors. Cases 1-6 and 11-13 are uses the standard orderings reject (no
// default constructor or constructor from a number, no relational operator
// between two orderings or against a number other than the literal 0, no
// conversion to a stronger category, a constant is not assignable); cases 7-10
// are static assertions of the comparator that have no standard counterpart.
// Cases 14-17 are the conversions between the own classes and the classes of
// <compare> that do not exist (the directions the standard categories lack);
// they need <compare> and fail with the text "needs three-way comparison"
// where it is missing, which the fixture reads as "not applicable".

#include <string>

#include "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp"

#if LUMEX_HAS_THREE_WAY_COMPARISON
#include <compare>
#endif

using namespace lumex::core::utility::numeric;

int
main ()
{
  int used = 0;
  int zero = 0;

#if defined(LUMEX_ORDERING_GOOD_CASE)
  // The uses each bad case is the broken version of.
  strong_ordering strong = strong_ordering::less;
  weak_ordering weak = strong_ordering::equal;
  partial_ordering partial = weak_ordering::greater;
  partial = partial_ordering::unordered;
  used += strong < 0 ? 1 : 0;
  used += 0 < strong ? 1 : 0;
  used += weak == 0 ? 1 : 0;
  used += partial != 0 ? 1 : 0;
  used += strong == weak ? 1 : 0;
  used += strong == partial_ordering::less ? 1 : 0;
  used += is_lt (strong) ? 1 : 0;
  used += is_less (weak) ? 1 : 0;
  used += is_equal (partial) ? 1 : 0;
  used += is_not_equal (strong_ordering_t::less) ? 1 : 0;
  used += safe_three_way_compare (1, 2) < 0 ? 1 : 0;
  used += safe_comparator<int> (1).safe_three_way_compare (2.5) < 0 ? 1 : 0;
  used += zero;
#if LUMEX_HAS_THREE_WAY_COMPARISON
  std::strong_ordering const stdStrong = strong;   // to the same category
  std::partial_ordering const stdPartial = strong; // to a weaker one
  strong_ordering const fromStd = std::strong_ordering::greater;
  partial_ordering const widened = std::strong_ordering::less;
  used += stdStrong == std::strong_ordering::less ? 1 : 0;
  used += stdPartial == std::partial_ordering::less ? 1 : 0;
  used += fromStd == std::strong_ordering::greater ? 1 : 0;
  used += std::strong_ordering::greater == fromStd ? 1 : 0;
  used += widened != std::partial_ordering::unordered ? 1 : 0;
  used += safe_three_way_compare (1, 2) == std::strong_ordering::less ? 1 : 0;
#endif
#elif LUMEX_ORDERING_BAD_CASE == 1
  // No default constructor.
  strong_ordering value;
  used += value == 0 ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 2
  // No constructor from a number.
  strong_ordering value (1);
  used += value == 0 ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 3
  // No relational operator between two orderings.
  used += strong_ordering::less < strong_ordering::greater ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 4
  // Only the literal 0 fits, not a variable of value 0.
  used += strong_ordering::less < zero ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 5
  // No conversion from a weaker category to a stronger one.
  weak_ordering value = partial_ordering::less;
  used += value == 0 ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 6
  // No conversion from a weak ordering to a strong one.
  strong_ordering value = weak_ordering::less;
  used += value == 0 ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 7
  // The helpers take an ordering, not a number.
  used += is_less (1) ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 8
  // The same for a floating-point number.
  used += is_equal (1.5) ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 9
  // Only arithmetic types are comparable.
  used += safe_three_way_compare (std::string (), 1) < 0 ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 10
  // The member has the same requirement.
  used += safe_comparator<int> (1).safe_three_way_compare (std::string ()) < 0
              ? 1
              : 0;
#elif LUMEX_ORDERING_BAD_CASE == 11
  // The named values are constants.
  strong_ordering::less = strong_ordering::greater;
#elif LUMEX_ORDERING_BAD_CASE == 12
  // A number other than the literal 0 does not fit.
  used += strong_ordering::less == 1 ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 13
  // Nor does a floating-point zero.
  used += 0.0 < strong_ordering::greater ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE >= 14 && LUMEX_ORDERING_BAD_CASE <= 17          \
    && !LUMEX_HAS_THREE_WAY_COMPARISON
#error "the ordering check needs three-way comparison"
#elif LUMEX_ORDERING_BAD_CASE == 14
  // No conversion to a stronger standard category.
  std::strong_ordering value = weak_ordering::less;
  used += value == 0 ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 15
  // No construction from a weaker standard category.
  strong_ordering value = std::weak_ordering::less;
  used += value == 0 ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 16
  // Not even from a partial one to a weak one.
  weak_ordering value = std::partial_ordering::less;
  used += value == 0 ? 1 : 0;
#elif LUMEX_ORDERING_BAD_CASE == 17
  // No relational operator between an own ordering and a standard one.
  used += strong_ordering::less < std::strong_ordering::greater ? 1 : 0;
#else
#error "LUMEX_ORDERING_BAD_CASE is not one of the cases"
#endif
  return used == -1;
}
