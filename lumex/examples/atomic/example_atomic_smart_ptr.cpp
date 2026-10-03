// Walkthrough of atomic_shared_ptr and atomic_weak_ptr, section by section.
// Built at C++11, the floor of the module; the same code compiles unchanged
// at C++20, where the classes wrap std::atomic<std::shared_ptr<T>> when the
// standard library provides it.

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "lumex/core/atomic/LumexAtomic"

// The templates are declared only in lumex::core::atomic::smart_ptr; a
// program names them in full or brings the short names in itself.
using lumex::core::atomic::smart_ptr::atomic_shared_ptr;
using lumex::core::atomic::smart_ptr::atomic_weak_ptr;

namespace
{
struct Settings
{
  std::string name;
  int retries;
};

void
section_implementation ()
{
  std::cout << "=== 1. Selected implementation ===\n";
  atomic_shared_ptr<int> probe;
  std::cout << "wraps the standard type: "
            << (LUMEX_ATOMIC_SMART_PTR_USES_STD ? "yes" : "no")
            << ", is_lock_free: " << (probe.is_lock_free () ? "yes" : "no")
            << ", is_always_lock_free: "
            << (atomic_shared_ptr<int>::is_always_lock_free ? "yes" : "no")
            << "\n\n";
}

void
section_load_store ()
{
  std::cout << "=== 2. Construction, load, store, assignment ===\n";
  atomic_shared_ptr<Settings> current (
      std::make_shared<Settings> (Settings{ "default", 3 }));
  std::shared_ptr<Settings> const snapshot = current.load ();
  std::cout << "loaded: " << snapshot->name << " retries=" << snapshot->retries
            << '\n';

  current.store (std::make_shared<Settings> (Settings{ "tuned", 5 }));
  std::shared_ptr<Settings> const converted = current; // operator shared_ptr
  std::cout << "after store: " << converted->name
            << "; the old snapshot is still valid: " << snapshot->name << '\n';

  current = nullptr; // LWG 3893
  std::cout << "after = nullptr: " << (current.load () ? "set" : "empty")
            << "\n\n";
}

void
section_exchange ()
{
  std::cout << "=== 3. exchange ===\n";
  atomic_shared_ptr<std::string> slot (
      std::make_shared<std::string> ("first"));
  std::shared_ptr<std::string> const previous
      = slot.exchange (std::make_shared<std::string> ("second"));
  std::cout << "exchange returned '" << *previous << "', slot holds '"
            << *slot.load () << "'\n\n";
}

void
section_compare_exchange ()
{
  std::cout << "=== 4. compare_exchange_strong ===\n";
  std::shared_ptr<int> const one = std::make_shared<int> (1);
  atomic_shared_ptr<int> value (one);

  std::shared_ptr<int> expected = one;
  bool const ok
      = value.compare_exchange_strong (expected, std::make_shared<int> (2));
  std::cout << "expected the stored owner: " << (ok ? "stored" : "failed")
            << ", now " << *value.load () << '\n';

  // Equal values in different objects are not equivalent: the comparison is
  // by stored pointer and owner, not by value.
  std::shared_ptr<int> copy_of_two = std::make_shared<int> (2);
  bool const by_value
      = value.compare_exchange_strong (copy_of_two, std::make_shared<int> (3));
  std::cout << "expected an equal value in another object: "
            << (by_value ? "stored" : "failed")
            << "; expected now holds the stored owner: "
            << (copy_of_two == value.load () ? "yes" : "no") << "\n\n";
}

void
section_copy_on_write ()
{
  std::cout << "=== 5. Copy-on-write update with compare_exchange_weak ===\n";
  atomic_shared_ptr<int const> counter (std::make_shared<int const> (0));
  for (int i = 0; i < 3; ++i)
    {
      std::shared_ptr<int const> expected = counter.load ();
      std::shared_ptr<int const> desired;
      do
        desired = std::make_shared<int const> (*expected + 10);
      while (!counter.compare_exchange_weak (expected, desired));
    }
  std::cout << "counter after three updates: " << *counter.load () << "\n\n";
}

void
section_weak ()
{
  std::cout << "=== 6. atomic_weak_ptr ===\n";
  atomic_weak_ptr<Settings> cache;
  {
    std::shared_ptr<Settings> const owner
        = std::make_shared<Settings> (Settings{ "session", 1 });
    cache.store (owner);
    std::shared_ptr<Settings> const locked = cache.load ().lock ();
    std::cout << "while the owner lives: "
              << (locked ? locked->name : std::string ("expired")) << '\n';
  }
  std::cout << "after the owner is gone: "
            << (cache.load ().expired () ? "expired" : "alive") << "\n\n";
}

void
section_wait_notify ()
{
  std::cout << "=== 7. wait and notify_one ===\n";
  atomic_shared_ptr<std::string> mailbox (
      std::make_shared<std::string> ("empty"));
  std::shared_ptr<std::string> const seen = mailbox.load ();
  std::thread reader (
      [&mailbox, seen]
        {
          mailbox.wait (
              seen); // sleeps until the value changes and is notified
          std::cout << "reader woke up with '" << *mailbox.load () << "'\n";
        });
  mailbox.store (std::make_shared<std::string> ("hello"));
  mailbox.notify_one ();
  reader.join ();
  std::cout << '\n';
}
} // namespace

int
main ()
{
  section_implementation ();
  section_load_store ();
  section_exchange ();
  section_compare_exchange ();
  section_copy_on_write ();
  section_weak ();
  section_wait_notify ();
  return 0;
}
