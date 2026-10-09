// Tests of static_pointer_cast, dynamic_pointer_cast, const_pointer_cast and
// reinterpret_pointer_cast ([util.smartptr.shared.cast]): shared ownership
// with a converted stored pointer, the null result of a failed dynamic cast,
// a base at a non-zero offset, and the rvalue forms (which the module has on
// every standard; the standard library has them from C++20).

#include <atomic>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

template <class F>
void
scenario_static_cast (Trace &t)
{
  std::atomic<int> live (0);
  {
    shared_of<F, Animal> animal (new Dog (&live));
    shared_of<F, Dog> dog = F::template static_cast_<Dog> (animal);
    t.note (animal.use_count ());
    t.note (dog->bark);
    t.note_bool (static_cast<void *> (dog.get ())
                 == static_cast<void *> (animal.get ()));
    shared_of<F, Animal> up = F::template static_cast_<Animal> (dog);
    t.note (up.use_count ());
    shared_of<F, void> erased = F::template static_cast_<void> (dog);
    t.note (erased.use_count ());
    shared_of<F, Dog> back = F::template static_cast_<Dog> (erased);
    t.note_bool (back == dog);
    shared_of<F, Dog> from_null
        = F::template static_cast_<Dog> (shared_of<F, Animal> ());
    t.note (from_null.use_count ());
    t.note_bool (from_null.get () == nullptr);
  }
  t.note (live.load ());
}

template <class F>
void
scenario_dynamic_cast (Trace &t)
{
  std::atomic<int> live (0);
  {
    shared_of<F, Animal> dog (new Dog (&live));
    shared_of<F, Animal> bird (new Bird (&live));
    shared_of<F, Dog> ok = F::template dynamic_cast_<Dog> (dog);
    shared_of<F, Dog> failed = F::template dynamic_cast_<Dog> (bird);
    t.note_bool (ok != nullptr);
    t.note_bool (failed == nullptr);
    t.note (dog.use_count ());
    t.note (bird.use_count ());
    t.note (failed.use_count ());
    // Cross cast between unrelated bases of one object.
    shared_of<F, Left> left (new Both ());
    shared_of<F, Right> right = F::template dynamic_cast_<Right> (left);
    t.note_bool (right != nullptr);
    t.note (left.use_count ());
    // Down to a class the object is not.
    shared_of<F, Stranger> stranger
        = F::template dynamic_cast_<Stranger> (left);
    t.note_bool (stranger == nullptr);
    // A null pointer stays null.
    shared_of<F, Dog> from_null
        = F::template dynamic_cast_<Dog> (shared_of<F, Animal> ());
    t.note_bool (from_null == nullptr);
  }
  t.note (live.load ());
}

template <class F>
void
scenario_const_cast (Trace &t)
{
  shared_of<F, int const> constant (new int (4));
  shared_of<F, int> mutable_view = F::template const_cast_<int> (constant);
  t.note (constant.use_count ());
  *mutable_view = 9;
  t.note (*constant);
  shared_of<F, int const> again
      = F::template const_cast_<int const> (mutable_view);
  t.note (again.use_count ());
  t.note_bool (again.get () == constant.get ());
}

template <class F>
void
scenario_reinterpret_cast (Trace &t)
{
  shared_of<F, std::uint32_t> word (new std::uint32_t (0x01020304u));
  shared_of<F, unsigned char> bytes
      = F::template reinterpret_cast_<unsigned char> (word);
  t.note (word.use_count ());
  t.note_bool (static_cast<void *> (bytes.get ())
               == static_cast<void *> (word.get ()));
  unsigned int const first = bytes.get ()[0];
  unsigned int const last = bytes.get ()[3];
  t.note_bool ((first == 4u && last == 1u) || (first == 1u && last == 4u));
  shared_of<F, std::uint32_t> back
      = F::template reinterpret_cast_<std::uint32_t> (bytes);
  t.note_bool (back == word);
}

