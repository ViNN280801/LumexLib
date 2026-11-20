// Everything here is allowed: a raw constexpr that is not on a constructor or
// a destructor, and the macros of the special members.
#define LUMEX_CONSTEXPR_CTOR constexpr
#define LUMEX_CONSTEXPR_DTOR constexpr

// constexpr plain () {}  in a line comment
/* constexpr ~plain () {}
   in a block comment */
char const *const text = "constexpr plain () {}";
char const *const raw = R"x(constexpr ~plain () {} )x";
char const digits[] = { '\'', 1'000 };

struct plain
{
  LUMEX_CONSTEXPR_CTOR
  plain () {}
  LUMEX_CONSTEXPR_DTOR ~plain () {}
  LUMEX_CONSTEXPR_CTOR explicit plain (int) {}
  plain (plain const &) = default;
  constexpr plain
  make (int) const
  {
    return plain ();
  }
  constexpr
  operator bool () const
  {
    return true;
  }
  constexpr
  operator() (int) const
  {
    return 0;
  }
  static constexpr int value = 1;
  static constexpr plain (*factory) (int) = nullptr;
  constexpr plain &
  self ()
  {
    return *this;
  }
  constexpr auto
  name () const -> decltype (1)
  {
    return 1;
  }
};

struct limits
{
  static constexpr int (max) () { return 1; }
  LUMEX_CONSTEXPR static long (min) () { return 0; }
  constexpr LUMEX_SIZE_T
  size () const
  {
    return 0;
  }
  static constexpr plain (make2) () { return plain (); }
};
char const quote = u8'a';
constexpr plain after_quote = plain ();

constexpr plain constant = plain ();
constexpr plain other (plain ());
constexpr int
twice (int x)
{
  return 2 * x;
}
inline auto lambda = [] (int x) constexpr { return x; };

template <typename T>
int
pick (T)
{
  if constexpr (sizeof (T) > 1)
    return 1;
  static_assert (sizeof (int) >= 2, "constexpr plain ()");
  return 0;
}
