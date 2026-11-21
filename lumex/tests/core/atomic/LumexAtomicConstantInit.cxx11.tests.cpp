// LumexAtomicConstantInit.cxx11.tests.cpp
//
// The default and nullptr constructors of every atomic smart pointer engine
// (and of the cells and the bit lock under them) spell constexpr by
// LUMEX_CONSTEXPR_CTOR (todo 108), so an object with static storage is
// constant-initialized (LWG 3661). The classes have destructors with a side
// effect, so they are not literal types and no constexpr variable of them
// exists before C++20. The constructors are checked where the standard lets
// the check be made:
//   - from C++20 (__cpp_constinit) every object below is `constinit`: the
//     build stops when a constructor on the way is not constexpr;
//   - before C++20 the compiler checks it where it can: clang (clang-cl
//     included) by [[clang::require_constant_initialization]] and GCC 10 and
//     newer by __constinit, so the build stops there too;
//   - where neither exists (MSVC, older GCC) and as a second check
//     everywhere, each object is stored into by the initializer of a
//     variable defined before it. A constant-initialized object is not
//     constructed again by its own dynamic initialization, so the stored
//     value is still there when main runs; a constructor that lost constexpr
//     runs after the store and empties the object. This check holds only
//     where the compiler does not use the permission of [basic.start.static]
//     to initialize statically an object whose constructor is not constexpr
//     (cl at /O2 does, and then the check sees nothing). Class templates are
//     not diagnosed when a constexpr constructor cannot be constant, so the
//     build alone would miss it.
// dwcas_word is a literal type and is pinned by static_assert in
// LumexDwcasWord.cxx11.tests.cpp.
#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/atomic/sync/LumexBitLock.hpp"
#include "lumex/core/smart_ptr/LumexSmartPtr"

namespace
{
namespace asp = lumex::core::atomic::smart_ptr;
namespace sync_detail = lumex::core::atomic::sync::Detail;
namespace lsp = lumex::core::smart_ptr;

#if !defined(__cpp_constinit)
// The pointer values the early initializers store. A weak pointer needs an
// owner that outlives it.
std::shared_ptr<int> const &
std_owner ()
{
  static std::shared_ptr<int> const owner = std::make_shared<int> (7);
  return owner;
}

template <class T>
lsp::shared_ptr<T> const &
lumex_owner ()
{
  static lsp::shared_ptr<T> const owner = lsp::make_shared<T> (7);
  return owner;
}

std::shared_ptr<int>
make_value (std::shared_ptr<int> const *)
{
  return std_owner ();
}

std::weak_ptr<int>
make_value (std::weak_ptr<int> const *)
{
  return std::weak_ptr<int> (std_owner ());
}

// The overloads of the module's own pointer family are templates: the family
// is not part of every build, and an unused template is not reported.
template <class T>
lsp::shared_ptr<T>
make_value (lsp::shared_ptr<T> const *)
{
  return lumex_owner<T> ();
}

template <class T>
lsp::weak_ptr<T>
make_value (lsp::weak_ptr<T> const *)
{
  return lsp::weak_ptr<T> (lumex_owner<T> ());
}

#endif // !__cpp_constinit

bool
is_set (std::shared_ptr<int> const &pointer)
{
  return static_cast<bool> (pointer);
}

bool
is_set (std::weak_ptr<int> const &pointer)
{
  return static_cast<bool> (pointer.lock ());
}

template <class T>
bool
is_set (lsp::shared_ptr<T> const &pointer)
{
  return static_cast<bool> (pointer);
}

template <class T>
bool
is_set (lsp::weak_ptr<T> const &pointer)
{
  return static_cast<bool> (pointer.lock ());
}

#if !defined(__cpp_constinit)
template <class Atomic> struct early_store
{
  explicit early_store (Atomic &object)
  {
    typename Atomic::value_type const *const tag = nullptr;
    object.store (make_value (tag));
  }
};

// Takes the lock before main. The lock is constant-initialized, but a
// statically initialized object is destroyed as if it were dynamically
// initialized at its definition ([basic.start.term]/3). The lock is given
// back by release_lock, which is defined after the lock and so is destroyed
// before it. No process, whichever test it runs, destroys a taken lock or
// releases a free one.
struct early_lock
{
  explicit early_lock (sync_detail::bit_lock &object) { object.lock (); }
};

struct release_lock
{
  explicit release_lock (sync_detail::bit_lock &object) : lock_ (object) {}

