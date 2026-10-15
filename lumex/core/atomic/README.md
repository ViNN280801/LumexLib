# LumexAtomic: atomic_shared_ptr and atomic_weak_ptr from C++11 {#lumex_atomic}

`lumex::core::atomic` (target `lumex::atomic`, header-only) provides `atomic_shared_ptr<T>` and `atomic_weak_ptr<T>`: the interface of C++20's `std::atomic<std::shared_ptr<T>>` and `std::atomic<std::weak_ptr<T>>` (P0718R2, with LWG 3661 and LWG 3893) from C++11 on, over the ordinary `std::shared_ptr` and `std::weak_ptr` of any standard library. Three engines implement it, each under a class template of its own: a lock-free engine built on `core/hazard_pointer` (`atomic_shared_ptr_lock_free`), a lock-based engine of the module's own (`atomic_shared_ptr_lock_based`) and a wrapper of the standard library's type where it exists (`atomic_shared_ptr_std_backed`). The common names `atomic_shared_ptr` and `atomic_weak_ptr` are alias templates of the lock-free engine where it exists and of the lock-based one otherwise; the wrapper of the standard library's type is never chosen for you. A fourth engine, the split-count engine, is built on the module's own pointers of `core/smart_ptr` instead; it has its own section, "The split-count engine". The family alias pair `lumex::core::smart_ptr::atomic_shared_ptr` and `atomic_weak_ptr` is the lock-based engine over those pointers by default and the split-count engine only where `LUMEX_ATOMIC_SMART_PTR_FAMILY_PREFER_SPLIT_COUNT` is defined (x86-64).

```cpp
#include "lumex/core/atomic/LumexAtomic"

using lumex::core::atomic::smart_ptr::atomic_shared_ptr;

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

LumexAtomic ports the lock-based method. The lock-free method is described below, in "The lock-free method of the libc++ implementation", together with the reason it is not ported; the lock-free engine of this module is a different algorithm (next section, "The lock-free engine").

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/atomic/LumexAtomic` | Umbrella: everything below |
| `lumex/core/atomic/smart_ptr/LumexAtomicSharedPtr.hpp` | `atomic_shared_ptr_lock_free`, `_lock_based`, `_std_backed`, `_lock_based_lumex` and `_lock_free_split_count`, and the alias templates `atomic_shared_ptr` and `lumex::core::smart_ptr::atomic_shared_ptr` |
| `lumex/core/atomic/smart_ptr/LumexAtomicWeakPtr.hpp` | The same for `atomic_weak_ptr` |
| `lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp` | Which engines exist and which one the common names are (`LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE`, `_HAS_STD_BACKED`, `_COMMON_IS_LOCK_FREE`), the split-count and family macros (`_HAS_LUMEX_FAMILY`, `_HAS_SPLIT_COUNT`, `_FAMILY_IS_SPLIT_COUNT`), memory order checks |
| `lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrLumexFamily.hpp` | The equivalence traits of the module's own pointer family, used by the lock-based engine over it |
| `lumex/core/atomic/smart_ptr/split_count/LumexSplitCountWord.hpp` | The slot word of the split-count engine: a pure codec of its two halves |
| `lumex/core/atomic/smart_ptr/split_count/LumexSplitCountPolicy.hpp` | The policy the engine is a template on (the protocol steps, the events of a test ledger, the reserve, the tick limit) |
| `lumex/core/atomic/smart_ptr/split_count/LumexSplitCountTraits.hpp` | The strong and weak ledgers of the counters, the holder factories and the pointer traits of the family |
| `lumex/core/atomic/smart_ptr/split_count/LumexSplitCountCell.hpp` | The split-count cell: the protocol of the engine, in full |
| `lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrCell.hpp` | The lock-based and the standard-backed cell, the equivalence traits and the wait counter all cells share |
| `lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrBox.hpp` | The heap box of the lock-free engine and the `reclaim::immediate` / `reclaim::deferred` policies |
| `lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrLockFreeCell.hpp` | The lock-free cell |
| `lumex/core/atomic/sync/LumexBitLock.hpp` | The two-bit lock of the lock-based cell |
| `lumex/core/atomic/sync/LumexAtomicWait.hpp` | Waiting for a 32-bit word to change: `std::atomic::wait` or a striped table |
| `lumex/core/atomic/dwcas/LumexDwcasWord.hpp` | `dwcas_word`, a 16-byte atomic word with a 128-bit compare-and-swap (x86-64 only), and `dwcas_value_t`; includes the four headers below |
| `lumex/core/atomic/dwcas/LumexDwcasConfig.hpp` | `LUMEX_ATOMIC_HAS_DWCAS`, `LUMEX_DWCAS_BACKEND`, `LUMEX_ATOMIC_DISABLE_DWCAS`; macros only |
| `lumex/core/atomic/dwcas/LumexDwcasCpu.hpp` | `dwcas_supported ()`, `require_dwcas ()`: the run-time CMPXCHG16B check |
| `lumex/core/atomic/dwcas/LumexDwcasBackend{Asm,Msvc,Builtin}.hpp` | The three backends; `LumexDwcasValue.hpp` and `LumexDwcasFromCas.hpp` are their common parts |

Everything outside the class and alias templates is an implementation detail (`Detail` namespaces).

## Interface

All names live in `lumex::core::atomic::smart_ptr`, and only there: the module declares nothing at global scope, so a program that has its own global `atomic_shared_ptr` or `atomic_weak_ptr` keeps compiling (`LumexAtomicGlobalNamesTest` checks that). For the short names write a `using` declaration (`using lumex::core::atomic::smart_ptr::atomic_shared_ptr;`) in your own scope. The names:

| Name | Kind | Meaning |
| --- | --- | --- |
| `atomic_shared_ptr<T>`, `atomic_weak_ptr<T>` | alias templates | the common names: the lock-free engine where it exists, else the lock-based one; never the standard-backed wrapper |
| `atomic_shared_ptr_lock_free<T, Reclaim>`, `atomic_weak_ptr_lock_free<T, Reclaim>` | class templates | the hazard-protected box engine; declared only when `LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE` is 1; `Reclaim` is `reclaim::immediate` (the default) or `reclaim::deferred` |
| `atomic_shared_ptr_lock_based<T>`, `atomic_weak_ptr_lock_based<T>` | class templates | the two-bit lock engine; always declared |
| `atomic_shared_ptr_std_backed<T>`, `atomic_weak_ptr_std_backed<T>` | class templates | a wrapper of the standard library's `std::atomic<std::shared_ptr<T>>` and `std::atomic<std::weak_ptr<T>>`; declared only when `LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED` is 1 (C++20 with libstdc++ 12 and later or the MSVC STL); an explicit opt-in name, a class and not an alias of the standard type |

The common names are alias templates, so they cannot be forward-declared, partially specialized or befriended; name an engine to do that. Naming an engine that does not exist (`_lock_free` in a build without `core/hazard_pointer`, `_std_backed` without the standard type) is a compile error: the macros let code branch. The members of every engine follow `std::atomic<std::shared_ptr<T>>` and `std::atomic<std::weak_ptr<T>>`:

| Member | Notes |
| --- | --- |
| `value_type`, `is_always_lock_free` | `true` for the lock-free engine, `false` for the lock-based one, the standard type's value for the wrapper |
| default, `nullptr` (shared only) and value constructors | the default and `nullptr` constructors are `constexpr`, so `constinit` works (LWG 3661) |
| copy constructor and copy assignment | deleted |
| `is_lock_free ()` | `[[nodiscard]]`; `true` for the lock-free engine (in the steady-state sense, see below), `false` for the lock-based one, the standard type's answer for the wrapper (`false` in libstdc++ and the MSVC STL) |
| `store`, `operator=` | `operator= (nullptr_t)` for the shared pointer (LWG 3893) |
| `load`, `operator value_type` | `load` is `[[nodiscard]]` |
| `exchange` | |
| `compare_exchange_weak`, `compare_exchange_strong` | with one or two memory orders |
| `wait (old, order)`, `notify_one`, `notify_all` | from C++11, not only C++20 |

Every operation is `noexcept`. Constant memory order arguments that the standard forbids (`store` with `acquire`, `load` with `release`, a failure order of `release`, ...) are reported at compile time by Clang through the `diagnose_if` attribute, as libc++ does for `std::atomic`.

## Which engine a build has

Detection is in one place, `LumexAtomicSmartPtrConfig.hpp`:

| Macro | Value |
| --- | --- |
| `LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE` | 1 when `LUMEX_ATOMIC_HAS_HAZARD_POINTER` is defined, `ATOMIC_POINTER_LOCK_FREE == 2` and `LUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE` is not defined |
| `LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED` | 1 when the library has `std::atomic<std::shared_ptr<T>>` (`__cpp_lib_atomic_shared_ptr`) |
| `LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE` | `HAS_LOCK_FREE` and `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` is not defined |
| `LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY` | 1 when `LUMEX_ATOMIC_HAS_SMART_PTR` is defined (CMake: the target `lumex::smart_ptr` exists) |
| `LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT` | 1 when the family is present, `LUMEX_ATOMIC_HAS_DWCAS` is 1 (x86-64) and `LUMEX_ATOMIC_SMART_PTR_DISABLE_SPLIT_COUNT` is not defined |
| `LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT` | 1 when the family alias pair is the split-count engine: `LUMEX_ATOMIC_SMART_PTR_FAMILY_PREFER_SPLIT_COUNT` is defined, `HAS_SPLIT_COUNT` is 1 and `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` is not defined; 0 by default |

