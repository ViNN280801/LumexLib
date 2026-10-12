/**
 * @file LumexSmartPtrTestSupport.hpp
 * @brief Shared fixtures of the smart pointer tests.
 * @details This is the only place that names the module's namespace
 * (`sp`) and the standard twin (`std`) of every class under test, so a
 * differential scenario is written once as a template on a "family" and run
 * on both:
 *
 * - `std_family` and `lumex_family` name `shared`, `weak`,
 *   `enable_from_this`, `make`, `allocate`, the four casts and the exception
 *   type of each side.
 * - `Trace` collects what a scenario observed (counts, booleans, order of
 *   events) as a list of numbers; two traces from the two families must be
 *   equal (`EXPECT_TRACES_EQUAL`).
 * - The test types: an object that registers in an `ObjectLedger`, a
 *   polymorphic hierarchy with a second base at a non-zero offset (so
 *   conversions change the pointer value), a type that needs 64-byte
 *   alignment, deleters that count, allocators that count allocations,
 *   constructions and destructions and that can be told to fail.
 */
#ifndef LUMEX_TESTS_CORE_SMART_PTR_SMART_PTR_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_SMART_PTR_SMART_PTR_TEST_SUPPORT_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/smart_ptr/LumexSmartPtr"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/tests/support/LumexTestConfig.hpp"
#include "lumex/tests/support/LumexTestLedger.hpp"

namespace smart_ptr_test
{
/// The module under test (the one place that names it).
namespace sp = lumex::core::smart_ptr;

// --- The two families of a differential scenario ----------------------------

/// The standard library side.
struct std_family
{
  template <class T> using shared = std::shared_ptr<T>;
  template <class T> using weak = std::weak_ptr<T>;
  template <class T> using enable_from_this = std::enable_shared_from_this<T>;
  typedef std::bad_weak_ptr bad_weak;

  static char const *
  name ()
  {
    return "std";
  }

  template <class T, class... Args>
  static shared<T>
  make (Args &&...args)
  {
    return std::make_shared<T> (std::forward<Args> (args)...);
  }

  template <class T, class A, class... Args>
  static shared<T>
  allocate (A const &alloc, Args &&...args)
  {
    return std::allocate_shared<T> (alloc, std::forward<Args> (args)...);
  }

  template <class T, class U>
  static shared<T>
  static_cast_ (shared<U> const &p)
  {
    return std::static_pointer_cast<T> (p);
  }

  template <class T, class U>
  static shared<T>
  dynamic_cast_ (shared<U> const &p)
  {
    return std::dynamic_pointer_cast<T> (p);
  }

  template <class T, class U>
  static shared<T>
  const_cast_ (shared<U> const &p)
  {
    return std::const_pointer_cast<T> (p);
  }

  template <class T, class U>
  static shared<T>
  reinterpret_cast_ (shared<U> const &p)
  {
    // std::reinterpret_pointer_cast is C++17; the aliasing constructor is the
    // specified equivalent and works on every standard.
    return shared<T> (
        p, reinterpret_cast<typename shared<T>::element_type *> (p.get ()));
  }

  template <class D, class T>
  static D *
  get_deleter (shared<T> const &p)
  {
    return std::get_deleter<D> (p);
  }

  /// A weak reference to @p object (empty when it is not owned). C++17 has
  /// weak_from_this; before it the weak pointer comes from shared_from_this.
  template <class T>
  static weak<T>
  weak_from (T &object)
  {
#if __cplusplus >= 201703L
    return object.weak_from_this ();
#else
    try
      {
        return weak<T> (object.shared_from_this ());
      }
    catch (std::bad_weak_ptr const &)
      {
        return weak<T> ();
      }
#endif
  }
};

/// The module's side.
struct lumex_family
{
  template <class T> using shared = sp::shared_ptr<T>;
  template <class T> using weak = sp::weak_ptr<T>;
  template <class T> using enable_from_this = sp::enable_shared_from_this<T>;
  typedef sp::bad_weak_ptr bad_weak;

