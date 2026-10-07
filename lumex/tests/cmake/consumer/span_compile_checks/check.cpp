// Compile checks of span, built by cmake.span_compile_checks. Exactly one of
// LUMEX_SPAN_GOOD_CASE or LUMEX_SPAN_BAD_CASE=<n> is defined; the fixture
// compiles with warnings as errors. Every bad case is a use the standard
// std::span rejects (a constraint of a constructor, a static_assert of a
// member, a deleted conversion, a discarded result), so the check that should
// reject it must be there at every standard.

#include <array>
#include <cstddef>
#include <deque>
#include <initializer_list>
#include <list>
#include <utility>
#include <vector>

#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::as_bytes;
using lumex::core::span::view::as_writable_bytes;
using lumex::core::span::view::span;

namespace
{
struct base_type
{
  int value;
};

struct derived_type : base_type
{
  int extra;
};

int
take_four (span<int, 4> view)
{
  return static_cast<int> (view.size ());
}

int
take_dynamic (span<int> view)
{
  return static_cast<int> (view.size ());
}

int
take_const_dynamic (span<int const> view)
{
  return static_cast<int> (view.size ());
}
} // namespace

int
main ()
{
  int values[5] = { 1, 2, 3, 4, 5 };
  int const read_only[5] = { 1, 2, 3, 4, 5 };
  std::array<int, 3> array_values = { { 1, 2, 3 } };
  std::array<int, 3> const const_array_values = { { 1, 2, 3 } };
  std::vector<int> vector_values = { 1, 2, 3 };
  std::vector<bool> flag_values (3, true);
  std::deque<int> deque_values (3, 1);
  std::list<int> list_values (3, 1);
  derived_type derived[2] = {};
  span<int> const dynamic (values);
  span<int, 5> const fixed (values);
  int used = 0;

#if defined(LUMEX_SPAN_GOOD_CASE)
  // The uses each bad case is the broken version of.
  used += take_four (span<int, 4> (values, 4));
  used += take_dynamic (values);
  used += take_dynamic (vector_values);
  used += take_dynamic (array_values);
  used += take_const_dynamic (read_only);
  used += take_const_dynamic (const_array_values);
  used += take_const_dynamic (std::vector<int>{ 1, 2 });
  used += take_const_dynamic (std::initializer_list<int>{ 1, 2, 3 });
  used += take_dynamic (fixed);
  used += static_cast<int> (span<int, 5> (dynamic).size ());
  used += static_cast<int> (fixed.first<5> ().size ());
  used += static_cast<int> (fixed.last<5> ().size ());
  used += static_cast<int> (fixed.subspan<2, 3> ().size ());
  used += static_cast<int> (fixed.subspan<5> ().size ());
  used += static_cast<int> (dynamic.first<9> ().size () > 0 ? 0 : 1);
  used += static_cast<int> (as_bytes (fixed).size ());
  used += static_cast<int> (as_writable_bytes (dynamic).size ());
  used += static_cast<int> (span<derived_type> (derived, 2).size ());
  span<int const> constant (values);
  used += constant.size () > 0 ? 1 : 0;
  used += dynamic.empty () ? 0 : 1;
  used += dynamic.at (1);
  (void)flag_values;
  (void)deque_values;
  (void)list_values;
#elif LUMEX_SPAN_BAD_CASE == 1
  // A discarded size () is a warning, an error with -Werror.
  dynamic.size ();
#elif LUMEX_SPAN_BAD_CASE == 2
  // A discarded empty ().
  dynamic.empty ();
#elif LUMEX_SPAN_BAD_CASE == 3
  // A discarded at ().
  dynamic.at (0);
#elif LUMEX_SPAN_BAD_CASE == 4
  // A discarded subview.
  dynamic.first (1);
#elif LUMEX_SPAN_BAD_CASE == 5
  // A span of const elements from a pointer to const: no non-const span.
  span<int> view (read_only, 5);
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 6
  // A static extent has no default constructor.
  span<int, 3> view;
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 7
  // A deque is not contiguous.
  span<int> view (deque_values);
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 8
  // A std::vector<bool> is not contiguous.
  span<bool> view (flag_values);
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 9
  // An array of 5 does not fit a static extent of 4.
  span<int, 4> view (values);
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 10
  // first<Count> of a static extent with Count out of range.
  used += static_cast<int> (fixed.first<6> ().size ());
#elif LUMEX_SPAN_BAD_CASE == 11
  // last<Count> of a static extent with Count out of range.
  used += static_cast<int> (fixed.last<6> ().size ());
#elif LUMEX_SPAN_BAD_CASE == 12
  // subspan<Offset> of a static extent with Offset out of range.
  used += static_cast<int> (fixed.subspan<6> ().size ());
#elif LUMEX_SPAN_BAD_CASE == 13
  // subspan<Offset, Count> with Offset + Count out of range.
  used += static_cast<int> (fixed.subspan<2, 4> ().size ());
#elif LUMEX_SPAN_BAD_CASE == 14
  // Dynamic to static is explicit.
  span<int, 5> view = dynamic;
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 15
  // A static extent from a range is explicit.
  span<int, 3> view = vector_values;
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 16
  // A static extent from a pointer and a count is explicit.
  span<int, 2> view = { values, 2 };
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 17
  // A static extent from a pointer pair is explicit.
  span<int, 2> view = { values, values + 2 };
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 18
  // No writable byte view of const elements.
  span<int const> view (read_only);
  used += static_cast<int> (as_writable_bytes (view).size ());
#elif LUMEX_SPAN_BAD_CASE == 19
  // An array of Derived is not an array of Base.
  span<base_type> view (derived, 2);
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 20
  // A temporary vector may not give a mutable span.
  span<int> view (std::vector<int>{ 1, 2 });
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 21
  // The elements of a span of const are read-only.
  span<int const> view (values);
  view[0] = 1;
#elif LUMEX_SPAN_BAD_CASE == 22
  // A span of void.
  span<void> view;
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 23
  // A span of references.
  span<int &> view;
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 24
  // A span of a function type is not a span of objects.
  span<int (int)> view;
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 25
  // A const std::array gives no mutable span.
  span<int> view (const_array_values);
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 26
  // A list is not contiguous.
  span<int> view (list_values);
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 27
  // A volatile element has no byte view.
  int volatile guard[2] = { 1, 2 };
  span<int volatile> view (guard);
  used += static_cast<int> (as_bytes (view).size ());
#elif LUMEX_SPAN_BAD_CASE == 28
  // A span of 3 does not pass as a span of 4.
  span<int, 3> three (values, 3);
  used += take_four (three);
#elif LUMEX_SPAN_BAD_CASE == 29
  // A braced list is not a span, and never a mutable one.
  used += take_dynamic ({ 1, 2, 3 });
#elif LUMEX_SPAN_BAD_CASE == 30
  // const must not be dropped by the span conversion.
  span<int const> view (values);
  used += take_dynamic (view);
#elif LUMEX_SPAN_BAD_CASE == 31
  // A pointer alone is not a range.
  span<int> view (values + 1);
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 32
  // A rvalue array is not viewed mutably.
  span<int> view (std::move (values));
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 33
  // Spans of unrelated element types do not convert.
  span<char> view (dynamic);
  used += static_cast<int> (view.size ());
#elif LUMEX_SPAN_BAD_CASE == 34
  // A braced list is not a span either, as the constructor of C++26 is not
  // provided (it would make f ({1, 2}) ambiguous next to a std::vector one).
  used += take_const_dynamic ({ 1, 2, 3 });
#else
#error "LUMEX_SPAN_BAD_CASE is not one of the cases of check.cpp"
#endif
  return used == 12345 ? 1 : 0;
}