The family pair `lumex::core::smart_ptr::atomic_shared_ptr` and `atomic_weak_ptr` has its own choice, shown by the last macro and described in "The split-count engine"; the table above is about the common names of this module only.

`LUMEX_ATOMIC_HAS_HAZARD_POINTER` comes from CMake: `lumex::atomic` is still an INTERFACE target, and when the target `lumex::hazard_pointer` exists (`LUMEX_BUILD_HAZARD_POINTER=ON`, the default) it links it and defines the macro. The edge `atomic -> hazard_pointer` is soft, with no `lumex_require_module` line: `LUMEX_BUILD_ATOMIC=ON` with `LUMEX_BUILD_HAZARD_POINTER=OFF` is a valid configuration that has the lock-based engine only, and `cmake.install_header_only_without_utility` builds exactly that. A build that does not use CMake defines the macro itself and links the hazard pointer library. The macro is not derived from `__has_include`: the header can be present while the library is not linked. The Conan package always has both modules, so `core_atomic` requires `core_hazard_pointer`.

| Build | `atomic_shared_ptr` | Sleeping | Inline namespace |
| --- | --- | --- | --- |
| C++11 to C++17 with `lumex::hazard_pointer` | `atomic_shared_ptr_lock_free` | striped table | `table_wait` |
| C++20 with `lumex::hazard_pointer` | `atomic_shared_ptr_lock_free` | `std::atomic::wait` | `std_wait` |
| no `lumex::hazard_pointer`, or `LUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE` | `atomic_shared_ptr_lock_based` | as above | as above |
| `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` | `atomic_shared_ptr_lock_based` | as above | as above |
| `LUMEX_ATOMIC_WAIT_FORCE_TABLE` | as above | striped table | `table_wait` |