  static char const *
  name ()
  {
    return "lumex";
  }

  template <class T, class... Args>
  static shared<T>
  make (Args &&...args)
  {
    return sp::make_shared<T> (std::forward<Args> (args)...);
  }

  template <class T, class A, class... Args>
  static shared<T>
  allocate (A const &alloc, Args &&...args)
  {
    return sp::allocate_shared<T> (alloc, std::forward<Args> (args)...);
  }

  template <class T, class U>
  static shared<T>
  static_cast_ (shared<U> const &p)
  {
    return sp::static_pointer_cast<T> (p);
  }

  template <class T, class U>
  static shared<T>
  dynamic_cast_ (shared<U> const &p)
  {
    return sp::dynamic_pointer_cast<T> (p);
  }

  template <class T, class U>
  static shared<T>
  const_cast_ (shared<U> const &p)
  {
    return sp::const_pointer_cast<T> (p);
  }

  template <class T, class U>
  static shared<T>
  reinterpret_cast_ (shared<U> const &p)
  {
    return sp::reinterpret_pointer_cast<T> (p);
  }

  template <class D, class T>
  static D *
  get_deleter (shared<T> const &p)
  {
    return sp::get_deleter<D> (p);
  }

  /// A weak reference to @p object (empty when it is not owned).
  template <class T>
  static weak<T>
  weak_from (T &object)
  {
    return object.weak_from_this ();
  }
};

/// `shared<T>` / `weak<T>` of a family, spelled short inside scenarios.
template <class F, class T> using shared_of = typename F::template shared<T>;
template <class F, class T> using weak_of = typename F::template weak<T>;

// --- What a scenario observed ----------------------------------------------

/// A list of numbers a scenario writes down; equal scenarios give equal lists.
class Trace
{
public:
  /// Appends a number.
  void
  note (long value)
  {
    values_.push_back (value);
  }

  /// Appends a boolean as 0 or 1.
  void
  note_bool (bool value)
  {
    values_.push_back (value ? 1 : 0);
  }

  /// The numbers so far.
  std::vector<long> const &
  values () const
  {
    return values_;
  }

  bool
  operator== (Trace const &other) const
  {
    return values_ == other.values_;
  }

  /// "1 2 3" for a failure message.
  std::string
  text () const
  {
    std::ostringstream out;
    for (std::size_t i = 0; i < values_.size (); ++i)
      out << (i == 0 ? "" : " ") << values_[i];
    return out.str ();
  }

private:
  std::vector<long> values_;
};

/// Runs `Scenario<Family> ()` on both families and compares the traces.
#define EXPECT_TRACES_EQUAL(scenario)                                         \
  do                                                                          \
    {                                                                         \
      smart_ptr_test::Trace std_trace;                                        \
      smart_ptr_test::Trace lumex_trace;                                      \
      scenario<smart_ptr_test::std_family> (std_trace);                       \
      scenario<smart_ptr_test::lumex_family> (lumex_trace);                   \
      EXPECT_TRUE (std_trace == lumex_trace)                                  \
          << #scenario << "\n  std:   " << std_trace.text ()                  \
          << "\n  lumex: " << lumex_trace.text ();                            \
    }                                                                         \
  while (false)

// --- Test types
// --------------------------------------------------------------

/// An object that registers in a ledger; `value` is free for the test.
class Probe
{
public:
  explicit Probe (lumex_test::ObjectLedger &ledger, int initial = 0)
      : value (initial), entry_ (ledger)
  {
  }

  /// True while the object is alive and its memory is intact.
  bool
  intact () const
  {
    return entry_.intact ();
  }

  int value;

private:
  lumex_test::LedgerEntry entry_;
};

/// A polymorphic base that counts its live objects in a shared counter.
class Animal
{
public:
  explicit Animal (std::atomic<int> *live = nullptr) : live_ (live)
  {
    if (live_ != nullptr)
      live_->fetch_add (1);
  }

