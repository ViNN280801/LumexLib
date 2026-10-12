// Compile checks of the smart pointers, built by
// cmake.smart_ptr_compile_checks. Exactly one of LUMEX_SP_GOOD_CASE or
// LUMEX_SP_BAD_CASE=<n> is defined; the fixture compiles with warnings as
// errors. Every bad case is a use that the interface rejects (a constraint of
// a constructor, a deleted or missing conversion, an implicit conversion to or
// from the standard class, a static_assert about enable_shared_from_this or
// the array forms of make_shared), so the check that should reject it must be
// there at every standard. LUMEX_SP_ALLOW_CASE (with
// LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS) must compile too.

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

#include "lumex/core/smart_ptr/LumexSmartPtr"

namespace sp = lumex::core::smart_ptr;

namespace
{
class Animal
{
public:
  virtual ~Animal () {}
};

class Dog : public Animal
{
};

class Plain
{
};

class StdOnly : public std::enable_shared_from_this<StdOnly>
{
};

class OwnBase : public sp::enable_shared_from_this<OwnBase>
{
};

class BothBases : public sp::enable_shared_from_this<BothBases>,
                  public std::enable_shared_from_this<BothBases>
{
};

void
count_deleter (int *p)
{
  delete p;
}

template <class T>
void
sink (T const &)
{
}
} // namespace