template <class F>
void
scenario_offsets (Trace &t)
{
  std::atomic<int> live (0);
  {
    shared_of<F, Both> both (new Both (&live));
    shared_of<F, Right> right = F::template static_cast_<Right> (both);
    shared_of<F, Left> left = F::template static_cast_<Left> (both);
    t.note_bool (static_cast<void *> (right.get ())
                 != static_cast<void *> (both.get ()));
    t.note (both.use_count ());
    shared_of<F, Both> down = F::template static_cast_<Both> (right);
    t.note_bool (down == both);
    t.note_bool (down.get () == both.get ());
    right.reset ();
    left.reset ();
    t.note (live.load ());
    down.reset ();
    t.note (live.load ());
    both.reset ();
    t.note (live.load ());
  }
}

TEST (LumexSharedPtrCastsTest, GivenStaticCasts_WhenObserved_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_static_cast);
}

TEST (LumexSharedPtrCastsTest, GivenDynamicCasts_WhenObserved_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_dynamic_cast);
}

TEST (LumexSharedPtrCastsTest, GivenConstCasts_WhenObserved_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_const_cast);
}

TEST (LumexSharedPtrCastsTest,
      GivenReinterpretCasts_WhenObserved_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_reinterpret_cast);
}

TEST (
    LumexSharedPtrCastsTest,
    GivenBasesAtOffsets_WhenCasting_ThenTheBlockIsSharedAndThePointerIsAdjusted)
{
  EXPECT_TRACES_EQUAL (scenario_offsets);
}

TEST (LumexSharedPtrCastsTest,
      GivenRvalueCasts_WhenCasting_ThenTheOwnershipMovesWithoutACount)
{
  std::atomic<int> live (0);
  sp::shared_ptr<Animal> animal (new Dog (&live));
  Animal *const raw = animal.get ();
  sp::shared_ptr<Dog> dog = sp::static_pointer_cast<Dog> (std::move (animal));
  EXPECT_EQ (animal.use_count (), 0);
  EXPECT_EQ (animal.get (), nullptr);
  EXPECT_EQ (dog.use_count (), 1);
  EXPECT_EQ (static_cast<Animal *> (dog.get ()), raw);

  sp::shared_ptr<Animal> again (dog);
  sp::shared_ptr<Dog> checked
      = sp::dynamic_pointer_cast<Dog> (std::move (again));
  EXPECT_EQ (again.use_count (), 0);
  EXPECT_EQ (checked.use_count (), 2);

  // A failed rvalue dynamic cast leaves the source untouched.
  sp::shared_ptr<Animal> bird (new Bird (&live));
  sp::shared_ptr<Dog> not_a_dog
      = sp::dynamic_pointer_cast<Dog> (std::move (bird));
  EXPECT_EQ (not_a_dog.get (), nullptr);
  ASSERT_NE (bird.get (), nullptr);
  EXPECT_EQ (bird.use_count (), 1);
  EXPECT_EQ (bird->legs (), 2);

  sp::shared_ptr<int const> constant (new int (3));
  sp::shared_ptr<int> mutable_view
      = sp::const_pointer_cast<int> (std::move (constant));
  EXPECT_EQ (constant.use_count (), 0);
  EXPECT_EQ (mutable_view.use_count (), 1);

  sp::shared_ptr<std::uint32_t> word (new std::uint32_t (1));
  sp::shared_ptr<unsigned char> bytes
      = sp::reinterpret_pointer_cast<unsigned char> (std::move (word));
  EXPECT_EQ (word.use_count (), 0);
  EXPECT_EQ (bytes.use_count (), 1);
}

TEST (LumexSharedPtrCastsTest,
      GivenTheCasts_WhenCheckingTraits_ThenTheyAreNoexceptAndTyped)
{
  static_assert (
      std::is_same<decltype (sp::static_pointer_cast<Dog> (
                       std::declval<sp::shared_ptr<Animal> const &> ())),
                   sp::shared_ptr<Dog>>::value,
      "");
  static_assert (noexcept (sp::static_pointer_cast<Dog> (
                     std::declval<sp::shared_ptr<Animal> const &> ())),
                 "");
  static_assert (noexcept (sp::dynamic_pointer_cast<Dog> (
                     std::declval<sp::shared_ptr<Animal> const &> ())),
                 "");
  static_assert (noexcept (sp::const_pointer_cast<int> (
                     std::declval<sp::shared_ptr<int const> const &> ())),
                 "");
  static_assert (noexcept (sp::reinterpret_pointer_cast<char> (
                     std::declval<sp::shared_ptr<int> const &> ())),
                 "");
  SUCCEED ();
}
} // namespace
