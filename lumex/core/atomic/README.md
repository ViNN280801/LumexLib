# LumexAtomic: atomic_shared_ptr and atomic_weak_ptr from C++11 {#lumex_atomic}

`lumex::core::atomic` (target `lumex::atomic`, header-only) provides `atomic_shared_ptr<T>` and `atomic_weak_ptr<T>`: the interface of C++20's `std::atomic<std::shared_ptr<T>>` and `std::atomic<std::weak_ptr<T>>` (P0718R2, with LWG 3661 and LWG 3893) from C++11 on, over the ordinary `std::shared_ptr` and `std::weak_ptr` of any standard library. Where the standard library has the C++20 types, LumexAtomic wraps them; elsewhere it uses a lock-based implementation of its own.

```cpp
#include "lumex/core/atomic/LumexAtomic"

atomic_shared_ptr<Config const> g_config (std::make_shared<Config const> ());

void reload (std::shared_ptr<Config const> next)
{
  g_config.store (std::move (next)); // readers keep the old one alive
  g_config.notify_all ();
}

std::shared_ptr<Config const> current = g_config.load ();
g_config.wait (current); // until another configuration is stored
```

## Origin

This module is a port of the author's own implementation of `std::atomic<std::shared_ptr<T>>` and `std::atomic<std::weak_ptr<T>>` for LLVM libc++ (Vladislav Semykin, P0718R2, llvm-project pull request 194215). That implementation has two methods, a lock-based one and a lock-free one, and both work. It is not a third-party library dropped in, and it is not derived from libstdc++, the MSVC STL or Folly; the design notes, the review discussion and the investigations behind it are summarized below.