int
main ()
{
#if defined(LUMEX_SP_GOOD_CASE)
  // Everything below is valid and must compile.
  sp::shared_ptr<int> a (new int (1));
  sp::shared_ptr<int const> b = a;
  sp::shared_ptr<void> c = a;
  sp::shared_ptr<Animal> d (new Dog ());
  sp::shared_ptr<Dog> e = sp::static_pointer_cast<Dog> (d);
  sp::shared_ptr<Dog> f = sp::dynamic_pointer_cast<Dog> (d);
  sp::shared_ptr<int> g = sp::const_pointer_cast<int> (b);
  sp::shared_ptr<char> h = sp::reinterpret_pointer_cast<char> (a);
  sp::shared_ptr<int> i (new int (2), &count_deleter);
  sp::shared_ptr<int> j (nullptr, &count_deleter);
  sp::weak_ptr<int> w (a);
  sp::shared_ptr<int> k (w);
  sp::shared_ptr<int[]> l (new int[3]);
  sp::shared_ptr<int[3]> m (new int[3]);
  sp::shared_ptr<int[]> n = m;
  sp::shared_ptr<OwnBase> o = sp::make_shared<OwnBase> ();
  sp::shared_ptr<BothBases> q (new BothBases ());
  sp::shared_ptr<int> r = sp::make_shared<int> (3);
  sp::shared_ptr<int> s = sp::allocate_shared<int> (std::allocator<int> (), 4);
  std::shared_ptr<int> standard = std::make_shared<int> (5);
  sp::shared_ptr<int> t = sp::from_std (standard);
  std::shared_ptr<int> u = sp::to_std (t);
  std::unique_ptr<int> v (new int (6));
  sp::shared_ptr<int> x (std::move (v));
  sink (*a);
  sink (l[1]);
  sink (std::hash<sp::shared_ptr<int>> () (a));
  sink (sp::owner_less<sp::shared_ptr<int>> () (a, k));
  sink (a == nullptr);
  sink (a == b);
  return 0;
#elif defined(LUMEX_SP_ALLOW_CASE)
  // With LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS the class that has
  // only the standard base is accepted (the standard base stays unset).
  sp::shared_ptr<StdOnly> p (new StdOnly ());
  sp::shared_ptr<StdOnly> q = sp::make_shared<StdOnly> ();
  return p.use_count () == 1 && q.use_count () == 1 ? 0 : 1;
#elif LUMEX_SP_BAD_CASE == 1
  // A pointer of another type.
  sp::shared_ptr<int> p (new double (1.0));
#elif LUMEX_SP_BAD_CASE == 2
  // A base cannot become a derived class implicitly.
  sp::shared_ptr<Animal> base (new Dog ());
  sp::shared_ptr<Dog> derived = base;
#elif LUMEX_SP_BAD_CASE == 3
  // The pointer constructor is explicit.
  sp::shared_ptr<int> p = new int (1);
#elif LUMEX_SP_BAD_CASE == 4
  // No implicit conversion from the standard class.
  std::shared_ptr<int> standard = std::make_shared<int> (1);
  sp::shared_ptr<int> p = standard;
#elif LUMEX_SP_BAD_CASE == 5
  // No implicit conversion to the standard class.
  sp::shared_ptr<int> p (new int (1));
  std::shared_ptr<int> standard = p;
#elif LUMEX_SP_BAD_CASE == 6
  // A class with only the standard enable_shared_from_this base.
  sp::shared_ptr<StdOnly> p (new StdOnly ());
#elif LUMEX_SP_BAD_CASE == 7
  // The same through make_shared.
  sp::shared_ptr<StdOnly> p = sp::make_shared<StdOnly> ();
#elif LUMEX_SP_BAD_CASE == 8
  // The array forms of make_shared are C++20 and not provided.
  sp::shared_ptr<int[]> p = sp::make_shared<int[]> (3);
#elif LUMEX_SP_BAD_CASE == 9
  // The fixed-size array form too.
  sp::shared_ptr<int[3]> p = sp::make_shared<int[3]> ();
#elif LUMEX_SP_BAD_CASE == 10
  // A unique_ptr is taken by rvalue only.
  std::unique_ptr<int> u (new int (1));
  sp::shared_ptr<int> p (u);
#elif LUMEX_SP_BAD_CASE == 11
  // A deleter must be callable with the pointer.
  sp::shared_ptr<int> p (new int (1), 5);
#elif LUMEX_SP_BAD_CASE == 12
  // The deleter of the null form must accept the null pointer type.
  sp::shared_ptr<int> p (nullptr, 5);
#elif LUMEX_SP_BAD_CASE == 13
  // shared_ptr<void> cannot be dereferenced.
  sp::shared_ptr<void> p (new int (1));
  sink (*p);
#elif LUMEX_SP_BAD_CASE == 14
  // Neither can an array pointer.
  sp::shared_ptr<int[]> p (new int[2]);
  sink (*p);
#elif LUMEX_SP_BAD_CASE == 15
  // An object pointer has no operator[].
  sp::shared_ptr<int> p (new int (1));
  sink (p[0]);
#elif LUMEX_SP_BAD_CASE == 16
  // A weak pointer cannot be dereferenced.
  sp::shared_ptr<int> p (new int (1));
  sp::weak_ptr<int> w (p);
  sink (*w);
#elif LUMEX_SP_BAD_CASE == 17
  // A weak pointer of another type does not promote.
  sp::weak_ptr<double> w;
  sp::shared_ptr<int> p (w);
#elif LUMEX_SP_BAD_CASE == 18
  // Removing const needs const_pointer_cast.
  sp::shared_ptr<int const> p (new int (1));
  sp::shared_ptr<int> q = p;
#elif LUMEX_SP_BAD_CASE == 19
  // Arrays of derived objects do not convert to arrays of base objects.
  sp::shared_ptr<Animal[]> p (new Dog[2]);
#elif LUMEX_SP_BAD_CASE == 20
  // dynamic_pointer_cast needs a polymorphic source.
  sp::shared_ptr<Plain> p (new Plain ());
  sp::shared_ptr<Dog> q = sp::dynamic_pointer_cast<Dog> (p);
#elif LUMEX_SP_BAD_CASE == 21
  // static_pointer_cast between unrelated classes.
  sp::shared_ptr<Dog> p (new Dog ());
  sp::shared_ptr<Plain> q = sp::static_pointer_cast<Plain> (p);
#elif LUMEX_SP_BAD_CASE == 22
  // const_pointer_cast cannot change the type.
  sp::shared_ptr<double> p (new double (1.0));
  sp::shared_ptr<int> q = sp::const_pointer_cast<int> (p);
#elif LUMEX_SP_BAD_CASE == 23
  // reinterpret_pointer_cast cannot cast away const.
  sp::shared_ptr<int const> p (new int (1));
  sp::shared_ptr<char> q = sp::reinterpret_pointer_cast<char> (p);
#elif LUMEX_SP_BAD_CASE == 24
  // There is no hash of a weak pointer.
  sp::shared_ptr<int> p (new int (1));
  sp::weak_ptr<int> w (p);
  sink (std::hash<sp::weak_ptr<int>> () (w));
#elif LUMEX_SP_BAD_CASE == 25
  // unique () was removed in C++20 and the module never had it.
  sp::shared_ptr<int> p (new int (1));
  sink (p.unique ());
#elif LUMEX_SP_BAD_CASE == 26
  // owner_less is defined for the pointer types only.
  sp::owner_less<int> order;
  sink (&order);
#elif LUMEX_SP_BAD_CASE == 27
  // The explicit conversion functions are the only way across: no overload
  // from a weak pointer.
  std::weak_ptr<int> weak;
  sp::weak_ptr<int> w (weak);
#elif LUMEX_SP_BAD_CASE == 28
  // A unique_ptr of another type does not convert.
  sp::shared_ptr<int> p (std::unique_ptr<double> (new double (1.0)));
#elif LUMEX_SP_BAD_CASE == 29
  // An unrelated pointer type in the aliasing position is a different overload
  // set: the stored pointer must be convertible (here: a missing conversion).
  sp::shared_ptr<int> owner (new int (1));
  sp::shared_ptr<int> alias (owner, new double (1.0));
#elif LUMEX_SP_BAD_CASE == 30
  // Comparing with a plain integer other than a null constant.
  sp::shared_ptr<int> p (new int (1));
  sink (p == 5);
#elif LUMEX_SP_BAD_CASE == 31
  // get_deleter needs the deleter type explicitly.
  sp::shared_ptr<int> p (new int (1));
  sink (sp::get_deleter (p));
#else
#error "LUMEX_SP_BAD_CASE is not one of the cases"
#endif
  return 0;
}