The wrapper `atomic_shared_ptr_std_backed` exists in addition where the library has the standard type, and is chosen only by naming it. **Behavior change from the lock-based and std-backed defaults** (MAJOR window, 2.0.0.0): a C++20 build with libstdc++ 12+ or the MSVC STL used to get the wrapper of the standard library's type from the common name; it now gets the lock-free engine, and a build without `core/hazard_pointer` gets the lock-based one. A program that relied on the library's type (an `is_lock_free ()` of `false` that was the library's answer, for instance) names `atomic_shared_ptr_std_backed`.

`LUMEX_ATOMIC_WAIT_USES_STD` reports the way of sleeping: `std::atomic::wait` when the library defines `__cpp_lib_atomic_wait`. The forcing macros exist for tests and benchmarks, which use them to run every engine at C++20 as well, like libc++'s `_LIBCPP_FORCE_LOCK_BASED_ATOMIC_SHARED_PTR`.

The engine is in the class template name and the way of sleeping is an inline namespace (`std_wait`, `table_wait`). A program may mix translation units built with different standards or switches: their types are distinct, so passing an object across such a boundary through a function signature fails to link instead of silently mixing two layouts. (A mismatch hidden inside a user type that holds the object is not detected, as with any other configuration macro.)

The libc++ implementation dispatches inside the standard library: the lock-free method on x86-64 with `CMPXCHG16B` (`__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16`, that is `-mcx16` or `-march=x86-64-v2`) and on AArch64 with LSE (`__ARM_FEATURE_ATOMICS`), the lock-based method everywhere else (PowerPC and AIX always). Without `-mcx16` the dispatch silently compiles the lock-based method; the libc++ benchmark therefore checks a label in its output to prove which method it measured. LumexAtomic has no such CPU-specific branch: the lock-free engine needs only pointer atomics.

## The lock-free engine

`atomic_shared_ptr_lock_free<T>` and `atomic_weak_ptr_lock_free<T>` keep the value in an immutable heap box, `{retire node; std::shared_ptr<T> value}` (32 bytes), published through one `std::atomic<box *>` (null stands for the empty value, so the default and `nullptr` constructors stay `constexpr`) next to the 32-bit wait counter: 16 bytes. A box is never modified after it is published, so readers copy its smart pointer concurrently (a const access), and a box a thread names with a hazard pointer (`core/hazard_pointer`, Maged Michael's technique) is not destroyed under it. The user-visible type stays the standard smart pointer: aliasing, custom deleters, `make_shared` and `enable_shared_from_this` work unchanged, no control block of this library exists, and no 128-bit atomic, `-mcx16`, `libatomic` or pointer packing is involved.

- `load`: announce the box, validate, copy its smart pointer, drop the announcement. Nothing in the cell is written; no reader waits for another thread.
- `store`, `exchange`: allocate a box (none for the empty value), swap the pointer in, hand the old box to the reclaim policy. `exchange` returns a copy of the old value made before the hand-over.
- `compare_exchange_strong`: protect the current box, test the equivalence of its value with `expected` (same stored pointer and shared ownership, or both empty); not equivalent: `expected` receives a copy and nothing is allocated; equivalent: build the new box once and swap it in with a pointer compare-exchange from the protected box. A protected box cannot have been freed and allocated again, so the pointer compare-exchange has no ABA problem and needs no version tag. When it fails because the cell changed, the loop protects the new box and tests again, so the strong form fails only when the value is not equivalent, and a retry happens only because another thread made progress: lock-free in the usual sense, and no operation ever waits for a counter that another thread moves (the livelock of the libc++ double-width method, below, cannot occur). The weak form makes one attempt.
- `wait`, `notify_one`, `notify_all`: the wait counter shared with the other engines.

**Destruction of the replaced value.** The `Reclaim` argument decides when the box a `store`, `exchange` or successful compare-exchange removed is destroyed, and with it the replaced value:

| Policy | What happens | Cost |
| --- | --- | --- |
| `reclaim::immediate` (default) | the writer scans the hazard slots for that one box and destroys it before the call returns when no reader names it; only a box some reader is copying at that moment is retired and destroyed by a later pass | one read of every hazard slot ever created per replacing operation |
| `reclaim::deferred` | the box is retired at once; a reclamation pass of the hazard domain destroys it later, on whichever thread runs the pass | `store` is cheaper; up to `max (1000, 2 * R) - 1 + R` replaced values stay alive (`R`: hazard slots ever created, see `core/hazard_pointer`) |

With the default policy a single thread, or any run where no reader is inside `load` at that instant, sees the destruction inside the call: `use_count ()` of the replaced value and the run of its deleter behave as with the lock-based engine (the lifetime suites check it). Under contention a value may live until a later replacing call. The deleter of a replaced value runs on the replacing thread, never on a thread that only reads, with no lock held, so it may use any atomic smart pointer. The destructor of the cell destroys the last box at once; with the immediate policy a cell that had to retire a box (a reader named it at that moment) also runs a pass of the hazard domain, so everything the cell ever held is destroyed by the time the cell is (a deleter may refer to objects that die with the atomic, as with the other engines). With the deferred policy the boxes retired earlier stay with the domain until a pass (`lumex::core::hazard_pointer::clean_up ()` forces one; the domain never reclaims at process exit, so call it before a leak checker looks), and the deleters of replaced values must outlive the atomic. `[util.smartptr.atomic]` sequences the `use_count` decrement after the atomic operation without requiring it to be part of it, which is what the deferred destruction relies on.

**`is_lock_free ()`.** `true`, and `is_always_lock_free` is `true`, in the steady-state sense of the standard's intent (as in Folly and the hazard pointer module): no operation waits for another thread. The first operation of a thread takes a hazard record from the domain (a wait-free cache hit afterwards), and a `store`, `exchange` or successful compare-exchange allocates a box, which may block in the allocator. An allocation failure inside a `noexcept` operation calls `std::terminate`, as does running out of hazard records.

**Memory orders.** Every operation publishes with at least release and observes with at least acquire, because the box contents travel through the pointer; `seq_cst` requests are honoured. `relaxed` and `consume` loads are acquire loads.

**Windows.** The templates are header-only; the hazard domain is compiled into `LumexCore_hazard_pointer` and exported as free functions, so every DLL of a process shares one domain and a box stored in one DLL is safely loaded in another. The pre-C++20 wait table stays a per-DLL static (documented below).

Algorithm credits (ideas only, no code): Maged M. Michael's hazard pointers, the announce-validate protocol of Daniel Anderson, Guy E. Blelloch and Yuanhao Wei for atomic reference-counted pointers, and Anthony Williams' split reference count, which the box deliberately does not need (see `THIRD-PARTY-NOTICES.md`).

## The lock-based implementation

### The lock word

The libc++ lock-based method keeps a spin lock in the two low bits of the control block pointer stored in the atomic object: a control block is at least 4-byte aligned, so bit 0 (taken) and bit 1 (a thread sleeps on the lock) are free. That works inside libc++, which owns `std::shared_ptr`. LumexAtomic works over the `std::shared_ptr` of whichever standard library the program uses and must not touch its layout, so the same two bits live in a 32-bit word of their own (`bit_lock`, `sync/LumexBitLock.hpp`), next to an ordinary `std::shared_ptr` or `std::weak_ptr`:

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
- Before C++20 the standard has no address-keyed wait. A mutex and a condition variable in every object would add about 90 bytes to it and would make the constructors non-`constexpr` (the constructor of `std::condition_variable` is not), so `constinit` objects would be impossible. Instead one table of 64 stripes, each a `std::mutex` with a `std::condition_variable`, is keyed by the address of the word. Words that share a stripe share its condition variable: a wake-up reaches every sleeper of the stripe (`notify_one` acts as `notify_all`), and each sleeper re-checks its own word. The table is never destroyed, so objects with static storage duration can still wait and wake while the program exits. On ELF and Mach-O the table has default visibility, so one table serves the whole process even with hidden-visibility shared objects (checked by the `cmake.consumer_atomic_cross_module` fixture). A Windows DLL gets a table of its own: before C++20, a thread that sleeps through one DLL is not woken through another DLL.

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

The lock-free method reads and writes libc++'s own control block (`__shared_weak_count`, `__add_shared ()`, `__release_shared ()`), adopts and detaches `shared_ptr`'s private `__ptr_` and `__cntrl_` as a friend, and packs the count into the control block pointer. Over the `std::shared_ptr` of libstdc++ or the MSVC STL none of that is possible without undefined behaviour. A portable lock-free variant would need a control block of its own or hazard pointers: a different algorithm, not a port. The lock-free engine above is the hazard pointer variant, over the standard smart pointers. A variant with a control block of its own (a split count with an epoch tag, reserved name `atomic_shared_ptr_lock_free_split_count`) needs its own shared pointer family and is a separate item.

## The 128-bit compare-and-swap word

`lumex/core/atomic/dwcas/LumexDwcasWord.hpp` (also pulled in by the umbrella) declares `lumex::core::atomic::dwcas::dwcas_word`: 16 bytes, `alignas (16)`, two `std::uint64_t` halves (`dwcas_value_t {lo, hi}`) read and written together by `load`, `store`, `exchange`, `compare_exchange_strong` and `compare_exchange_weak`, plus `speculative_load` (two relaxed 64-bit reads that may be torn, to be validated by a compare-and-swap). It is the hardware layer of the split-count engine of the atomic smart pointers and is usable on its own. A compare-and-swap returns the value it observed; the swap happened exactly when that equals the expected value.

```cpp
using namespace lumex::core::atomic::dwcas;

require_dwcas (); // once, from a constructor: terminates with a message on a CPU without CMPXCHG16B
dwcas_word word;
dwcas_value_t seen = word.load ();
for (;;)
  {
    dwcas_value_t next = { seen.lo + 1, seen.hi ^ seen.lo };
    dwcas_value_t found = word.compare_exchange_strong (seen, next);
    if (found == seen)
      break;
    seen = found;
  }
```

**Where it exists.** x86-64 only, 64-bit pointers, with GCC, Clang (clang-cl included) or MSVC: `LUMEX_ATOMIC_HAS_DWCAS` is 1. AArch64, the 32-bit targets and every other architecture are not supported: the macro is 0 and the names are not declared (naming one is a compile error, so code branches on the macro). `LUMEX_ATOMIC_DISABLE_DWCAS`, defined before the first include, turns the layer off on x86-64 too.

**Backends**, chosen at compile time (`LUMEX_DWCAS_BACKEND`):

| Backend | How | Default for |
| --- | --- | --- |
| `LUMEX_DWCAS_BACKEND_ASM` (1) | `lock cmpxchg16b` in GNU inline assembly; no `-mcx16`, no libatomic | GCC, Clang, clang-cl, MinGW |
| `LUMEX_DWCAS_BACKEND_MSVC` (2) | `_InterlockedCompareExchange128` | MSVC (`cl.exe`) |
| `LUMEX_DWCAS_BACKEND_BUILTIN` (3) | `__atomic_*` on `unsigned __int128` (GCC and Clang send it to libatomic: link `-latomic`) | nothing; define it for ThreadSanitizer |

GCC sends every 16-byte `__atomic_*` operation to libatomic, with or without `-mcx16`, and `__sync_*_16` without the flag does not link, so the assembly backend writes the instruction itself. ThreadSanitizer does not see inline assembly and would report races on data the word protects: build sanitizer runs with `LUMEX_DWCAS_BACKEND=3`. Each backend has its own inline namespace, so translation units that disagree on the backend never share a function definition; the layout is the same.

**Memory orders.** The members take `std::memory_order` like `std::atomic`. The assembly and MSVC backends ignore them: a locked instruction is a full barrier on x86-64 and the `"memory"` clobber (the intrinsic) is a compiler barrier, so every call behaves as a `seq_cst` read-modify-write, which satisfies any order the caller asks for. The C++ memory model does not know the instruction, so this is the layer's contract for the supported compilers, not a derived property; the built-in backend passes the orders through and is the one ThreadSanitizer understands. A weak compare-and-swap is the strong one: the instruction does not fail spuriously.

**A load writes.** The only instruction that reads 16 bytes atomically on every x86-64 CPU is the locked `cmpxchg16b`, which needs write access to its operand. So the halves are `mutable` and `load` is `const`, but the word must live in writable memory (a word on a read-only page faults on `load`; `speculative_load` does not) and a load takes the cache line exclusively like a store, so readers do not scale among themselves.

**Alignment and aliasing.** `cmpxchg16b` faults (SIGSEGV, an access violation on Windows) on an address that is not 16-byte aligned. `dwcas_word` is `alignas (16)` and `static_assert`s its size and alignment; a packed layout around it is rejected by the compilers as a warning, with one exception: Clang accepts `#pragma pack (1)` around a struct that holds a word and lays the word out at offset 1. Do not put a word under `#pragma pack`, in a packed struct or in a buffer you align by hand; before C++17 a plain `new dwcas_word` is only as aligned as `operator new` is (16 on x86-64 glibc and the MSVC x64 runtime), C++17 guarantees it. The operations are correct under strict aliasing and `-fno-strict-aliasing` alike. Do not touch the halves of a live word except through the members.

**The CPU.** Every x86-64 CPU since 2006 has `CMPXCHG16B` (Windows 8.1 requires it). `dwcas_supported ()` reads CPUID leaf 1, ECX bit 13, once (a constant-initialized `std::atomic<int>`, thread-safe, no static-initialization-order problem); `require_dwcas ()` writes one line to `stderr` and calls `std::abort ()` when the bit is missing. `dwcas_word` itself does not check, so a static word has a `constexpr` constructor; the objects built on the layer call `require_dwcas ()` from their constructors.

**Status of the backends.** Run and tested here: the assembly backend (GCC 13.2, GCC 8.3, Clang 23 with libstdc++ and libc++) and the built-in backend (the same compilers, libatomic), with the same suites, also under AddressSanitizer and UBSan and under ThreadSanitizer (Clang, and GCC through `setarch x86_64 -R`; both on the built-in backend) without a report; compiled for MinGW-w64. The MSVC wrapper is tested against a stand-in `_InterlockedCompareExchange128` written with the assembly (so the order of the arguments and halves is exercised) but has not been run with MSVC or clang-cl; run `ctest -R "^atomic\.dwcas\."` there. The checks of the configuration for AArch64, 32-bit and unknown compilers compile the configuration header with simulated predefined macros (`cmake.dwcas_compile_checks`).

**Cost.** Measured with `benchmarks/atomic` (`LumexDwcasBenchmark`): one attempt of the 128-bit compare-and-swap against one of the 64-bit one on a shared word, ratios of medians; see `benchmarks/atomic/README.md`. A reduced run (GCC 13.2, Intel Core i7-12700K) gave 1.31, 1.01, 1.34 and 1.44 times the cost of a 64-bit attempt at 1, 2, 4 and 8 threads.

**Tests.** `lumex/tests/core/atomic/dwcas/` (CTest prefix `atomic.dwcas.`; suites at C++11, 14, 17, 20, and the variants `builtin` and `msvc_wrapper` on the other backends): layout and every operation with every pattern of halves, the returned value of a compare-and-swap, every memory order, a const word, a misaligned word and a read-only page (faults in a child process), the CPU check with an injected CPUID answer (the abort and its message in a child process), tearing (writers store pairs with tied halves, readers assert the tie, a torn guess must never pass a compare-and-swap), lost updates, exchange as a permutation, the failure value, linearizability of short histories, message passing and a spin lock built on the word (judged by ThreadSanitizer on the built-in backend), and two deliberately broken words that the same scenarios must catch.

## The split-count engine

`atomic_shared_ptr_lock_free_split_count<T>` and `atomic_weak_ptr_lock_free_split_count<T>` are the fourth engine. The first three keep the value in a standard smart pointer and need the hazard pointer library. This one is built on the module's own pointers, `lumex::core::smart_ptr::shared_ptr` and `weak_ptr` (`core/smart_ptr`), whose control block has a split strong counter. The value is one 16-byte word, updated by a 128-bit compare-and-swap (`dwcas_word`), next to the 32-bit wait counter: the object is 32 bytes.

| Name | Meaning |
| --- | --- |
| `lumex::core::smart_ptr::atomic_shared_ptr<T>`, `atomic_weak_ptr<T>` | The family alias pair, declared by `lumex/core/atomic/LumexAtomic`: the lock-based engine over the same pointers by default, the split-count engine where `LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT` is 1 (opt-in) |
| `atomic_shared_ptr_lock_free_split_count<T>`, `atomic_weak_ptr_lock_free_split_count<T>` | The engine named explicitly, in `lumex::core::atomic::smart_ptr`; declared only where `LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT` is 1 |
| `atomic_shared_ptr_lock_based_lumex<T>`, `atomic_weak_ptr_lock_based_lumex<T>` | The lock-based engine over the family pointers; declared where `LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY` is 1 |

The family pair has the members of the table in "Interface". The family pair is the lock-based engine by default (see "Cost" below for how the two compare); define `LUMEX_ATOMIC_SMART_PTR_FAMILY_PREFER_SPLIT_COUNT` before the first include to make it the split-count engine (x86-64 only, no effect elsewhere), or name `atomic_shared_ptr_lock_free_split_count` / `atomic_weak_ptr_lock_free_split_count` directly. `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` makes the family pair the lock-based engine in every case and leaves the common names as they are. The family pair exists only when the pointer module is linked (the soft CMake edge of `lumex::atomic`, `LUMEX_ATOMIC_HAS_SMART_PTR`). The alias pair lives in the atomic module, because the pointer module does not depend on the atomic one.

### The word

Each slot is one `dwcas_word` (16 bytes) with a 32-bit epoch next to it. Its two halves:

| Half | Bits | Field | Meaning |
| --- | --- | --- | --- |
| `lo` | 0 | `H` | 1: the block is a holder (`ctl_holder` in the shared engine, `ctl_weak_holder` in the weak engine) |
| `lo` | 1..63 | `cb` | The control block address; `lo == 0` is the empty value |
| `hi`, `lo != 0` | 0..23 | `L` | Ticks: pins on this installation that are not yet taken back or paid |
| `hi`, `lo != 0` | 24..63 | `delta` | The stored pointer minus the block's anchor, in bytes, two's complement over 40 bits; 0 for a holder word |
| `hi`, `lo == 0` | 0..63 | `raw` | The stored pointer of an owner-less alias |

A control block has two counter words: `Z = {count:32, ext:32}` for the strong group (it also carries the implicit weak unit), and one for the weak ledger. A non-empty word owns one `count` unit of the group it names; a loaded pointer owns one unit of its owner's group. `use_count () = count + ext`, where `ext` counts the owners in transit described below. `ext` is never negative.

### The protocol in words

- **Pin.** A load does not own the block it knows only by its address, so it first raises `L` by one with a compare-exchange of the whole word (a tick). While a tick exists the block cannot be freed: either the word still names the block, or the writer that swapped it out has deposited the tick as a unit of `ext` before its swap.
- **Load.** The loader reads the block (its anchor, or the owner and the pointer of the holder) and then decides, with an acquire read, whether its own tick is still in the word. If it is, the loader counts one reference on the owner (this is the linearization point), then takes a tick back with a release compare-exchange; ticks are interchangeable. If the tick is already gone, the loader pays the unit the writer deposited for it. If the tick was swapped out, one read-modify-write counts the reference and pays the unit together.
- **Store and exchange.** A word without ticks is swapped by one compare-exchange. Otherwise the writer pins the block like a loader and deposits the ticks it sees, plus a reserve of 8 units, into `ext` before its swap; its own tick is not deposited, because the swap consumes it. After a successful swap the surplus leaves with the drop of the word's unit (`store`) or is returned before the hand-over (`exchange`). If the swap fails with the same block still in the word, the deposit is topped up and the swap retried; if the block was removed, the writer pays the deposit in one read-modify-write and starts again.
- **Compare-exchange.** `expected` owns its block, so no pin is needed: the writer deposits the ticks it sees plus the same reserve of 8 units as a `store` (`Policy::cas_reserve ()`, which is `reserve ()` unless a policy says otherwise; 0 switches it off), swaps, and returns the surplus with the drop of the old word's unit when the swap succeeds. When the swap fails, the weak form withdraws the whole deposit; the strong form, if the word now holds the same block and offset with other ticks, keeps the deposit, tops it up when the ticks outgrew it and swaps again, and withdraws it when the value is not equivalent. A compare-exchange whose `expected` is a holder-shaped alias pins the holder word like a loader and then deposits like a `store` (the ticks of the pinned word, its own excluded, plus the reserve).
- **Holders.** A stored pointer whose offset from the anchor does not fit in 40 bits is kept through a holder block that owns one reference on its owner (a strong one, or a weak one in the weak engine). A load of a holder word returns the owner, never the holder. In the shared engine every pin on a holder is mirrored on the owner's `ext`, so `use_count ()` of the owner sees it. The reference of the holder on the owner belongs to the word: whoever drops the word's unit drops it too.
- **Waiting.** Modifying operations (`store`, `exchange`, `compare_exchange_*`) do not notify waiters. `wait` returns only after `notify_one ()` or `notify_all ()`, as with the other engines.

### The guarantee

`use_count ()` of a family pointer never reports fewer owners than exist. So `use_count () == 1` means sole ownership. It never waits. It is exact when no operation on the atomic that holds the value is in flight. While a `store`, `exchange`, compare-exchange or `load` of that atomic is in flight, it can be higher than the owner count by the units of that operation (the windows below). A higher value can make a test for sole ownership fail, never pass wrongly.

The windows, where the count is above the owners, are:

- **(i)** a `store` or a successful compare-exchange between its swap and the drop of the old value: +1, the lagging decrement. `exchange` has no such window.
- **(ii)** a writer, a compare-exchange included, between its pre-transfer and the end of its call: the ticks it found plus the reserve (8 units in production). If the swap succeeds, the surplus leaves with the drop; if it fails, the deposit is topped up or undone. A weak compare-exchange that fails shows the reserve until its withdraw.
- **(iii)** a load whose pin has already counted itself, until its payment: +1.

A holder word adds no window: the owner reference of the holder goes to whoever drops the word's unit.

`use_count_settled ()` on the family pointers (`shared_ptr` and `weak_ptr`) is the waiting twin. It spins for a bounded time and then yields while owners are in transit, so it is exact apart from the lag of window (i). It can block for as long as a thread pinned on the value is suspended, it may not return while loads and stores of the same atomic keep running, and it must not be called from a signal handler on a pinned thread. It is `noexcept`.

### Bounds

| Field | Width | Bound | At the bound |
| --- | --- | --- | --- |
| `L` | 24 bits | 2^24 - 1 pins on one installation | A pin yields until a tick is taken back; unreachable in practice, and the tests lower the limit |
| `delta` | 40 bits, signed | [-2^39, 2^39) bytes from the anchor | A holder word (one allocation); an allocation failure ends the program (`std::terminate`) |
| `count` | 32 bits, 31 used | 2^31 - 1 owners | A debug assertion, as in `std` |
| `ext` | 32 bits | The threads holding a pin, plus the reserve of each pending writer | Not reached |

### x86-64 only

The engine needs the 128-bit compare-and-swap of `core/atomic/dwcas`, which exists on x86-64 with 64-bit pointers only (GCC, Clang, clang-cl and MSVC). On AArch64 and on every other target `LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT` is 0 and the two engine classes are not declared, so naming one is a compile error. Test for the engine with `LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT`; `LUMEX_ATOMIC_SMART_PTR_DISABLE_SPLIT_COUNT` turns it off on x86-64 too. Every operation terminates the program on a CPU without CMPXCHG16B; the check runs once, at the first use.

### Memory-order models

The simulator suites of the engine are sequentially consistent, and ThreadSanitizer sees only the built-in backend, so the memory orders of the protocol are also checked against the C++11 memory model with [Relacy](https://github.com/dvyukov/relacy), a race detector that searches the interleavings and some of the stale reads of a small program. The models are in `lumex/tests/core/atomic/smart_ptr/split_count/relacy/`. Relacy is vendored in `3rdparty/relacy` (the headers unmodified except that the line endings are LF, and the license) and is used only by these models; it is never installed. The models are not in the default build or the default test run; with `-DLUMEX_BUILD_RELACY=ON` (or `LUMEX_RELACY_DIR` given) they are ordinary CTest cases (labels `relacy` and `relacy_mutant`), so a plain `ctest` runs them too and `ctest -LE relacy` leaves them out.

**What is modelled.** The shared-engine cell is transcribed step by step with the orders of the code: the tick, the decision on an acquire read, the settle, the writer's pin, the deposit with the reserve `k`, the retry, the top-up and the undo, the surplus that returns with the drop, `store`, `exchange` with the hand-over (also of an empty pointer), both compare-exchanges (a block-shaped and a holder-shaped `expected`, and the retry of the strong form), the holder with the mirror on the owner, and the counters. The counter exists in the two flavours of the library: the default one (a `release` drop and an `acquire` fence when the counter reaches zero) and the one used under ThreadSanitizer (an `acq_rel` drop and no fence); every program is built and run with both. Three programs hold the models: `Block` (publication and load, load against store, two loaders, two writers, exchange, compare-exchange, a reset to an empty pointer, a strong retry), `Holder` (the same on holder words, two loads in one thread, a holder compare-exchange that matches and one that does not) and `Litmus` (store buffering and independent reads). Where a reserve applies each model runs with `k` = 0 and `k` = 2 (two also with 8); `k` is also the reserve of the compare-exchange (`cas_reserve`), so `k` = 0 runs the path of a compare-exchange without a reserve and `k` = 2 and 8 the path with it (the retained deposit of the strong form and its top-up, and the holder deposit). Every model checks, at every step, that no object is read after its disposal or without a happens-before edge from its creation, that a block is disposed once, that `count + ext` is never below the owners (G1) and that `ext` is not negative; at the end every word has no tick, every `ext` is zero and every `count` equals its owners. A payment that was decided on a read which is not an acquire read of the swap that made it necessary (also when the swap installs an empty pointer) is a data race on a ghost variable. The ghost check applies when the payer decides on the image of the one writer that swapped the block out; the models keep to one slow-path writer per block for it.

**What is not modelled.** The 128-bit word is one 64-bit Relacy atomic that packs the block, the ticks and the offset. Every modification of it is one read-modify-write with the order the library requests (`acquire`, `release`, `acq_rel`, `seq_cst` when the caller asks): `lock cmpxchg16b` is a single locked read-modify-write, at least as strong as that, so the model is the weaker, portable case. Its plain reads are not read-modify-writes: the `acquire` loads of the decisions (L5, L8), the relaxed speculative guesses, the relaxed reloads of the back-off and the speculative read of the destructor. The model makes the guess one relaxed read of the whole word, so a torn guess is not modelled (the simulator's torn policy covers it). Most guesses are validated by a whole-word compare-and-swap; the guess at L5 that decides to count is not, and that is harmless because the settle that follows re-validates it. Also not modelled: the weak engine (the same cell over the weak ledger), `wait` and `notify`, the tick limit and its back-off, `use_count` as an observer, and the weak and deallocation parts of a control block. Relacy orders the read-modify-writes of one atomic by the schedule, so a payment can never precede its deposit in the modification order of a counter there (the ghost variable stands in for it), and in the full and bounded searches a plain load reads only the latest or the previous store of an atomic (the random search sees the last three). A search over that restricted reads-from relation is evidence, not proof, whatever the number of executions.

**Running.** Configure with `-DLUMEX_BUILD_RELACY=ON` (the vendored Relacy is used; `-DLUMEX_RELACY_DIR=<a Relacy checkout>` replaces it) (and `-DLUMEX_RELACY_MUTANTS=ON` for the mutants below), build `LumexSplitCountRelacyBlock`, `LumexSplitCountRelacyHolder` and `LumexSplitCountRelacyLitmus` (and their `_TsanCounter` twins), and run `ctest -L '^relacy$'`. A program takes `--only <text>`, `--search random|bound|full`, `--iterations N`, `--bound B` and `--scale S`, and prints the protocol steps that its search reached. Most two-thread models are searched with the full scheduler (every schedule of the restricted reads-from relation: 16 thousand to 1.2 million executions); the others are bounded or random: two loads in one thread up to eight preemptions (1.6 to 3.9 million executions), the holder mismatch model up to eight and the strong retry model up to six, the 3- and 4-thread models up to four preemptions (two for the four-thread model), each followed by 2 million random schedules; the `Litmus` store-buffering models up to four preemptions, the independent-reads model up to two (168 thousand executions) and then 200 thousand random schedules, and the independent-reads controls on plain atomics random only. A run of both counters takes about 9 minutes in Release with GCC 13.2 on an idle machine (511 s measured for the six programs, see "Results"); the nineteen mutant cases take another 47 s. Relacy builds at C++11 to C++20 on GCC 13 and Clang 23 (the targets are pinned to C++17) and needs a library with `<source_location>`, so GCC 8 cannot build it; CMake checks this by compiling a Relacy header.

**Results.** All models pass with both counters: 93,039,008 executions per counter (`Block` 36,253,268, `Holder` 56,208,155, `Litmus` 577,585; the number of executions of every model is the same with the two counters). The programs took 92 and 94 s (`Block`, library and TSan counter), 161 and 162 s (`Holder`) and 1 s (`Litmus`) with GCC 13.2 in Release on an Intel Core i7-12700K (wall time of the whole program, one program at a time; six programs one after another take 511 s). The full table of the models, their searches and bounds, the number of executions and the seconds (library counter; the seconds are the elapsed wall time as Relacy reports it, in steps of 10 ms; "<0.01 (floor)" is the 1 ms that Relacy reports for a run shorter than one step, not a measurement):

| Model | Threads | Search | Reserve k | Executions | Seconds |
| --- | ---: | --- | --- | ---: | ---: |
| block.publish_load | 2 | full | 0, 2 | 39,841 / 39,841 | 0.09 / 0.07 |
| block.load_store | 2 | full | 0, 2 | 39,841 / 39,841 | 0.07 / 0.07 |
| block.exchange_load | 2 | full | 0, 2 | 42,947 / 96,502 | 0.08 / 0.19 |
| block.two_loaders_store | 3 | bound 4 | 0, 2 | 1,310,860 / 1,307,788 | 3.10 / 3.06 |
| block.two_loaders_store | 3 | random | 0, 2 | 2,000,000 / 2,000,000 | 5.03 / 5.05 |
| block.two_writers_load | 3 | bound 4 | 0, 2 | 484,234 / 483,146 | 1.12 / 1.12 |
| block.two_writers_load | 3 | random | 0, 2, 8 | 2,000,000 / 2,000,000 / 2,000,000 | 4.90 / 4.84 / 4.88 |
| block.publish_exchange | 3 | bound 4 | 0, 2 | 486,090 / 534,534 | 1.16 / 1.24 |
| block.publish_exchange | 3 | random | 0, 2, 8 | 2,000,000 / 2,000,000 / 2,000,000 | 4.97 / 4.94 / 4.91 |
| block.cas_load_store | 3 | bound 4 | 0, 2 | 3,603,484 / 3,588,990 | 10.54 / 10.18 |
| block.cas_load_store | 3 | random | 0, 2 | 2,000,000 / 2,000,000 | 6.48 / 6.31 |
| block.load_reset | 2 | full | 0, 2 | 16,156 / 16,156 | 0.03 / 0.02 |
| block.exchange_reset | 2 | full | 0, 2 | 17,401 / 39,722 | 0.03 / 0.07 |
| block.cas_restore | 2 | bound 6 | 0, 2 | 32,947 / 32,947 | 0.08 / 0.08 |
| block.cas_restore | 2 | random | 0, 2 | 2,000,000 / 2,000,000 | 3.82 / 3.82 |
| holder.load_store | 2 | full | 0, 2 | 229,361 / 229,361 | 0.51 / 0.52 |
| holder.exchange_load | 2 | full | 0, 2 | 244,673 / 1,248,512 | 0.56 / 2.93 |
| holder.cas_load | 2 | full | 0, 2 | 646,080 / 646,080 | 1.47 / 1.49 |
| holder.cas_miss_load | 2 | bound 8 | 0, 2 | 1,786,529 / 1,786,529 | 4.80 / 4.77 |
| holder.cas_miss_load | 2 | random | 0, 2 | 2,000,000 / 2,000,000 | 5.40 / 5.22 |
| holder.two_loads_in_one_thread_store | 2 | bound 8 | 0, 2 | 1,601,285 / 1,601,285 | 4.23 / 4.21 |
| holder.two_loads_in_one_thread_store | 2 | random | 0, 2 | 2,000,000 / 2,000,000 | 4.97 / 5.12 |
| holder.two_loads_in_one_thread_exchange | 2 | bound 8 | 0, 2 | 1,977,253 / 3,884,944 | 5.33 / 10.61 |
| holder.two_loads_in_one_thread_exchange | 2 | random | 0, 2 | 2,000,000 / 2,000,000 | 5.07 / 5.22 |
| holder.load_reset | 2 | full | 0, 2 | 95,517 / 95,517 | 0.21 / 0.20 |
| holder.exchange_reset | 2 | full | 0, 2 | 101,749 / 526,670 | 0.22 / 1.19 |
| holder.two_loaders_store | 3 | bound 4 | 0, 2 | 1,858,592 / 1,849,376 | 5.12 / 5.02 |
| holder.two_loaders_store | 3 | random | 0, 2 | 2,000,000 / 2,000,000 | 5.77 / 5.81 |
| holder.publish_load | 3 | bound 4 | 0, 2 | 1,891,183 / 1,886,287 | 5.97 / 5.88 |
| holder.publish_load | 3 | random | 0, 2 | 2,000,000 / 2,000,000 | 6.45 / 6.46 |
| holder.cas_load_store | 3 | bound 4 | 0, 2 | 1,583,966 / 1,578,446 | 4.72 / 4.66 |
| holder.cas_load_store | 3 | random | 0, 2 | 2,000,000 / 2,000,000 | 6.40 / 6.42 |
| holder.three_loaders_store | 4 | bound 2 | 0, 2 | 429,480 / 429,480 | 1.41 / 1.40 |
| holder.three_loaders_store | 4 | random | 0, 2 | 2,000,000 / 2,000,000 | 7.96 / 7.90 |
| litmus.sb_plain_weak_is_found (control: the weak outcome must be found) | 2 | full | - | 2 (to the first violation) | - |
| litmus.sb_plain_seq_cst | 2 | full | - | 20 | <0.01 (floor) |
| litmus.sb_cells | 2 | bound 4 | - | 4,380 | 0.02 |
| litmus.sb_cells_seq_cst | 2 | bound 4 | - | 4,380 | 0.01 |
| litmus.sb_cell_and_flag_seq_cst | 2 | bound 4 | - | 257 | <0.01 (floor) |
| litmus.iriw_cells_seq_cst | 4 | bound 2 | - | 168,528 | 0.40 |
| litmus.iriw_cells_seq_cst | 4 | random | - | 200,000 | 0.47 |
| litmus.iriw_plain_weak_is_found (control: the weak outcome must be found) | 4 | random | - | 18 (to the first violation) | - |
| litmus.iriw_plain_seq_cst | 4 | random | - | 200,000 | 0.07 |

`k` is the reserve of the writer in the model ("-": the model does not depend on it). A *full* search ran every schedule of the restricted reads-from relation and ended before its cap of 50 million executions; a *bound* search is exhaustive up to the given number of preemptions; a *random* search ran the given number of random schedules. The two *control* entries are plain atomics (no engine code): the search must find the weak outcome, and it does (the assertion fails at the execution shown and the entry counts as passed), while the same shapes with `seq_cst` orders pass the whole search. The programs also print the protocol steps their searches reached. Coverage counters exist for L3, L6, L7, L8, L9, S1 to S7, S10 to S12 (swap and withdrawal, retry; swap, mismatch, top-up and undo), `S10-keep` (a failed swap of the strong compare-exchange kept its deposit) and `S10-keep-topup` (and topped it up) and the disposal of a block, and between `Block` and `Holder` each of them was reached at least once; there are no counters for L0 to L2, L4, L5, S8 and S9, so the models do not show that those steps were reached.

Mutations of single orders, each built as its own program (`-DLUMEX_RELACY_MUTANT=<n>`, `ctest -L relacy_mutant`; a caught case passes only if the named model reports a data race at the expected site, except V4/201 to V4/203, the mutations of the reserve of the compare-exchange (the surplus one too big, the retained deposit not topped up, the holder deposit one short), which no race reveals and which pass when the named model fails its check of the balance; all nineteen cases give the verdict of this table). The last column names the first model that fails, the function of the model code in which Relacy reports the race and the number of the execution of the first report; the counter is the flavour of the counter drop (library: a `release` drop and an `acquire` fence on zero; TSan: the `acq_rel` drop without fence of ThreadSanitizer builds). M33a and M33b are the two halves of M33 (the loader's decision read, and the failure order of the writer's swap), each alone; V4/101 to V4/105 are mutants of the models only, with no number of the pre-registered list.

| Mutation | Counter | Verdict | First failing model, site and execution |
| --- | --- | --- | --- |
| M8 L3 tick of a load relaxed | library | caught | block.publish_load/k0; DATA RACE in load; found at execution 1 (full search) |
| M10 L7 un-tick relaxed | library | caught | holder.load_store/k0; DATA RACE in finish; found at execution 9 (full search) |
| M33 decision reads relaxed, load and swap failure | library | caught | block.load_store/k0; DATA RACE in witness_read; found at execution 47 (full search) |
| M33a (331) decision read of a load relaxed | library | caught | block.load_store/k0; DATA RACE in witness_read; found at execution 47 (full search) |
| M33b (332) failure order of the swap relaxed | library | caught | block.two_writers_load/k0; DATA RACE in witness_read; found at execution 5621 (bound 4 search; 3 in the random search) |
| M33 decision reads relaxed, swap to an empty pointer | library | caught | block.load_reset/k0; DATA RACE in witness_read; found at execution 34 (full search) |
| M33a (331) decision read of a load relaxed, swap to an empty pointer | library | caught | block.load_reset/k0; DATA RACE in witness_read; found at execution 34 (full search) |
| V4/101 no acquire fence after a drop that made the counter zero | library | caught | block.load_store/k0; DATA RACE in finish; found at execution 1 (full search) |
| V4/102 counter drop relaxed (no release) | library | caught | block.load_store/k0; DATA RACE in finish; found at execution 1 (full search) |
| V4/103 S1 swap of a tick-free word relaxed | library | caught | block.publish_load/k0; DATA RACE in load; found at execution 1 (full search) |
| V4/104 S2 writer's pin relaxed | library | caught | holder.publish_load/k0; DATA RACE in deposit; found at execution 45 (bound 4 search; 155 in the random search) |
| V4/105 S10 and S12 compare-exchange swap relaxed (block word) | library | caught | block.cas_load_store/k0; DATA RACE in finish; found at execution 1 (bound 4 search; 4 in the random search) |
| V4/105 S10 and S12 compare-exchange swap relaxed (holder word) | library | caught | holder.cas_load; DATA RACE in witness_read; found at execution 80 (full search) |
| V4/201 compare-exchange: the surplus of the swap one too big (block word) | library | caught | block.cas_load_store/k2; USER INVARIANT FAILED (`ext` negative); found at execution 1 (bound 4 search; 2 in the random search) |
| V4/202 strong compare-exchange: the retained deposit is not topped up | library | caught | block.cas_load_store/k2; USER INVARIANT FAILED (`ext` negative); found at execution 899,400 (bound 4 search; 88 in the random search) |
| V4/203 holder compare-exchange: the deposit is one short | library | caught | holder.cas_load/k2; USER INVARIANT FAILED (`ext` negative); found at execution 59 (full search) |
| M9 S4 and S12 swap success order without acquire | TSan | caught | holder.two_loads_in_one_thread_store/k0; DATA RACE in finish; found at execution 126746 (bound 8 search; 2138 in the random search) |
| M9 S4 and S12 swap success order without acquire | library | survives | the 8 entries of the CTest case (filter two_loads_in_one_thread) pass; the whole LumexSplitCountRelacyHolder_M9 program, 35 entries, also passes in a re-run of this page (152 s); masked by the acquire fence of the disposing drop |
| M25 rmw_order ignores seq_cst | library | survives | all models of the Litmus program pass (see the argument in the README) |

M9 is not redundant. With two loads of a holder word in one thread, the success of the writer's pinned swap is the only direct edge from the second un-tick of the loader to the writer's disposal of the holder. The counter of the library hides a missing edge, because the thread that makes the counter zero runs an `acquire` fence after a `release` drop, which synchronizes with every earlier drop of the counter and, through the counter chain, with the loader's reads. The counter used under ThreadSanitizer has an `acq_rel` drop and no fence, and there the success order is needed (the data race above). The order costs nothing on x86-64 and stays as written.

M25 survives, and the reason is narrower than "every access is a read-modify-write". Every operation that decides a user-visible result (`load`, `store`, `exchange`, `compare_exchange_*`) ends in an `acq_rel` read-modify-write of the word, and every modification of the word is at least `release`, so the release sequences are unbroken and the modification order of the word is contained in happens-before. That closes the cycle of the weak outcome in store buffering and in independent reads of independent writes without `seq_cst`; the control tests show that Relacy finds the weak outcome of the same shapes on plain atomics with weak orders and not with `seq_cst`. The plain loads of the protocol only choose between paying a unit and taking a tick back. `wait` and `notify` are not modelled. The argument fails if a plain store ever modifies the word, which is why the engine never uses `dwcas_word::store`.

### Cost

Measured with `benchmarks/atomic` (GCC 13.2, libstdc++ 13, Intel Core i7-12700K, 20 logical CPUs, one machine); the method, the tables, the quartiles, the code placement check and what is not measured are in `benchmarks/atomic/README.md`, section "The split-count engine". What the measurements show, as ratios to a contended 64-bit compare-exchange unless ns are given:

- Faster than the lock-based engine over the same pointer (`atomic_shared_ptr_lock_based_lumex`) for `store ()` and `exchange ()` with 4 or more threads (3.5 against 5.8 at 4 threads, 3.5 against 12.9 at 20), for `compare_exchange_strong ()` from 8 threads (16.0 against 19.2; 22.2 against 31.1 at 20), and, at 12 threads and more, for `load ()` against the C++20 build of the lock-based engine (9.1 against 12.2 at 20 threads; the C++11 build of the lock-based engine, 8.0, is ahead). Uncontended `store ()` and `exchange ()` are 9 to 12 % cheaper (18.3 and 19.2 ns against 20.9 and 21.1).
- Faster than `std::atomic<std::shared_ptr>` of libstdc++ 13 from 8 threads on (at 20 threads 3.4 times for `load ()`, 5.7 for `store ()`, 6.5 for `exchange ()`, 3.1 for `compare_exchange_strong ()`), and flat from 20 to 60 threads on 20 CPUs where the standard type grows to 98 for `load ()`.
- Slower uncontended `load ()` (29.4 ns against 18.0 to 19.1 for the lock-based engine, `std::atomic` and the box engine; 126 cycles and 111 instructions against 77 and 29 for the lock-based engine; the load path has three locked operations), `compare_exchange_strong ()` (60.9 ns, 25 % above the lock-based engine, equal to `std::atomic`) and the `load` plus `store` pair (47.8 against 39.6 ns). At 2 to 4 threads `load ()` costs 6.3 to 6.5 against 3.7 to 4.2 for the lock-based engine.
- Against the hazard-pointer box engine (`atomic_shared_ptr_lock_free`): `load ()` is 3 to 4 times dearer under contention (6.3 against 2.0 at 4 threads, 9.1 against 2.3 at 20) and `compare_exchange_strong ()` 1.5 to 2.1 times dearer from 8 threads on; `store ()` and `exchange ()` are cheaper (uncontended 18.3 against 45.6 ns, at 20 threads 3.5 against 4.4); the replaced value is destroyed within 0.8 us at the 99th percentile with 3 reader threads, the box engine's 99th percentile is 892 us.
- Against `boost::atomic_shared_ptr` (a spinlock): dearer `load ()`, `compare_exchange_strong ()` and read-heavy loads, cheaper uncontended `store ()` and `exchange ()`.
- Writers are not starved by readers in the read-heavy run (one writer, up to 19 readers, and up to 60 threads on 20 CPUs): the writer keeps storing, 0.5 million stores per second at 20 threads. The longest wait of a single store was not measured.
- `reserve` 4 and 8 measure the same in every scenario of the sweep. The reserve of `compare_exchange_*` (`cas_reserve ()`, equal to `reserve ()` by default) was evaluated afterwards: the contended `compare_exchange_strong ()` is 2 to 10 % cheaper, nothing else changes (section "Reserve in compare_exchange" of `benchmarks/atomic/README.md`). The numbers of the sweep above were measured before that change.

All of this is one machine, one compiler and one standard library. Differences of the lock-based engine below about 15 % move with code placement (a build with `-falign-functions=64 -falign-loops=64` shifts its contended `load ()` by up to 3.0 at 20 threads). Whether the family alias should be this engine is not decided by these numbers.

## Examples

The programs in `lumex/examples/atomic/` show the public members:

| Example | Shows |
| --- | --- |
| `example_atomic_smart_ptr.cpp` | The selected implementation, `load` / `store`, `exchange`, both compare-exchanges, a copy-on-write update loop, `atomic_weak_ptr`, `wait` / `notify_*` (built at C++11) |
| `example_atomic_config_reload.cpp` | A reloader publishes immutable configuration snapshots with a compare-exchange loop; workers read the current snapshot and sleep in `wait ()` until `notify_all ()` announces the next one; an `atomic_weak_ptr` remembers the last snapshot without keeping it alive |
| `example_atomic_split_count.cpp` | The family pair `lumex::core::smart_ptr::atomic_shared_ptr`, `load` / `store`, a compare-exchange loop, `use_count ()` and `use_count_settled ()`, the weak engine, and the detection macros with a fallback branch (built at C++11 when `lumex::smart_ptr` is built) |

## How it is verified

The verification of the split-count engine (what is claimed and what is not, the layers that check each property, every result with its bounds and limits, the mutation tables, the Relacy models, the sanitizer matrix and the charts) is written out in [VERIFICATION.md](VERIFICATION.md). The list below is the verification of the module as a whole.

- Five GoogleTest suites (CTest prefix `atomic.`, suffix `.cxx<std>`, variants `.lock_based.cxx20` and `.wait_table.cxx20`); each compiles the test files of its standard and the lower ones: `LumexAtomicCxx11Tests` (C++11, lock-based, table), `LumexAtomicCxx17Tests`, `LumexAtomicCxx20Tests` (the standard type where the library has it), `LumexAtomicLockBasedCxx20Tests` (forced lock-based) and `LumexAtomicWaitTableCxx20Tests` (forced table). Built and run with GCC 13.2 (Release and Debug), GCC 8.3, Clang 19 with libc++ and with libstdc++, and MSVC 19.51 x64 Release (2232 tests, 0 failed; the two `constinit` cases below C++20 skip).
- The libc++ conformance tests of the pull request are ported (load, store, exchange, both compare-exchanges, conversion, types, wait, notify, `is_lock_free`, aliasing, nullptr, reference counts, stress), widened to six value types, both smart pointers and every memory order, plus a differential run against `std::atomic<std::shared_ptr<T>>`.
- Concurrency tests cover many threads, ABA, producers and consumers, the lost wake-up between the comparison and the sleep, and the two failure classes of the libc++ investigation: a CAS livelock under concurrent `load ()` (the test requires a minimum number of successful swaps) and reference leaks (a ledger checks that every object is deleted exactly once). Seeds and thread counts are printed and can be set (`LUMEX_ATOMIC_STRESS_SEED`, `LUMEX_ATOMIC_STRESS_THREADS`, `LUMEX_ATOMIC_STRESS_SCALE`).
- ABA and linearizability suites (`lumex/tests/core/atomic/smart_ptr/`, written against an engine alias so the same bodies run on every engine): nodes come from an allocator that returns the address of the node just destroyed (object and control block), a stale handle that names only that address must not match the new node, `A -> B -> A` with the same owner matches and with an equal new object does not, aliasing pointers (same pointer under two owners, one owner under two pointers) follow the pointer-and-owner rule for every triple, `weak.lock ()` races the last strong release, `expired ()` is monotone. A Treiber stack, a Michael-Scott queue and a Harris-Michael sorted list of shared nodes run with 1, 2, 4, 8 and more threads than cores under four schedules (tight, yielding, jittered, cold caches) and are checked by multiset, per-producer FIFO, net inserts, a constructor/destructor ledger and the allocator balance; short histories go to a Wing-Gong search (register with compare-exchange, stack, queue, set). The same Treiber algorithm over raw atomic pointers with immediate address reuse is run through the forced ABA interleaving and must be reported (a version tag cures it). Every checker is a template over the engine and is itself falsified: the self-tests run it on broken copies (a compare-exchange that locks twice or not at all, equivalence of the pointer or the owner only, a store that keeps the old value, a stale load, a naive lock-free box that frees at once, a box published with relaxed orders) and require a report. Seeds, thread counts and work are set with `LUMEX_TEST_SEED`, `LUMEX_TEST_THREADS`, `LUMEX_TEST_SCALE`; a failure prints the replay line. Soak runs (`LUMEX_TEST_SOAK=1`, `-DLUMEX_BUILD_SOAK_TESTS=ON`, `ctest -L soak`) and ThreadSanitizer (`ctest -L tsan` in a `-DLUMEX_USE_TSAN=ON` tree) are separate runs.
- ThreadSanitizer and AddressSanitizer with LeakSanitizer and UBSan run the stress tests repeatedly with several thread counts.
- `cmake.atomic_compile_checks` checks `[[nodiscard]]` and, with Clang, the rejected memory orders; `cmake.consumer_atomic_cross_module` checks waking across hidden-visibility shared objects.

## Sanitizer matrix

The split-count suites (`atomic.smart_ptr.split_count.*`) and the family suites of the three test directories (`atomic.`, `atomic.smart_ptr.` and `atomic.sync.`, variants `split_count_engine` and `lock_based_lumex`, C++11 and C++20) were run on the rows below. The default selection is `-R 'atomic\.smart_ptr\.split_count\.|\.(split_count_engine|lock_based_lumex)\.cxx' -LE soak`: 4183 tests (4109 in the first run, which built the family targets of `atomic.smart_ptr.` only; the family targets of `atomic.` and `atomic.sync.` and the engine fallback tests were added since; a tree configured with the Relacy models adds their 22 CTest cases to the same selection). The soak selection is `-L soak` with the same filter (1036 tests; the simulator runs only in the C++11 suites with the default backend, so most of the tests of the soak selection are skipped by design). Counts are CTest results; CTest counts a skipped test as passed, so the table gives the passed and the skipped tests separately. A row has up to two phases: the first run, and, where the table shows one, a run after the two defects of the tests that the first run found were fixed (see "What the first run found"). The data of the table, one line per row, phase and kind of run, with seeds and the source of each number, is `benchmarks/atomic/results/verification/sanitizer_matrix.csv`.

| Row | Compiler, library, sanitizers, build | First run, default (4109 tests) | First run, soak (1036 tests) | After the fixes, default (4183 tests) | After the fixes, soak |
| --- | --- | --- | --- | --- | --- |
| 1 | GCC 13.2, libstdc++ 13, ASan + UBSan + LSan, Debug | 297 failed of 4109, skips not recorded; 2 runs (seeds default, 7001); UBSan misalignment in test code | 256 failed of 1036, skips not recorded; same class | 3410 passed, 773 skipped (of 4183) | run in two parts. Part 1 (1028 tests): 215 passed, 644 skipped by design, 169 not completed: 139 not run (39 C++11 and 100 C++20 tests of the `prefer_split_count` suites found no executable), 28 C++20 tests failed to start (loader error: `libLumexCore_hazard_pointer.so.2.0` not found), 2 C++11 tests killed after 187 s and 371 s; 87 other tests of the C++11 `prefer_split_count` suite passed before; the log has no oracle, UBSan or AddressSanitizer report. Part 2 (the 256 `prefer_split_count` simulator soak tests, which contain the 169): 128 passed, 128 skipped by design, none failed. Only 256 of the 1028 tests execute the simulator (128 in each of the two C++11 executables with the default backend); the other 772 are skipped by design |
| 2 | GCC 13.2, libstdc++ 13, ASan + UBSan, Release | 3 failed, 3333 passed, 773 skipped (of 4109); UBSan misalignment (`thread_counters_t`) | 264 passed, 772 skipped (of 1036); 2 runs (seeds default, 1111) | not run | not run |
| 3 | Clang 23.1, libstdc++ 13, ASan + UBSan, Debug | 3 failed, 3333 passed, 773 skipped (of 4109); 2 runs (seeds default, 7001); the same three tests in both runs | 264 passed, 772 skipped (of 1036) | 3410 passed, 773 skipped (of 4183) | not run |
| 4 | Clang 23.1, libc++, ASan + UBSan, Debug | 3 failed, 3331 passed, 775 skipped (of 4109); 2 runs (seeds default, 7001); the same three tests in both runs | 264 passed, 772 skipped (of 1036) | 3408 passed, 775 skipped (of 4183) | not run |
| 5a | Clang 23.1, libc++, TSan, Debug | 53 failed, 2979 passed, 1077 skipped (of 4109); 25 in the Cxx11 family suite, 28 in the Cxx20 one, none in the dedicated suites | 4 failed, 4 passed, 1028 skipped (of 1036); 2 runs (seeds default, 1111) | 3106 passed, 1077 skipped (of 4183) | 8 passed, 1028 skipped (of 1036) |
| 5b | Clang 23.1, libc++, TSan, Release | 53 failed, 2979 passed, 1077 skipped (of 4109) | 4 failed, 4 passed, 1028 skipped (of 1036); 2 runs (seeds default, 1111) | not run | not run |
| 5c | Clang 23.1, libstdc++ 13, TSan, Debug | 54 failed, 2980 passed, 1075 skipped (of 4109) | 4 failed, 4 passed, 1028 skipped (of 1036); 2 runs (seeds default, 1111) | 3108 passed, 1075 skipped (of 4183) | not run |
| 5d | Clang 23.1, libstdc++ 13, TSan, Release | 54 failed, 2980 passed, 1075 skipped (of 4109) | 4 failed, 4 passed, 1028 skipped (of 1036); 2 runs (seeds default, 1111) | not run | not run |
| 6 | GCC 13.2, libstdc++ 13, TSan, Debug, run under `setarch x86_64 -R` | 53 failed, 2981 passed, 1075 skipped (of 4109) | 4 failed, 4 passed, 1028 skipped (of 1036); 2 runs (seeds default, 1111) | 3108 passed, 1075 skipped (of 4183) | not run |
| 7 | GCC 8.3, Release, no sanitizer, C++11 targets only | 1641 passed, 136 skipped (of 1777) | 260 passed, 130 skipped (of 390); 2 runs (seeds default, 1111) | 1677 passed, 136 skipped (of 1813) | not run |
| 8 | GCC 13.2, libstdc++ 13, Release, no sanitizer, all targets | none of 4109 failed, passed and skipped not recorded | 264 passed, 772 skipped (of 1036); 5 runs (seeds default, 1111, 2222, 3333, 4444) | 3418 passed, 765 skipped (of 4183) | not run |
| fallback | GCC 13.2, Release, `LUMEX_ATOMIC_SMART_PTR_DISABLE_SPLIT_COUNT`, family suites at C++11 | - | - | 499 passed, 4 skipped (of 503); 3 of the skips are visible skips that name `lock_based_lumex` | - |

The soak runs are not repeated at full size on every row: the sanitizer rows ran the default seed and, on some rows, one more seed (1111); only row 8 ran five. The seed (`LUMEX_TEST_SEED`) changes the stress tests; the simulator uses a fixed seed (20261010) for its random schedules, so a soak run with another seed repeats the simulator work and varies only the stress tests. The rows that were run again after the fixes are 1 (default selection and the simulator soak tests), 3, 4, 5a (default selection and the soak run with seed 1111), 5c, 6, 7 and 8 (default selection); rows 2, 5b and 5d were not run again, and the soak runs of rows 3, 4, 5c, 6, 7 and 8 were not repeated. The failures of the first run in the TSan rows are in the family `split_count_engine` suites only; the dedicated suites passed on every TSan row. Only the two C++11 executables with the default backend (the default one and `prefer_split_count`) run the simulator tests, so most of the 1036 tests of the soak selection are skipped by design: of the 1028 simulator soak tests of row 1, 256 execute the simulator and 772 are skipped, and under ThreadSanitizer, where the simulator needs the replaced allocation functions that the TSan trees do not install, the soak run executes only a few tests and ends in under a minute; the simulator is covered by rows 1 to 4 and 8. The numbers of the first run of row 1, of the default run of row 3 with its default seed and of the default run and three of the soak runs of row 8 come from the run output of that time; the log files of those runs were lost. The skipped counts of those runs are not recorded, except where another run of the same row has a log (stated in `sanitizer_matrix.csv`). All other numbers are from the CTest logs.

What the first run found, and what was done about it:

- UBSan (rows 1 to 4): the tests held over-aligned objects where the allocation does not honour the alignment. `thread_counters_t` (`alignas (64)`) lived in a `std::vector` in C++11 (`LumexSplitCountExact.cxx11.tests.cpp`): it is now padded to 64 bytes without `alignas`, which keeps the false-sharing hint and needs no over-aligned allocation. `scenario_world_t` of the simulator (`LumexSplitCountSim.hpp`) lives on the stack, where the fake stack of AddressSanitizer aligns frames to 32 bytes only, so its `alignas (64)` storage member was misplaced: the storage is now aligned by hand inside a plain buffer. The static arenas keep `alignas (64)`. Row 1 counted 297 failures, 294 of them the type `scenario_world_t` at its storage member and 3 `thread_counters_t`; rows 2 to 4 only the three `thread_counters_t` tests (the `LumexSplitCountExactTest` case in the `.cxx11`, `.builtin.cxx11` and `.prefer_split_count.cxx11` suites). Fixed; the rows are clean.
- ThreadSanitizer (rows 5 and 6): the family `split_count_engine` targets ran on the inline-assembly backend, which ThreadSanitizer cannot see, and the publication of the control blocks was reported as a race (53 or 54 failed tests, first frames `ctl_base::anchor ()` against `ctl_base::ctl_base ()`). These targets now define `LUMEX_DWCAS_BACKEND=3` and link `libatomic` in a ThreadSanitizer tree (`lumex/tests/core/atomic/LumexAtomicFamilySplitCount.cmake`), the way the dedicated suites do. Fixed; the rows are clean.

How the TSan reports were attributed to the backend and not to the engine (the reduction): the four failing ABA and lifecycle tests of the family suite (a reuse pool, allocator boundaries that yield, lockers against the last owner, a weak pointer published after the strong one) were each run alone on the Cxx11 target of row 5a. Each failed alone in every configuration tried (default threads, 2 and 4 threads, default seed and seed 4242), with 8 to 264 TSan reports per run; the run with the fewest reports was `LumexAtomicSmartPtrAbaTest.GivenAllocatorBoundariesThatYield_WhenNodesAreRecycledAtOnce_ThenNoNodeIsStaleOrCorrupt` with 2 threads and seed 4242 (8 reports, 9 with the default seed); `...GivenLockersAndAnOwner_WhenTheLastStrongReferenceGoes_ThenPromotionIsLiveOrNull` gave 13 or 14 with 2 threads and 17 with the default threads. The same target built with the same flags and `LUMEX_DWCAS_BACKEND=3`: the 13 tests of the ABA suite passed in three of three runs (against 4 of 13 failing in three of three runs on the assembly backend), and the whole family `split_count_engine.cxx11` suite passed (485 of 485; 25 of 485 failed on the assembly backend, 3472 TSan warnings). The reports therefore come from the assembly backend that ThreadSanitizer cannot see; they were not a race of the engine. That is a statement about these reports, not a proof that the engine has no race on the assembly backend, which ThreadSanitizer cannot check (see the limits below).

Known limits: ThreadSanitizer covers the built-in backend only (the inline-assembly backend is invisible to it, so the TSan rows use `LUMEX_DWCAS_BACKEND=3`); the hardware `lock cmpxchg16b` path is covered by the ASan and UBSan rows and by the plain rows 7 and 8; the rows of this table have no Windows or MSVC run (the Windows runs that are recorded are in `VERIFICATION.md`). A variant of the suites whose engine a build lacks runs the lock-based engine instead and reports a skip that names the engine that ran (`LumexAtomicEngineFallbackTest`; the fallback row above is the build with the engine disabled). The repository's own gate (the CTest cases whose names start with `cmake.` or `lint.`) passes 196 of 196 with GCC 13.2; with a GCC 8.3 tree one case fails, `cmake.consumer_atomic_lock_free_hooks` (`free(): invalid pointer`), which fails the same way on the tree before the sanitizer work and concerns the hazard-pointer box engine, not the split-count engine; it was not investigated.

## Benchmarks

Contention benchmark with the author's libc++ method: every number is the time per operation divided by the `compare_exchange_strong` of a `std::atomic<std::uint64_t>` timed in the same process with the same threads, the median of 100 interleaved runs (GCC 13.2, libstdc++ 13, Intel Core i7-12700K, 20 logical CPUs).

![compare_exchange_strong () under contention](../../../benchmarks/atomic/results/atomic_benchmark_compare_exchange_strong.svg)

![load () under contention](../../../benchmarks/atomic/results/atomic_benchmark_load.svg)

![Compile time of one translation unit](../../../benchmarks/atomic/results/atomic_compile_time.svg)

- Under contention the lock-based implementation grows much more slowly than libstdc++ 13's `std::atomic<std::shared_ptr<T>>`: at 20 threads it costs 2.8-3.5 times less for `load ()`, 2.4-2.9 times less for `compare_exchange_strong ()`, 2.0-2.3 times less for `exchange ()` and 1.6-1.8 times less for `store ()`.
- libstdc++ wins `store ()` with 2 to 6 threads.
- Uncontended, `load ()`, `store ()` and `exchange ()` cost 17-22 ns in every implementation; the lock-based compare-exchange costs 44-47 ns against 57 ns.
- The default selection at C++20 (a wrapper over libstdc++'s type with a conforming `wait`) measures the same as libstdc++ itself.

The last chart is the compile time of one translation unit (`-fsyntax-only -O0`, median of 7, GCC 13.2 and Clang 23.1 with libc++) that includes the header and uses `load`, `store`, `exchange` and `compare_exchange_strong`: at C++20 the lock-based, the lock-free and the std-backed variants take 370 to 376 ms with GCC and 376 to 383 ms with Clang (the `include` unit), over a stub of 262 and 142 ms, against 388 ms and 425 ms for `boost::atomic_shared_ptr` and 263 ms for `std::atomic<std::shared_ptr>` with GCC; at C++11 the variants take 183 to 184 ms (GCC) and 219 to 220 ms (Clang) over 93 and 98 ms. The libc++ of Clang 23.1 has no `std::atomic<std::shared_ptr>`, so that variant and the std-backed wrapper are not in the Clang rows.

The method, all series and charts, the machine, and the author's numbers for the two libc++ methods: `benchmarks/atomic/README.md` and `benchmarks/atomic/results/atomic_benchmark.md` in the repository. The same sweep against the MSVC STL (Intel Core i9-12900H, MSVC 19.51) is in `benchmarks/atomic/results/msvc/`.
