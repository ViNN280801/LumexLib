// 16 hand-written constexpr special members, one per case the checker must
// see (the test pins the line of each). clang-format stays off: the layout of
// a case is the case.
// clang-format off
struct plain
{
  constexpr plain () {}                      // raw, constructor
  constexpr ~plain () {}                     // raw, destructor
  explicit constexpr plain (int) {}          // raw after `explicit`
  constexpr explicit plain (long) {}         // raw before `explicit`
  constexpr plain (plain const &) = default; // raw, defaulted
  LUMEX_CONSTEXPR plain (char) {}            // the macro of a function
  constexpr
  plain (short) {}                           // the name on the next line
  constexpr LUMEX_ATTRIBUTE_ALWAYS_INLINE plain (unsigned) {} // attribute
  LUMEX_CONSTEXPR_FUNCTION ~plain (int) {}   // function macro, destructor
};

template <typename T> struct tpl
{
  tpl ();
};
template <typename T> constexpr tpl<T>::tpl () {} // out of line

#define MAKE_CTOR(name) constexpr name () {}   // in a macro body

struct second
{
  constexpr explicit (true) second (int, int) {} // explicit (condition)
  constexpr __forceinline second (long) {}       // a MSVC keyword between
  consteval second (double) {}                   // consteval
  LUMEX_CONSTEXPR_CXX14 ~second () {}            // C++14 macro on a destructor
};
char const letter = u8'a'; constexpr second::second (char) {} // after u8'a'
// clang-format on
