// Compile checks of the precondition checks of span in constant expressions,
// built by cmake.span_compile_checks. Exactly one of LUMEX_SPAN_GOOD_CASE or
// LUMEX_SPAN_BAD_CASE=<n> is defined. The fixture builds it twice: with
// LUMEX_SPAN_CHECKS=1 every bad case must NOT compile (a violated
// precondition is not a constant expression) and the good case must compile;
// with LUMEX_SPAN_CHECKS=0 every bad case MUST compile (so the check is the
// only reason the first build rejects it: each view is over a larger array,
// so the violating access stays inside it).

#include <cstddef>

#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::span;

namespace
{
constexpr int values[8] = { 10, 11, 12, 13, 14, 15, 16, 17 };
constexpr span<int const> four (values, 4);
constexpr span<int const> middle (values + 2, 4);
constexpr span<int const> none (values + 1, 0);
constexpr span<int const> three (values, 3);
} // namespace

// Subviews of a count that makes the check fold to a constant must not drop
// the call of size () under -Werror=unused-result (GCC 8 warned about a
// nodiscard call in a comparison that folds).
unsigned long
fold_checks (span<int> dynamic, span<int, 4> fixed)
{
  return dynamic.first<0> ().size () + dynamic.last<0> ().size ()
         + dynamic.subspan<0> ().size () + dynamic.subspan<0, 0> ().size ()
         + dynamic.subspan<1, 0> ().size () + dynamic.subspan<0, 2> ().size ()
         + fixed.first<0> ().size () + fixed.last<0> ().size ()
         + fixed.subspan<0> ().size () + fixed.subspan<4> ().size ();
}

#if defined(LUMEX_SPAN_GOOD_CASE)
static_assert (four[3] == 13 && four.front () == 10 && four.back () == 13,
               "the borders");
static_assert (four.first (4).size () == 4 && four.last (4).size () == 4
                   && four.subspan (4).size () == 0
                   && four.subspan (1, 3).size () == 3,
               "the borders of the subviews");
static_assert (four.first<4> ().size () == 4 && four.last<4> ().size () == 4
                   && four.subspan<2, 2> ().size () == 2,
               "the static subviews");
static_assert (span<int const, 4> (values, 4).size () == 4
                   && span<int const, 4> (values, values + 4).size () == 4
                   && span<int const, 4> (four).size () == 4
                   && span<int const> (values + 4, values + 4).size () == 0,
               "the constructors");
#elif LUMEX_SPAN_BAD_CASE == 1
constexpr int result = four[5];
#elif LUMEX_SPAN_BAD_CASE == 2
constexpr int result = none.front ();
#elif LUMEX_SPAN_BAD_CASE == 3
constexpr int result = none.back ();
#elif LUMEX_SPAN_BAD_CASE == 4
constexpr span<int const> result = four.first (5);
#elif LUMEX_SPAN_BAD_CASE == 5
constexpr span<int const> result = middle.last (5);
#elif LUMEX_SPAN_BAD_CASE == 6
constexpr span<int const> result = four.subspan (5);
#elif LUMEX_SPAN_BAD_CASE == 7
constexpr span<int const> result = four.subspan (1, 4);
#elif LUMEX_SPAN_BAD_CASE == 8
constexpr span<int const, 5> result = four.first<5> ();
#elif LUMEX_SPAN_BAD_CASE == 9
constexpr span<int const, 5> result = middle.last<5> ();
#elif LUMEX_SPAN_BAD_CASE == 10
constexpr span<int const, 3> result = four.subspan<2, 3> ();
#elif LUMEX_SPAN_BAD_CASE == 11
constexpr span<int const, 4> result (values, 5);
#elif LUMEX_SPAN_BAD_CASE == 12
constexpr span<int const, 4> result (values, values + 5);
#elif LUMEX_SPAN_BAD_CASE == 13
constexpr span<int const> result (values + 4, values + 3);
#elif LUMEX_SPAN_BAD_CASE == 14
constexpr span<int const, 4> result (three);
#else
#error "LUMEX_SPAN_BAD_CASE is not one of the cases of check_constexpr.cpp"
#endif

int
main ()
{
  int storage[4] = { 1, 2, 3, 4 };
  return fold_checks (span<int> (storage, 4), span<int, 4> (storage, 4)) == 0
             ? 1
             : 0;
}