  Animal (Animal const &other) : live_ (other.live_)
  {
    if (live_ != nullptr)
      live_->fetch_add (1);
  }

  virtual ~Animal ()
  {
    if (live_ != nullptr)
      live_->fetch_sub (1);
  }

  virtual int
  legs () const
  {
    return 0;
  }

private:
  std::atomic<int> *live_;
};

class Dog : public Animal
{
public:
  explicit Dog (std::atomic<int> *live = nullptr) : Animal (live) {}
  int
  legs () const override
  {
    return 4;
  }
  int bark = 7;
};

class Bird : public Animal
{
public:
  explicit Bird (std::atomic<int> *live = nullptr) : Animal (live) {}
  int
  legs () const override
  {
    return 2;
  }
};

/// Two independent bases; `Both` converts to `Right` with a non-zero offset.
class Left
{
public:
  virtual ~Left () {}
  long left_payload[3] = { 1, 2, 3 };
};

class Right
{
public:
  virtual ~Right () {}
  long right_payload[3] = { 4, 5, 6 };
};

class Both : public Left, public Right
{
public:
  explicit Both (std::atomic<int> *live = nullptr) : live_ (live)
  {
    if (live_ != nullptr)
      live_->fetch_add (1);
  }

  ~Both () override
  {
    if (live_ != nullptr)
      live_->fetch_sub (1);
  }

private:
  std::atomic<int> *live_;
};

/// A class that is not related to the hierarchies above.
class Stranger
{
public:
  virtual ~Stranger () {}
};

/// A type with a 64-byte alignment requirement.
struct alignas (64) OverAligned
{
  explicit OverAligned (int v = 0) : value (v) {}
  int value;
};

/// A type whose constructor throws when asked to.
class Fragile
{
public:
  explicit Fragile (std::atomic<int> *live, bool fail = false) : live_ (live)
  {
    if (fail)
      throw std::runtime_error ("Fragile: construction failed");
    live_->fetch_add (1);
  }

  ~Fragile () { live_->fetch_sub (1); }

private:
  std::atomic<int> *live_;
};

// --- Deleters
// -----------------------------------------------------------------

/// Deletes what it is given and counts the calls in `*calls`.
template <class T> class CountingDeleter
{
public:
  explicit CountingDeleter (std::atomic<int> *calls = nullptr) : calls_ (calls)
  {
  }

  void
  operator() (T *p) const
  {
    if (calls_ != nullptr)
      calls_->fetch_add (1);
    delete p;
  }

  /// The counter this deleter reports to.
  std::atomic<int> *
  counter () const
  {
    return calls_;
  }

private:
  std::atomic<int> *calls_;
};

/// A deleter that does not delete: counts and remembers the last pointer.
class RecordingDeleter
{
public:
  RecordingDeleter (int *calls, void **last) : calls_ (calls), last_ (last) {}

  template <class P>
  void
  operator() (P p) const
  {
    ++*calls_;
    *last_ = const_cast<void *> (static_cast<void const *> (p));
  }

private:
  int *calls_;
  void **last_;
};

/// An empty deleter class (to see the empty-base optimization).
struct EmptyDeleter
{
  template <class P>
  void
  operator() (P p) const
  {
    delete p;
  }
};

/// A deleter that is move-only.
class MoveOnlyDeleter
{
public:
  explicit MoveOnlyDeleter (std::atomic<int> *calls = nullptr) : calls_ (calls)
  {
  }
  MoveOnlyDeleter (MoveOnlyDeleter &&other) noexcept : calls_ (other.calls_)
  {
    other.calls_ = nullptr;
  }
  MoveOnlyDeleter (MoveOnlyDeleter const &) = delete;
  MoveOnlyDeleter &operator= (MoveOnlyDeleter const &) = delete;

