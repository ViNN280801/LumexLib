// Compile checks of hazard_pointer, built by
// cmake.hazard_pointer_compile_checks. Exactly one of LUMEX_HP_GOOD_CASE or
// LUMEX_HP_BAD_CASE=<n> is defined; the fixture compiles with warnings as
// errors. The preamble is valid code; a bad case adds one statement that the
// interface of [saferecl.hp] rejects.

#include <atomic>
#include <memory>
#include <vector>

#include "lumex/core/hazard_pointer/LumexHazardPointer"

namespace hp = lumex::core::hazard_pointer;

namespace
{
struct good : hp::hazard_pointer_obj_base<good>
{
  int value;
};

struct plain
{
  int value;
};

// Retire on this one hits the Mandates of retire: the object type its base
// names is not hazard-protectable.
struct wrong_object : hp::hazard_pointer_obj_base<plain>
{
};

struct private_base : private hp::hazard_pointer_obj_base<private_base>
{
};

struct virtual_base : virtual hp::hazard_pointer_obj_base<virtual_base>
{
};

struct first_deleter
{
  void
  operator() (struct two_bases *) const
  {
  }
};

struct second_deleter
{
  void
  operator() (struct two_bases *) const
  {
  }
};

struct two_bases : hp::hazard_pointer_obj_base<two_bases, first_deleter>,
                   hp::hazard_pointer_obj_base<two_bases, second_deleter>
{
};

struct exposes_base : hp::hazard_pointer_obj_base<exposes_base>
{
  // The base constructor is protected: only a derived class makes one.
  exposes_base () : hp::hazard_pointer_obj_base<exposes_base> () {}
};

void
use ()
{
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  good object;
  std::atomic<good *> source (&object);
  good *ptr = holder.protect (source);
  if (holder.try_protect (ptr, source))
    {
      holder.reset_protection (ptr);
    }
  holder.reset_protection ();
  hp::hazard_pointer other (std::move (holder));
  hp::swap (holder, other);
  if (holder.empty ())
    {
      hp::hazard_pointer hps[2];
      hp::make_hazard_pointer_batch (
          lumex::core::span::view::span<hp::hazard_pointer> (hps, 2));
      hp::clear_hazard_pointer_batch (
          lumex::core::span::view::span<hp::hazard_pointer> (hps, 2));
    }
  object.retire ();
  exposes_base e;
  (void)e;
}

#if defined(LUMEX_HP_BAD_CASE)
void
bad ()
{
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  good object;
  std::atomic<good *> source (&object);
  good *ptr = &object;
  plain plain_object;
  std::atomic<plain *> plain_source (&plain_object);
  plain *plain_ptr = &plain_object;
  std::atomic<private_base *> private_source (nullptr);
  private_base *private_ptr = nullptr;
  std::atomic<virtual_base *> virtual_source (nullptr);
  std::atomic<two_bases *> two_source (nullptr);
  two_bases *two_ptr = nullptr;
  std::atomic<int *> int_source (nullptr);
  std::atomic<wrong_object *> wrong_source (nullptr);
  wrong_object wrong;
  std::vector<hp::hazard_pointer> const holders (1);
  (void)ptr;
  (void)source;
  (void)plain_ptr;
  (void)plain_source;
  (void)private_ptr;
  (void)private_source;
  (void)virtual_source;
  (void)two_ptr;
  (void)two_source;
  (void)int_source;
  (void)wrong_source;
  (void)wrong;
  (void)holders;
#if LUMEX_HP_BAD_CASE == 1
  // No hazard_pointer_obj_base base.
  (void)holder.protect (plain_source);
#elif LUMEX_HP_BAD_CASE == 2
  (void)holder.try_protect (plain_ptr, plain_source);
#elif LUMEX_HP_BAD_CASE == 3
  holder.reset_protection (plain_ptr);
#elif LUMEX_HP_BAD_CASE == 4
  // A private base is not a protectable base.
  (void)holder.protect (private_source);
#elif LUMEX_HP_BAD_CASE == 5
  (void)holder.protect (virtual_source);
#elif LUMEX_HP_BAD_CASE == 6
  (void)holder.protect (two_source);
#elif LUMEX_HP_BAD_CASE == 7
  // Not a class at all.
  (void)holder.protect (int_source);
#elif LUMEX_HP_BAD_CASE == 8
  // The base names another object type: retire's Mandates fails.
  wrong.retire ();
#elif LUMEX_HP_BAD_CASE == 9
  // A holder is not copyable.
  hp::hazard_pointer copy = holder;
  (void)copy;
#elif LUMEX_HP_BAD_CASE == 10
  hp::hazard_pointer copy;
  copy = holder;
#elif LUMEX_HP_BAD_CASE == 11
  // Discarded results (nodiscard).
  holder.protect (source);
#elif LUMEX_HP_BAD_CASE == 12
  holder.try_protect (ptr, source);
#elif LUMEX_HP_BAD_CASE == 13
  holder.empty ();
#elif LUMEX_HP_BAD_CASE == 14
  // The base is not constructible or destructible by users.
  hp::hazard_pointer_obj_base<good> base;
  (void)base;
#elif LUMEX_HP_BAD_CASE == 15
  good *heap = new good;
  delete static_cast<hp::hazard_pointer_obj_base<good> *> (heap);
#elif LUMEX_HP_BAD_CASE == 16
  // A deleter that is not the deleter type.
  object.retire (5);
#elif LUMEX_HP_BAD_CASE == 17
  // A holder is not made from a null pointer.
  hp::hazard_pointer from_null = nullptr;
  (void)from_null;
#elif LUMEX_HP_BAD_CASE == 18
  // The batch functions take holders, not a read-only span.
  hp::make_hazard_pointer_batch (
      lumex::core::span::view::span<hp::hazard_pointer const> (
          holders.data (), holders.size ()));
#elif LUMEX_HP_BAD_CASE == 19
  int numbers[2] = { 1, 2 };
  hp::clear_hazard_pointer_batch (
      lumex::core::span::view::span<int> (numbers, 2));
#elif LUMEX_HP_BAD_CASE == 20
  // The pointer type of try_protect must match the source.
  good *base_ptr = &object;
  std::atomic<good const *> const_source (nullptr);
  (void)holder.try_protect (base_ptr, const_source);
#elif LUMEX_HP_BAD_CASE == 21
  // Swapping with something that is not a holder.
  int number = 0;
  holder.swap (number);
#elif LUMEX_HP_BAD_CASE == 22
  // retire needs a non-const object.
  good const constant;
  constant.retire ();
#else
#error LUMEX_HP_BAD_CASE is not one of the cases
#endif
}
#endif
} // namespace

int
main ()
{
#if defined(LUMEX_HP_GOOD_CASE)
  use ();
#elif defined(LUMEX_HP_BAD_CASE)
  bad ();
  use ();
#else
#error define LUMEX_HP_GOOD_CASE or LUMEX_HP_BAD_CASE
#endif
  return 0;
}