LumexAtomic ports the lock-based method. The lock-free method is described below, in "The lock-free method of the libc++ implementation", together with the reason it is not part of this library.

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/atomic/LumexAtomic` | Umbrella: both class templates |
| `lumex/core/atomic/smart_ptr/LumexAtomicSharedPtr.hpp` | `atomic_shared_ptr<T>` |
| `lumex/core/atomic/smart_ptr/LumexAtomicWeakPtr.hpp` | `atomic_weak_ptr<T>` |
| `lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp` | Which implementation a build gets (`LUMEX_ATOMIC_SMART_PTR_USES_STD`), memory order checks |
| `lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrCell.hpp` | The two cells behind the class templates: lock-based and standard |
| `lumex/core/atomic/sync/LumexBitLock.hpp` | The two-bit lock of the lock-based cell |
| `lumex/core/atomic/sync/LumexAtomicWait.hpp` | Waiting for a 32-bit word to change: `std::atomic::wait` or a striped table |

Everything outside the two class templates is an implementation detail (`Detail` namespaces).

## Interface

Both templates live in `lumex::core::atomic::smart_ptr` and are also available at global scope as `atomic_shared_ptr` and `atomic_weak_ptr`, the way LumexLib exports `optional`. The members follow `std::atomic<std::shared_ptr<T>>` and `std::atomic<std::weak_ptr<T>>`:

| Member | Notes |
| --- | --- |
| `value_type`, `is_always_lock_free` | `false` for the lock-based cell, the standard type's value otherwise |
| default, `nullptr` (shared only) and value constructors | the default and `nullptr` constructors are `constexpr`, so `constinit` works (LWG 3661) |
| copy constructor and copy assignment | deleted |
| `is_lock_free ()` | `[[nodiscard]]`; `false` for the lock-based cell, the standard type's answer otherwise (`false` in libstdc++ and the MSVC STL) |
| `store`, `operator=` | `operator= (nullptr_t)` for the shared pointer (LWG 3893) |
| `load`, `operator value_type` | `load` is `[[nodiscard]]` |
| `exchange` | |
| `compare_exchange_weak`, `compare_exchange_strong` | with one or two memory orders |
| `wait (old, order)`, `notify_one`, `notify_all` | from C++11, not only C++20 |

Every operation is `noexcept`. Constant memory order arguments that the standard forbids (`store` with `acquire`, `load` with `release`, a failure order of `release`, ...) are reported at compile time by Clang through the `diagnose_if` attribute, as libc++ does for `std::atomic`.

## Which implementation a build gets

| Build | Cell | Sleeping | Inline namespace |
| --- | --- | --- | --- |
| C++20 with libstdc++ 12+ or the MSVC STL | wraps the standard type | `std::atomic::wait` | `std_backed_std_wait` |
| C++20 with libc++ (no `std::atomic<std::shared_ptr<T>>` yet) | lock-based | `std::atomic::wait` | `lock_based_std_wait` |
| C++11, C++14, C++17 | lock-based | striped table | `lock_based_table_wait` |
| `LUMEX_FORCE_LOCK_BASED_ATOMIC_SHARED_PTR` | lock-based | as above | `lock_based_*` |
| `LUMEX_FORCE_ATOMIC_WAIT_TABLE` | as above | striped table | `*_table_wait` |

The standard type is used when the library defines `__cpp_lib_atomic_shared_ptr`; `LUMEX_ATOMIC_SMART_PTR_USES_STD` reports the choice. The lock-based implementation sleeps through `std::atomic::wait` when the library defines `__cpp_lib_atomic_wait`; `LUMEX_ATOMIC_WAIT_USES_STD` reports that choice. The two `LUMEX_FORCE_*` macros exist for tests and benchmarks, which use them to run every implementation at C++20 as well, like libc++'s `_LIBCPP_FORCE_LOCK_BASED_ATOMIC_SHARED_PTR`.

Each combination lives in its own inline namespace. A program may mix translation units built with different standards or switches: their types are distinct, so passing an object across such a boundary through a function signature fails to link instead of silently mixing two layouts. (A mismatch hidden inside a user type that holds the object is not detected, as with any other configuration macro.)

The libc++ implementation dispatches the same way inside the standard library: the lock-free method on x86-64 with `CMPXCHG16B` (`__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16`, that is `-mcx16` or `-march=x86-64-v2`) and on AArch64 with LSE (`__ARM_FEATURE_ATOMICS`), the lock-based method everywhere else (PowerPC and AIX always), and `_LIBCPP_FORCE_LOCK_BASED_ATOMIC_SHARED_PTR` to force the lock-based method for an A/B comparison on the same machine. Without `-mcx16` the dispatch silently compiles the lock-based method; the libc++ benchmark therefore checks a label in its output to prove which method it measured.

## The lock-based implementation

### The lock word

The libc++ lock-based method keeps a spin lock in the two low bits of the control block pointer stored in the atomic object: a control block is at least 4-byte aligned, so bit 0 (taken) and bit 1 (a thread sleeps on the lock) are free. That works inside libc++, which owns `std::shared_ptr`. LumexAtomic works over the `std::shared_ptr` of whichever standard library the program uses and must not touch its layout, so the same two bits live in a 32-bit word of their own (`BitLock`, `sync/LumexBitLock.hpp`), next to an ordinary `std::shared_ptr` or `std::weak_ptr`:

- `lock ()`: a compare-exchange from "free" to "taken" (acquire). A thread that finds the lock taken sets the sleeper bit and sleeps until the word changes. LumexAtomic first spins briefly (12 pause instructions, then 4 yields, the spin policy of libstdc++'s `std::atomic::wait`); the libc++ method parks at once in libc++'s global wait table, while sleeping on the wait table of a C++11 build costs a mutex and a condition variable.
- `unlock ()`: one exchange to 0 (release) clears both bits; if the sleeper bit was set, the sleepers are woken.

### The lost wake-up

The release clears the sleeper bit together with the lock bit. A woken thread races for the lock again, and a third thread may take it first. If the woken thread then goes back to sleep without setting the sleeper bit again, the word reads "taken, nobody sleeps": the next release wakes nobody, and the thread sleeps until something unrelated wakes it.

The author found this in the libc++ lock-based method while forcing it on x86-64: a byte-identical benchmark binary produced a clean curve on one run and a 300+ ms outlier on the next, with wall time and CPU time three to four orders of magnitude apart, the signature of a parked thread. The worst case came at the lowest contention tested (2 threads), so oversubscription could not explain it. The fix: set the sleeper bit again before every sleep, and sleep only on a word that still shows both bits. The libc++ code takes the wait table's monitor value, re-reads the word and parks only if both bits are still set.

LumexAtomic keeps the same rule. `lock ()` re-arms the sleeper bit on every round, and `wait_until_changed (word, state)` sleeps only while the word still equals the armed `state` (taken + sleeper): `std::atomic::wait` compares and sleeps atomically, and the table compares under the stripe mutex that the waking thread also takes. The test `LumexBitLockTest.GivenSleepingContenders_WhenTheLockChangesHands_ThenNoneSleepsForever` hangs (and its watchdog fails it) when the re-arming is removed.

### Operations under the lock

Each operation holds the lock only to copy, move or swap one smart pointer:

- `load` copies the stored value (the reference count increment is part of the atomic operation, as [util.smartptr.atomic] requires);
- `store` and `exchange` swap the new value in;
- `compare_exchange_*` compares the stored value with `expected` and either swaps `desired` in or copies the stored value into `expected`.

The previous value, a failed `desired` and the replaced content of `expected` are destroyed after the lock is released. A deleter that runs then may use the same atomic object without deadlocking on the non-recursive lock; the standard allows exactly that ("associated `use_count` decrements are sequenced after the atomic operation"). The tests run a deleter that loads, stores, exchanges, compares and waits on its own object for every operation that can release a value.

Equivalence ([util.smartptr.atomic]: the same stored pointer and shared ownership, or both empty) is `get ()` plus `owner_before ()` in both directions for `std::shared_ptr`. `std::weak_ptr` has no `get ()`; its stored pointer is read through `lock ()`, so two weak pointers with the same owner compare equal when the object is alive and both lock to the same pointer, or when the object is gone, because the stored pointers can no longer be observed. Only weak pointers made from aliasing shared pointers of an expired object can differ from `std::atomic<std::weak_ptr<T>>` there, which compares the hidden stored pointers.

### Memory orders

Every operation of the lock-based cell takes the lock with an acquire compare-exchange and releases it with a release exchange. Operations on one object are therefore totally ordered, each one synchronizes with the one before it, and they behave as `seq_cst`, including against other `seq_cst` atomics: a lock acquisition is a read-modify-write and reads the latest release. The memory order arguments are checked (above) and otherwise not needed; the libc++ lock-based method treats them the same way. The cell that wraps the standard type passes the orders through.

### wait and notify

`wait (old)` must block until it observes a value that is not equivalent to `old`, and it must notice a change of the stored pointer alone (an aliasing `std::shared_ptr` with the same owner). The `wait` of libstdc++ 13's `std::atomic<std::shared_ptr<T>>` sleeps on the word that holds the control block pointer: it returns when another thread merely takes the internal lock, and it keeps sleeping after a store that changes only the stored pointer. Both cells of LumexAtomic therefore wait the same way:

- `notify_one` and `notify_all` advance a 32-bit counter (release) and wake the threads that sleep on it;
- `wait` reads the counter (acquire), compares the current value with `old` and sleeps on the counter only while they are equivalent.

A notify between the comparison and the sleep changes the counter, so the sleep returns at once instead of losing the wake-up. As with `std::atomic`, a store without a notify does not wake a waiter, `wait` does not see a value that came and went (A-B-A), and it may wake spuriously. (The libc++ lock-based method also wakes value waiters on every store, which the standard allows as a spurious wake-up.)

Threads sleep through `sync/LumexAtomicWait.hpp`:

- From C++20, `wait` and `notify_*` of `std::atomic<std::uint32_t>`: a futex on Linux, `WaitOnAddress` on Windows.
- Before C++20 the standard has no address-keyed wait. A mutex and a condition variable in every object would add about 90 bytes to it and would make the constructors non-`constexpr` (the constructor of `std::condition_variable` is not), so `constinit` objects would be impossible. Instead one table of 64 stripes, each a `std::mutex` with a `std::condition_variable`, is keyed by the address of the word. Words that share a stripe share its condition variable: a wake-up reaches every sleeper of the stripe (`notify_one` acts as `notify_all`), and each sleeper re-checks its own word. The table is never destroyed, so objects with static storage duration can still wait and wake while the program exits. On ELF and Mach-O the table has default visibility, so one table serves the whole process even with hidden-visibility shared objects (checked by the `LumexCMake.consumer_atomic_cross_module` fixture). A Windows DLL gets a table of its own: before C++20, a thread that sleeps through one DLL is not woken through another DLL.

## The lock-free method of the libc++ implementation

### Design

On x86-64 with `CMPXCHG16B` and on AArch64 with LSE (`CASP`), the libc++ implementation stores the pair as one 16-byte, 16-byte-aligned word updated by a double-width compare-and-swap (DWCAS): the low 64 bits hold the stored pointer `T*`, the high 64 bits hold the control block pointer with a 16-bit local reference count in bits 48 to 63 (user-space pointers fit in 48 bits on both architectures; bits 0 and 1 stay zero). The local count is a split reference count in the style of Anthony Williams: it pins the control block while a `load ()` is between reading the word and incrementing the real reference count, which would otherwise be a use-after-free if a concurrent store released the last reference in that window.

- `load ()`: (1) DWCAS the word with the local count plus one (a "tick"); (2) `__add_shared ()` on the claimed control block, the reference the returned `shared_ptr` owns; (3) DWCAS the tick back off if the word still holds the same control block. A saturated local count (65535) makes the loader re-read and retry.
- `store ()` and `exchange ()`: if the new value has the same control block (an aliasing pointer), swap only `T*` and keep the local count, because zeroing it would retire ticks of loaders still in flight. Otherwise publish the new pair with a local count of 0 and "drain" the old one: `__add_shared ()` once per tick that was in the word, prepaying the references the in-flight loaders still owe, then release the atomic's own reference.
- `compare_exchange_*`: compare the current pair with `expected` (same `T*` and same control block, or both empty); on a match, the same same-control-block and drain logic as `store`; the strong form retries after a DWCAS that failed only because a loader moved the local count.
- `wait ()` watches the whole 16-byte pair, because an aliasing store changes `T*` without touching the control block half.
- `is_lock_free ()` is `true` in a translation unit that got this method; `is_always_lock_free` stays `false`, because another unit of the same program may have been compiled without `-mcx16`.

### The CAS livelock

A contended benchmark run (`load ()` then `compare_exchange_strong ()` in a loop, the client pattern of a copy-on-write update) hung at 16 threads. `compare_exchange_strong` waited for the local count to reach zero before trying its DWCAS, while every `load ()` on other threads kept raising it: the window in which the count was zero closed faster than the CAS could hit it. That is a livelock, not a deadlock: progress continued, but statistically almost never; single runs measured 90 to 110 times the baseline before hanging.

The first fix, dropping the wait, removed the livelock and then crashed with `double free or corruption` at 16 threads. The crash also appeared with the original code in a readers-heavy benchmark, so the defect was in the `store` / `load` pair, not in the CAS. AddressSanitizer runs pinned it down (the investigation's rule: never trust a reference-count argument on paper, run ASan, LeakSanitizer or ThreadSanitizer, and run every race 5 to 8 times with different thread counts). The accepted fix: the CAS never waits; it swaps the word with whatever local count it finds and drains like `store`, and it gained the same-control-block path that `store` and `exchange` already had. With 10 repetitions, the lock-free CAS ran at 7.6 to 17.6 times the baseline across 2 to 20 threads, against 14.3 to 21.0 for the lock-based method.

### The load () reference leak

The drain prepays one reference per tick, but the loader takes its own reference (`__add_shared ()` in step 2) regardless. When a writer swaps the word out after the loader's tick and before the loader's decrement, both references exist and only one is ever released: a bounded leak. The existing libc++ stress test leaked 4 objects in 6 of 6 ASan runs; an alternating-owner reproducer leaked 2 in 8 of 8.

Releasing an extra reference when the loader sees a different control block is wrong, because the control block may have gone A to B and back to A while the loader was in flight: the extra release then steals a reference that a live owner still holds, and both later release the same block (a real double free, reproduced). The implementation therefore keeps the leak, documented in the code, as the lesser evil. Closing it needs a version (epoch) tag next to the local count, so a loader can tell "my tick was drained" from "the word went away and came back"; that changes the layout of the 16-byte word, which is ABI in libc++, and was left out of the pull request. A related open point is ABA through reuse of a freed control block's address while a loader is still in its first loop.

### Why LumexAtomic does not port it

The lock-free method reads and writes libc++'s own control block (`__shared_weak_count`, `__add_shared ()`, `__release_shared ()`), adopts and detaches `shared_ptr`'s private `__ptr_` and `__cntrl_` as a friend, and packs the count into the control block pointer. Over the `std::shared_ptr` of libstdc++ or the MSVC STL none of that is possible without undefined behaviour. A portable lock-free variant would need a control block of its own (a node per stored value, that is an allocation per store) or hazard pointers: a different algorithm, not a port. Such a variant would not be bound by libc++'s ABI and could carry the epoch tag from the start.

## Examples

Two programs in `lumex/examples/atomic/` show every public member:

| Example | Shows |
| --- | --- |
| `example_atomic_smart_ptr.cpp` | The selected implementation, `load` / `store`, `exchange`, both compare-exchanges, a copy-on-write update loop, `atomic_weak_ptr`, `wait` / `notify_*` (built at C++11) |
| `example_atomic_config_reload.cpp` | A reloader publishes immutable configuration snapshots with a compare-exchange loop; workers read the current snapshot and sleep in `wait ()` until `notify_all ()` announces the next one; an `atomic_weak_ptr` remembers the last snapshot without keeping it alive |

## How it is verified

- Five GoogleTest suites run the same sources (CTest prefix `atomic.`): `LumexAtomicTests` (C++11, lock-based, table), `LumexAtomicCxx17Tests`, `LumexAtomicCxx20Tests` (the standard type where the library has it), `LumexAtomicLockBasedCxx20Tests` (forced lock-based) and `LumexAtomicWaitTableCxx20Tests` (forced table). Built and run with GCC 13.2 (Release and Debug), GCC 8.3, Clang 19 with libc++ and with libstdc++.
- The libc++ conformance tests of the pull request are ported (load, store, exchange, both compare-exchanges, conversion, types, wait, notify, `is_lock_free`, aliasing, nullptr, reference counts, stress), widened to six value types, both smart pointers and every memory order, plus a differential run against `std::atomic<std::shared_ptr<T>>`.
- Concurrency tests cover many threads, ABA, producers and consumers, the lost wake-up between the comparison and the sleep, and the two failure classes of the libc++ investigation: a CAS livelock under concurrent `load ()` (the test requires a minimum number of successful swaps) and reference leaks (a ledger checks that every object is deleted exactly once). Seeds and thread counts are printed and can be set (`LUMEX_ATOMIC_STRESS_SEED`, `LUMEX_ATOMIC_STRESS_THREADS`, `LUMEX_ATOMIC_STRESS_SCALE`).
- ThreadSanitizer and AddressSanitizer with LeakSanitizer and UBSan run the stress tests repeatedly with several thread counts.
- `LumexCMake.atomic_compile_checks` checks `[[nodiscard]]` and, with Clang, the rejected memory orders; `LumexCMake.consumer_atomic_cross_module` checks waking across hidden-visibility shared objects.

## Benchmarks

Contention benchmark with the author's libc++ method: every number is the time per operation divided by the `compare_exchange_strong` of a `std::atomic<std::uint64_t>` timed in the same process with the same threads, the median of 100 interleaved runs (GCC 13.2, libstdc++ 13, Intel Core i7-12700K, 20 logical CPUs).

![compare_exchange_strong () under contention](../../../benchmarks/atomic/results/atomic_benchmark_compare_exchange_strong.svg)

![load () under contention](../../../benchmarks/atomic/results/atomic_benchmark_load.svg)

- Under contention the lock-based implementation grows much more slowly than libstdc++ 13's `std::atomic<std::shared_ptr<T>>`: at 20 threads it costs 2.8-3.5 times less for `load ()`, 2.4-2.9 times less for `compare_exchange_strong ()`, 2.0-2.3 times less for `exchange ()` and 1.6-1.8 times less for `store ()`.
- libstdc++ wins `store ()` with 2 to 6 threads.
- Uncontended, `load ()`, `store ()` and `exchange ()` cost 17-22 ns in every implementation; the lock-based compare-exchange costs 44-47 ns against 57 ns.
- The default selection at C++20 (a wrapper over libstdc++'s type with a conforming `wait`) measures the same as libstdc++ itself.

The method, all series and charts, the machine, and the author's numbers for the two libc++ methods: `benchmarks/atomic/README.md` and `benchmarks/atomic/results/atomic_benchmark.md` in the repository.
