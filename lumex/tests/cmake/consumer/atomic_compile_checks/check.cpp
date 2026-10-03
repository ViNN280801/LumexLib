// Compile checks of atomic_shared_ptr and atomic_weak_ptr, built by
// cmake.atomic_compile_checks. They port two libc++ verify tests of
// llvm-project pull request 194215: nodiscard.verify.cpp (load () and
// is_lock_free () results must be used) and
// atomic_shared_ptr_memory_order.verify.cpp (constant invalid memory orders
// are diagnosed where the compiler has the diagnose_if attribute). Exactly
// one of LUMEX_ATOMIC_GOOD_CASE or LUMEX_ATOMIC_BAD_CASE is defined; the
// fixture compiles with warnings as errors.

#include <atomic>
#include <memory>

#include "lumex/core/atomic/LumexAtomic"

using lumex::core::atomic::smart_ptr::atomic_shared_ptr;
using lumex::core::atomic::smart_ptr::atomic_weak_ptr;

int
main ()
{
  atomic_shared_ptr<int> shared;
  atomic_weak_ptr<int> weak;
  std::shared_ptr<int> expected;
  std::shared_ptr<int> desired;
  std::weak_ptr<int> weak_expected;
  std::weak_ptr<int> weak_desired;
  int used = 0;

#if defined(LUMEX_ATOMIC_GOOD_CASE)
  // Every member with every valid constant order: no diagnostic.
  shared.store (desired, std::memory_order_relaxed);
  shared.store (desired, std::memory_order_release);
  shared.store (desired, std::memory_order_seq_cst);
  used += shared.load (std::memory_order_relaxed) ? 1 : 0;
  used += shared.load (std::memory_order_consume) ? 1 : 0;
  used += shared.load (std::memory_order_acquire) ? 1 : 0;
  used += shared.load (std::memory_order_seq_cst) ? 1 : 0;
  used += shared.is_lock_free () ? 1 : 0;
  used += shared.exchange (desired, std::memory_order_acq_rel) ? 1 : 0;
  used += shared.compare_exchange_strong (expected, desired,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)
              ? 1
              : 0;
  used += shared.compare_exchange_weak (expected, desired,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)
              ? 1
              : 0;
  used += shared.compare_exchange_strong (expected, desired,
                                          std::memory_order_acq_rel)
              ? 1
              : 0;
  shared.wait (std::make_shared<int> (1), std::memory_order_acquire);
  shared.notify_one ();
  shared.notify_all ();
  shared = nullptr;
  weak.store (weak_desired, std::memory_order_release);
  used += weak.load (std::memory_order_acquire).expired () ? 1 : 0;
  used += weak.is_lock_free () ? 1 : 0;
  used += weak.compare_exchange_strong (weak_expected, weak_desired,
                                        std::memory_order_seq_cst,
                                        std::memory_order_consume)
              ? 1
              : 0;
  weak.notify_all ();
#elif LUMEX_ATOMIC_BAD_CASE == 1
  shared.load ();
#elif LUMEX_ATOMIC_BAD_CASE == 2
  shared.is_lock_free ();
#elif LUMEX_ATOMIC_BAD_CASE == 3
  weak.load ();
#elif LUMEX_ATOMIC_BAD_CASE == 4
  weak.is_lock_free ();
#elif LUMEX_ATOMIC_BAD_CASE == 5
  shared.store (desired, std::memory_order_consume);
#elif LUMEX_ATOMIC_BAD_CASE == 6
  shared.store (desired, std::memory_order_acquire);
#elif LUMEX_ATOMIC_BAD_CASE == 7
  shared.store (desired, std::memory_order_acq_rel);
#elif LUMEX_ATOMIC_BAD_CASE == 8
  used += shared.load (std::memory_order_release) ? 1 : 0;
#elif LUMEX_ATOMIC_BAD_CASE == 9
  used += shared.load (std::memory_order_acq_rel) ? 1 : 0;
#elif LUMEX_ATOMIC_BAD_CASE == 10
  shared.wait (expected, std::memory_order_release);
#elif LUMEX_ATOMIC_BAD_CASE == 11
  shared.wait (expected, std::memory_order_acq_rel);
#elif LUMEX_ATOMIC_BAD_CASE == 12
  used += shared.compare_exchange_strong (
      expected, desired, std::memory_order_seq_cst, std::memory_order_release);
#elif LUMEX_ATOMIC_BAD_CASE == 13
  used += shared.compare_exchange_strong (
      expected, desired, std::memory_order_seq_cst, std::memory_order_acq_rel);
#elif LUMEX_ATOMIC_BAD_CASE == 14
  used += shared.compare_exchange_weak (
      expected, desired, std::memory_order_seq_cst, std::memory_order_release);
#elif LUMEX_ATOMIC_BAD_CASE == 15
  used += shared.compare_exchange_weak (
      expected, desired, std::memory_order_seq_cst, std::memory_order_acq_rel);
#elif LUMEX_ATOMIC_BAD_CASE == 16
  weak.store (weak_desired, std::memory_order_consume);
#elif LUMEX_ATOMIC_BAD_CASE == 17
  weak.store (weak_desired, std::memory_order_acquire);
#elif LUMEX_ATOMIC_BAD_CASE == 18
  weak.store (weak_desired, std::memory_order_acq_rel);
#elif LUMEX_ATOMIC_BAD_CASE == 19
  used += weak.load (std::memory_order_release).expired () ? 1 : 0;
#elif LUMEX_ATOMIC_BAD_CASE == 20
  used += weak.load (std::memory_order_acq_rel).expired () ? 1 : 0;
#elif LUMEX_ATOMIC_BAD_CASE == 21
  weak.wait (weak_expected, std::memory_order_release);
#elif LUMEX_ATOMIC_BAD_CASE == 22
  weak.wait (weak_expected, std::memory_order_acq_rel);
#elif LUMEX_ATOMIC_BAD_CASE == 23
  used += weak.compare_exchange_strong (weak_expected, weak_desired,
                                        std::memory_order_seq_cst,
                                        std::memory_order_release);
#elif LUMEX_ATOMIC_BAD_CASE == 24
  used += weak.compare_exchange_strong (weak_expected, weak_desired,
                                        std::memory_order_seq_cst,
                                        std::memory_order_acq_rel);
#elif LUMEX_ATOMIC_BAD_CASE == 25
  used += weak.compare_exchange_weak (weak_expected, weak_desired,
                                      std::memory_order_seq_cst,
                                      std::memory_order_release);
#elif LUMEX_ATOMIC_BAD_CASE == 26
  used += weak.compare_exchange_weak (weak_expected, weak_desired,
                                      std::memory_order_seq_cst,
                                      std::memory_order_acq_rel);
#elif defined(LUMEX_ATOMIC_PROBE_DIAGNOSE_IF)
#if !defined(LUMEX_ATOMIC_SMART_PTR_HAS_DIAGNOSE_IF)
#error "no diagnose_if attribute"
#endif
#else
#error "define LUMEX_ATOMIC_GOOD_CASE or LUMEX_ATOMIC_BAD_CASE"
#endif
  return used == -1 ? 1 : 0;
}