  template <class P>
  void
  operator() (P p) const
  {
    if (calls_ != nullptr)
      calls_->fetch_add (1);
    delete p;
  }

private:
  std::atomic<int> *calls_;
};

// --- Allocators
// -----------------------------------------------------------------

/// Counters shared by every copy and rebind of a `CountingAllocator`.
struct AllocStats
{
  std::atomic<long> allocations{ 0 };
  std::atomic<long> deallocations{ 0 };
  std::atomic<long> constructs{ 0 };
  std::atomic<long> destroys{ 0 };
  std::atomic<long> bytes{ 0 };
  std::atomic<std::size_t> last_alignment_seen{ 0 };
  /// Fail (throw `std::bad_alloc`) the allocation with this 1-based number;
  /// 0 never fails.
  std::atomic<long> fail_at{ 0 };

  long
  live_blocks () const
  {
    return allocations.load () - deallocations.load ();
  }
};

/// An allocator that counts allocations, deallocations, `construct` and
/// `destroy` calls, and can be told to throw. Allocates with `operator new`.
template <class T> class CountingAllocator
{
public:
  typedef T value_type;

  explicit CountingAllocator (AllocStats *stats) : stats_ (stats) {}

  template <class U>
  CountingAllocator (CountingAllocator<U> const &other) noexcept
      : stats_ (other.stats ())
  {
  }

  T *
  allocate (std::size_t n)
  {
    long const number = stats_->allocations.fetch_add (1) + 1;
    if (stats_->fail_at.load () == number)
      {
        stats_->allocations.fetch_sub (1);
        throw std::bad_alloc ();
      }
    stats_->bytes.fetch_add (static_cast<long> (n * sizeof (T)));
    stats_->last_alignment_seen.store (alignof (T));
    return static_cast<T *> (
        sp::detail::allocate_aligned (n * sizeof (T), alignof (T)));
  }

  void
  deallocate (T *p, std::size_t) noexcept
  {
    stats_->deallocations.fetch_add (1);
    sp::detail::deallocate_aligned (p, alignof (T));
  }

  template <class U, class... Args>
  void
  construct (U *p, Args &&...args)
  {
    stats_->constructs.fetch_add (1);
    ::new (static_cast<void *> (p)) U (std::forward<Args> (args)...);
  }

  template <class U>
  void
  destroy (U *p)
  {
    stats_->destroys.fetch_add (1);
    p->~U ();
  }

  AllocStats *
  stats () const noexcept
  {
    return stats_;
  }

private:
  AllocStats *stats_;
};

template <class T, class U>
bool
operator== (CountingAllocator<T> const &a, CountingAllocator<U> const &b)
{
  return a.stats () == b.stats ();
}

template <class T, class U>
bool
operator!= (CountingAllocator<T> const &a, CountingAllocator<U> const &b)
{
  return !(a == b);
}

// --- Misc
// --------------------------------------------------------------------------

/// Detects `*p` (a local class cannot have member templates).
struct deref_detect
{
  template <class P>
  static auto test (int)
      -> decltype (*std::declval<P const &> (), std::true_type ());
  template <class P> static std::false_type test (...);
};

/// Detects `p.operator-> ()`, `p[i]` and `*p` of the array tests.
struct array_detect
{
  template <class P>
  static auto deref (int)
      -> decltype (*std::declval<P const &> (), std::true_type ());
  template <class P> static std::false_type deref (...);
  template <class P>
  static auto arrow (int)
      -> decltype (std::declval<P const &> ().operator->(), std::true_type ());
  template <class P> static std::false_type arrow (...);
  template <class P>
  static auto index (int)
      -> decltype (std::declval<P const &> ()[std::ptrdiff_t ()],
                   std::true_type ());
  template <class P> static std::false_type index (...);
};

/// Thread counts and work of the stress tests, shrunk on slow builds.
inline int
stress_work (int base)
{
  return lumex_test::scaled (base);
}

/// True when the build is sanitized: address-exact checks are skipped there.
inline bool
sanitized ()
{
  return lumex_test::instrumented_build ();
}
} // namespace smart_ptr_test

#endif // !LUMEX_TESTS_CORE_SMART_PTR_SMART_PTR_TEST_SUPPORT_HPP