  ~release_lock ()
  {
    if ((lock_.state () & sync_detail::bit_lock::lock_bit) != 0u)
      lock_.unlock ();
  }

  release_lock (release_lock const &) = delete;
  release_lock &operator= (release_lock const &) = delete;

private:
  sync_detail::bit_lock &lock_;
};
#endif // !__cpp_constinit

// The value an object holds when main runs: set by the early initializer
// when the object is constant-initialized, empty when constinit (no early
// initializer) or when its constructor ran after the store.
template <class Atomic>
bool
holds_value (Atomic &object)
{
  return is_set (object.load ());
}
} // namespace

// The definition of an object, the check main makes on it. From C++20 the
// definition is `constinit` and the object is empty when main runs; before,
// an earlier variable stores into it and the stored value must survive.
#if defined(__cpp_constinit)
#define LUMEX_TEST_CONSTINIT constinit
#elif defined(__clang__)
#define LUMEX_TEST_CONSTINIT [[clang::require_constant_initialization]]
#elif defined(__GNUC__) && __GNUC__ >= 10 && !defined(__INTEL_COMPILER)       \
    && !defined(__NVCOMPILER)
#define LUMEX_TEST_CONSTINIT __constinit
#else
#define LUMEX_TEST_CONSTINIT
#endif

#if defined(__cpp_constinit)
#define LUMEX_TEST_EARLY(Type, name, init) constinit Type name init
#define LUMEX_TEST_EARLY_LOCK_DEFINE(name) constinit sync_detail::bit_lock name
#define LUMEX_TEST_RELEASE_AT_EXIT(name) static_assert (true, "")
#define LUMEX_TEST_EXPECT_EARLY(name) EXPECT_FALSE (holds_value (name))
#define LUMEX_TEST_EXPECT_EARLY_LOCK(name) EXPECT_EQ (name.state (), 0u)
#else
#define LUMEX_TEST_EARLY(Type, name, init)                                    \
  extern Type name;                                                           \
  early_store<Type> const name##_early (name);                                \
  LUMEX_TEST_CONSTINIT Type name init
#define LUMEX_TEST_EARLY_LOCK_DEFINE(name)                                    \
  extern sync_detail::bit_lock name;                                          \
  early_lock const name##_early (name);                                       \
  LUMEX_TEST_CONSTINIT sync_detail::bit_lock name
#define LUMEX_TEST_RELEASE_AT_EXIT(name)                                      \
  release_lock const name##_release (name)
#define LUMEX_TEST_EXPECT_EARLY(name) EXPECT_TRUE (holds_value (name))
#define LUMEX_TEST_EXPECT_EARLY_LOCK(name)                                    \
  EXPECT_EQ (name.state (),                                                   \
             static_cast<std::uint32_t> (sync_detail::bit_lock::lock_bit))
#endif

