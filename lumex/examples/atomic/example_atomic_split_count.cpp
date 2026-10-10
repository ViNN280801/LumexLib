// Walkthrough of the pointer family of lumex::core::smart_ptr and of the
// atomic pair over it, with the split-count engine where the build has it.
// Built at C++11, the floor of the module. The split-count engine exists on
// x86-64 only; elsewhere the family alias is the lock-based engine over the
// same pointers, and the sections below name the engine they run.

#include <iostream>
#include <string>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/smart_ptr/LumexSmartPtr"

#if !LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
#error                                                                        \
    "this example needs lumex::smart_ptr, the pointer family of the atomic pair"
#endif

namespace sp = lumex::core::smart_ptr;

// The family alias: the split-count engine where
// LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT is 1, the lock-based engine
// over the same pointers otherwise.
using sp::atomic_shared_ptr;

namespace
{
struct Settings
{
  std::string name;
  int retries;
};

void
section_engine ()
{
  std::cout << "=== 1. Selected engine ===\n";
  std::cout << "family alias: "
            << (LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT
                    ? "split-count engine"
                    : "lock-based engine over the family pointers")
            << "\n";
  std::cout << "is_always_lock_free: "
            << (atomic_shared_ptr<int>::is_always_lock_free ? "yes" : "no")
            << "\n\n";
}

void
section_load_store ()
{
  std::cout << "=== 2. Construction, load and store ===\n";
  atomic_shared_ptr<Settings> current (
      sp::make_shared<Settings> (Settings{ "default", 3 }));
  sp::shared_ptr<Settings> const snapshot = current.load ();
  std::cout << "loaded: " << snapshot->name << " retries=" << snapshot->retries
            << '\n';

  current.store (sp::make_shared<Settings> (Settings{ "tuned", 5 }));
  std::cout << "after store: " << current.load ()->name
            << "; the old snapshot is still valid: " << snapshot->name
            << "\n\n";
}

void
section_compare_exchange ()
{
  std::cout << "=== 3. Compare-exchange loop ===\n";
  atomic_shared_ptr<int const> counter (sp::make_shared<int const> (0));
  for (int i = 0; i < 3; ++i)
    {
      sp::shared_ptr<int const> expected = counter.load ();
      sp::shared_ptr<int const> desired;
      do
        desired = sp::make_shared<int const> (*expected + 10);
      while (!counter.compare_exchange_weak (expected, desired));
    }
  std::cout << "counter after three updates: " << *counter.load () << "\n\n";
}

void
section_use_count ()
{
  std::cout << "=== 4. use_count and use_count_settled ===\n";
  atomic_shared_ptr<int> slot (sp::make_shared<int> (7));
  {
    // The slot and this copy both own the value.
    sp::shared_ptr<int> const copy = slot.load ();
    std::cout << "while a copy is held: use_count () = " << copy.use_count ()
              << ", use_count_settled () = " << copy.use_count_settled ()
              << '\n';
  }
  // exchange returns the old owner: now it is the sole owner, so 1 means
  // sole ownership. use_count () never reports fewer owners than exist; it
  // can be higher only while another thread's operation on the slot is in
  // flight, and then use_count_settled () would wait for it to settle.
  sp::shared_ptr<int> const taken = slot.exchange (sp::make_shared<int> (8));
  std::cout << "after exchange, the returned owner: use_count () = "
            << taken.use_count ()
            << " (sole owner: " << (taken.use_count () == 1 ? "yes" : "no")
            << ")\n\n";
}

// The weak engine: the split-count one where the build has it, the lock-based
// engine over the same weak pointers otherwise. The family alias
// atomic_weak_ptr<T> resolves to the same choice as atomic_shared_ptr<T>.
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
using weak_engine
    = lumex::core::atomic::smart_ptr::atomic_weak_ptr_lock_free_split_count<
        Settings>;
char const *const weak_name = "atomic_weak_ptr_lock_free_split_count";
#else
using weak_engine
    = lumex::core::atomic::smart_ptr::atomic_weak_ptr_lock_based_lumex<
        Settings>;
char const *const weak_name = "atomic_weak_ptr_lock_based_lumex";
#endif

void
section_weak ()
{
  std::cout << "=== 5. The weak engine (" << weak_name << ") ===\n";
  weak_engine cache;
  {
    sp::shared_ptr<Settings> const owner
        = sp::make_shared<Settings> (Settings{ "session", 1 });
    cache.store (owner);
    sp::shared_ptr<Settings> const locked = cache.load ().lock ();
    std::cout << "while the owner lives: "
              << (locked ? locked->name : std::string ("expired")) << '\n';
  }
  std::cout << "after the owner is gone: "
            << (cache.load ().expired () ? "expired" : "alive") << "\n\n";
}
} // namespace

int
main ()
{
  section_engine ();
  section_load_store ();
  section_compare_exchange ();
  section_use_count ();
  section_weak ();
  return 0;
}