namespace
{
// The common base of the engines: the derived classes call its default
// constructor, so its constructor from nullptr is reached only directly.
using lock_based_base = asp::Detail::basic_atomic_shared_ptr<
    int, asp::Detail::lock_based_cell<std::shared_ptr<int>>>;

LUMEX_TEST_EARLY (lock_based_base, g_lock_based_base_null, (nullptr));
LUMEX_TEST_EARLY (asp::atomic_shared_ptr_lock_based<int>, g_lock_based, );
LUMEX_TEST_EARLY (asp::atomic_shared_ptr_lock_based<int>, g_lock_based_null,
                  (nullptr));
LUMEX_TEST_EARLY (asp::atomic_weak_ptr_lock_based<int>, g_lock_based_weak, );

#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
LUMEX_TEST_EARLY (asp::atomic_shared_ptr_std_backed<int>, g_std_backed, );
LUMEX_TEST_EARLY (asp::atomic_shared_ptr_std_backed<int>, g_std_backed_null,
                  (nullptr));
LUMEX_TEST_EARLY (asp::atomic_weak_ptr_std_backed<int>, g_std_backed_weak, );
#endif

#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
LUMEX_TEST_EARLY (asp::atomic_shared_ptr_lock_free<int>, g_lock_free, );
LUMEX_TEST_EARLY (asp::atomic_shared_ptr_lock_free<int>, g_lock_free_null,
                  (nullptr));
LUMEX_TEST_EARLY (asp::atomic_weak_ptr_lock_free<int>, g_lock_free_weak, );
#endif

#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
LUMEX_TEST_EARLY (asp::atomic_shared_ptr_lock_based_lumex<int>, g_lumex, );
LUMEX_TEST_EARLY (asp::atomic_shared_ptr_lock_based_lumex<int>, g_lumex_null,
                  (nullptr));
LUMEX_TEST_EARLY (asp::atomic_weak_ptr_lock_based_lumex<int>, g_lumex_weak, );
#endif

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
LUMEX_TEST_EARLY (asp::atomic_shared_ptr_lock_free_split_count<int>,
                  g_split_count, );
LUMEX_TEST_EARLY (asp::atomic_shared_ptr_lock_free_split_count<int>,
                  g_split_count_null, (nullptr));
LUMEX_TEST_EARLY (asp::atomic_weak_ptr_lock_free_split_count<int>,
                  g_split_count_weak, );
#endif

LUMEX_TEST_EARLY_LOCK_DEFINE (g_bit_lock);
LUMEX_TEST_RELEASE_AT_EXIT (g_bit_lock);
} // namespace

TEST (LumexAtomicConstantInitTest,
      GivenLockBasedEngines_WhenMainRuns_ThenTheyWereConstantInitialized)
{
  LUMEX_TEST_EXPECT_EARLY (g_lock_based_base_null);
  LUMEX_TEST_EXPECT_EARLY (g_lock_based);
  LUMEX_TEST_EXPECT_EARLY (g_lock_based_null);
  LUMEX_TEST_EXPECT_EARLY (g_lock_based_weak);
}

#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
TEST (LumexAtomicConstantInitTest,
      GivenStdBackedEngines_WhenMainRuns_ThenTheyWereConstantInitialized)
{
  LUMEX_TEST_EXPECT_EARLY (g_std_backed);
  LUMEX_TEST_EXPECT_EARLY (g_std_backed_null);
  LUMEX_TEST_EXPECT_EARLY (g_std_backed_weak);
}
#endif

#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
TEST (LumexAtomicConstantInitTest,
      GivenLockFreeEngines_WhenMainRuns_ThenTheyWereConstantInitialized)
{
  LUMEX_TEST_EXPECT_EARLY (g_lock_free);
  LUMEX_TEST_EXPECT_EARLY (g_lock_free_null);
  LUMEX_TEST_EXPECT_EARLY (g_lock_free_weak);
}
#endif

#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
TEST (LumexAtomicConstantInitTest,
      GivenLumexFamilyEngines_WhenMainRuns_ThenTheyWereConstantInitialized)
{
  LUMEX_TEST_EXPECT_EARLY (g_lumex);
  LUMEX_TEST_EXPECT_EARLY (g_lumex_null);
  LUMEX_TEST_EXPECT_EARLY (g_lumex_weak);
}
#endif

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
TEST (LumexAtomicConstantInitTest,
      GivenSplitCountEngines_WhenMainRuns_ThenTheyWereConstantInitialized)
{
  LUMEX_TEST_EXPECT_EARLY (g_split_count);
  LUMEX_TEST_EXPECT_EARLY (g_split_count_null);
  LUMEX_TEST_EXPECT_EARLY (g_split_count_weak);
}
#endif

TEST (LumexAtomicConstantInitTest,
      GivenTheBitLock_WhenMainRuns_ThenItWasConstantInitialized)
{
  LUMEX_TEST_EXPECT_EARLY_LOCK (g_bit_lock);
}
