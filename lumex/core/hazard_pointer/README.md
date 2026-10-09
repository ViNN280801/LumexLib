# LumexHazardPointer: hazard pointers from C++11 {#lumex_hazard_pointer}

`lumex::core::hazard_pointer` (target `lumex::hazard_pointer`, a compiled library whose reader side is header-only) provides `hazard_pointer`, `hazard_pointer_obj_base<T, D>`, `make_hazard_pointer`, `swap` and the batch functions `make_hazard_pointer_batch` and `clear_hazard_pointer_batch`: the interface of `<hazard_pointer>` of the C++26 draft ([saferecl.hp], P2530R3, with the batch functions of P3428R4) from C++11 on, on every platform. The classes are the module's own on every standard and toolchain (no aliases of the `std` ones).

```cpp
#include "lumex/core/hazard_pointer/LumexHazardPointer"

namespace hp = lumex::core::hazard_pointer;

struct Config : hp::hazard_pointer_obj_base<Config>
{
  int limit = 0;
};

std::atomic<Config *> g_config (new Config);

int read_limit ()
{
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  Config *config = holder.protect (g_config); // announced, validated
  return config->limit;                       // safe: nobody deletes it
}

void reload (Config *fresh)
{
  Config *old = g_config.exchange (fresh);
  old->retire (); // deleted once no hazard pointer protects it
}
```

## Origin

Hazard pointers are Maged M. Michael's technique (2004): a thread announces the object it is about to use in a single-writer slot, issues a fence and checks that the shared pointer still names the object; a thread that removed an object hands it to the domain, which deletes it only when no slot holds its address. The interface is the C++26 one; the engine was written from the ideas of the public implementations (Folly, the libc++ proposal, libcds), no source text of them is copied, and `THIRD-PARTY-NOTICES.md` credits the designs.

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/hazard_pointer/LumexHazardPointer` | Umbrella: everything below |
| `base/LumexHazardPointerObjBase.hpp` | `hazard_pointer_obj_base<T, D>`, `is_hazard_protectable<T>` |
| `holder/LumexHazardPointerHolder.hpp` | `hazard_pointer`, `make_hazard_pointer`, `swap`, `clean_up` |
| `holder/LumexHazardPointerBatch.hpp` | `make_hazard_pointer_batch`, `clear_hazard_pointer_batch` |
| `engine/LumexHazardPointerEngine.hpp` | Slots, nodes, the exported entry points, the reader fence, the contract checks |
| `engine/LumexHazardPointerDomain.cpp` | The compiled engine: slot pool, thread caches, retired lists, passes |

## Interface

The names are in `lumex::core::hazard_pointer` and nowhere else (`LumexHazardPointerGlobalNames` checks that the module declares nothing globally). A class named like its namespace is legal; write `hp::hazard_pointer` after a namespace alias, or a `using` declaration.

| Name | Notes |
| --- | --- |
| `hazard_pointer` | Move-only owner of one hazard slot; one pointer in size; default and moved-from are empty |
| `empty ()` | `[[nodiscard]]` |
| `protect (src)` | Loads `src` (relaxed), then `try_protect` until it holds; `[[nodiscard]]` |
| `try_protect (ptr, src)` | Announce, fence, reload with `acquire`, compare; on failure `ptr` is the new value and nothing is protected |
| `reset_protection (ptr)`, `reset_protection ()` | End the old protection, start one without checking a source (for hand-over-hand traversals); a null pointer ends it |
| `swap`, free `swap` | Exchange the slots; no protection starts or ends |
| `make_hazard_pointer ()` | The only call that may throw (`std::bad_alloc` when a block of slots cannot be allocated) |
| `make_hazard_pointer_batch (span)`, `clear_hazard_pointer_batch (span)` | All or nothing; the span is `lumex::core::span::view::span`, which converts from `std::span` |
| `hazard_pointer_obj_base<T, D = std::default_delete<T>>` | `retire (D d = D ())`, `noexcept`; copy and move make a fresh, not retired node; assignment leaves the node alone |
| `is_hazard_protectable<T>` (own classes) | exactly one public, non-virtual base `hazard_pointer_obj_base<T, D>`; cv is ignored |
| `clean_up ()` (extension) | A pass now: every unprotected retired object is reclaimed before the call returns |
| `reclaim_or_retire (D d = D ())` (extension, member of `hazard_pointer_obj_base`) | Scans the hazard slots for this one object: reclaims it now when no slot names it (returns `true`, the deleter has run), else retires it like `retire` (returns `false`); `noexcept` |

Extension overloads of the module's own `hazard_pointer` (not in the standard):

| Member | Notes |
| --- | --- |
| `try_protect (ptr, source, filter)` | `source ()` loads the shared word (any ordering the structure needs, for example `seq_cst`) and `filter (word)` returns the `T *` it stands for, so tagged or packed words work |
| `protect (source, filter)` | the same, looped |

A type is hazard-protectable when it has exactly one public non-virtual `hazard_pointer_obj_base<T, D>` base; `try_protect`, `reset_protection (ptr)` and `retire` reject anything else at compile time (`static_assert`). The slot holds the address of the base's node, not the `T *`, so objects with the base at a nonzero offset (multiple inheritance) work.

## One implementation on every standard

The classes are the module's own on every C++ standard and toolchain: they are not aliases of the `std` ones, which no library ships yet, and they do not switch to them when one does. The API shape is the standard's, so moving to `std::hazard_pointer` later is a change of the namespace and, for the extension `clean_up ()` and the callable-source overloads, of the callers.

## The protocol

One process-wide domain owns every hazard slot and every retired object. A slot is one atomic word; a retired object carries a two-word node (a link and the function that reclaims it).

Reader (`try_protect`, header-inline): `slot.store (node, release)`; `reader_fence ()` (a `seq_cst` thread fence, the single place that chooses the fence); `src.load (acquire)`; compare; on a mismatch `slot.store (nullptr, release)`. The release stores matter: a store that replaces a protection must order the reader's last use of the old object before the reclamation that sees the new value.

Remover: the removing store to the source happens before `retire`; `retire_node` issues a fence, pushes the node on one of 8 lists (lock-free, release) and counts it.

Pass (compiled): take all lists, issue a `seq_cst` fence, read every slot (relaxed loads, acquire loads under ThreadSanitizer, which does not model fences) into a hash set, issue an acquire fence, reclaim every node whose address is not in the set, push the others back. The two fences are totally ordered: either the pass sees the announcement or the reader's reload sees the removal and fails. If the allocation of the hash set fails the pass matches node by node against the slots, so `retire` stays `noexcept`.

## Reclamation policy and the bound

There is no background thread. A `retire` that finds the number of waiting nodes at `max (1000, 2 * R)`, where `R` is the number of slots ever created, runs the pass on its own thread, and so does a `retire` that finds two seconds gone since the last pass (the clock is read on every 64th retire only). Deleters therefore run on the thread that retires, never on a thread that only reads. Only one pass runs at a time; a thread that finds one running goes on. A pass runs until the lists are empty, so a deleter may retire more objects and use hazard pointers (a deleter that calls `clean_up ()` returns at once).

The number of retired objects that are not yet reclaimed is bounded by `max (1000, 2 * R) - 1 + R + n`, where `R` is the number of hazard slots ever created, `R` bounds the objects a pass must keep (each slot protects at most one object), and `n` is the number of objects that other threads retire while a pass runs. `R` is the high-water mark of hazard pointers alive at the same time plus the slots parked in thread caches, so it is bounded by the number of threads times the cache size (8) plus the largest number of simultaneously live `hazard_pointer` objects. Nothing is reclaimed by a program that stops retiring: the last objects wait for the next `retire`, for the two-second period of a later `retire`, or for `clean_up ()`. Nodes still pending when the process exits are not reclaimed (the domain is never destroyed); call `clean_up ()` first when a leak checker must see them gone.

## Reclaiming one object at once

`retire` waits for a pass over all retired objects. `reclaim_or_retire` (an extension, also `engine::reclaim_or_retire`) is for a caller that wants one object gone inside the call, as `std::atomic<std::shared_ptr<T>>` destroys the replaced value inside `store`: after the removing store it issues the same `seq_cst` fence as a pass, reads every hazard slot for the one address of the object's node (the cost is proportional to the number of slots ever created, no allocation, no list of retired objects touched), and runs the deleter on the spot when no slot holds it. When a slot holds it, the object goes the ordinary `retire` way and is reclaimed by a later pass. The protocol argument is the pass's: either the scan sees a reader's announcement or the reader's validation sees the removal. The deleter runs on the calling thread with nothing locked, so it may use hazard pointers, retire other objects, call `clean_up ()` or call `reclaim_or_retire` again. The caller must not hold a hazard pointer that protects the object (its own slot would keep the object alive and send it down the retire path); `retired` and `reclaimed` of `statistics ()` count an object reclaimed this way once each.

## Slots, caches and threads

Slots are 64-byte records (`kLine`, 128 on Apple arm64 and ppc64) in blocks of 32 to 512 that are never freed. A free slot is in the cache of a thread (8 slots, used without any atomic operation: `make_hazard_pointer` and the destruction of a holder are wait-free on a cache hit) or in the shared pool, a lock-free stack in which one bit of the head word locks pops out so that the removal of the head cannot suffer from ABA. A cache miss takes that bit: only the slow path can wait for a preempted thread.

The cache is reached through a trivially destructible `thread_local` pointer, so there is no construction or destruction order to get wrong, and handed back by a thread-exit hook the operating system calls (a pthread key destructor on POSIX, a fiber local storage callback on Windows). A thread that is already past its hook, for example a later round of thread-exit destructors that makes and releases holders, finds a sentinel and uses the shared pool directly. The domain is constant-initialized and trivially destructible: hazard pointers can be made, used and released, and objects retired, from static constructors and destructors (`cmake.consumer_hazard_pointer_static_destruction`). A pthread key destructor lives in the library: the ELF library is linked with `-z nodelete` so that it stays mapped; on Windows the module pins itself (`GetModuleHandleEx` with `GET_MODULE_HANDLE_EX_FLAG_PIN`) when the first cache is created, so the DLL cannot be unloaded afterwards. The default (general dynamic) TLS model is used, so the library can be loaded with `dlopen`.

After `fork` in a multithreaded process the child must not use the module before `exec` unless the forking thread was the only one that used it (a pool pop may have been in progress in another thread).

## Exported symbols and DLLs

One domain per process is a safety property, so the engine is compiled into the library and every DLL shares it. The library exports seven free functions (`acquire_slot`, `acquire_slots`, `release_slot`, `retire_node`, `reclaim_or_retire`, `clean_up`, `statistics` in `lumex::core::hazard_pointer::engine`) through `LUMEX_HAZARD_POINTER_API`, keyed on `LumexCore_hazard_pointer_EXPORTS`; no class carries an export macro. Their signatures do not depend on the C++ standard (the library is compiled as C++11 and consumers at any standard link, `cmake.consumer_standard_mismatch_*` pattern). A static library linked into several shared libraries on Windows would give each its own domain: build the library shared (the default) or link it into one binary.

## Contract checks

`LUMEX_HAZARD_POINTER_DEBUG_ASSERT` checks that a hazard pointer used by `protect`, `try_protect` or `reset_protection` is not empty, in builds without `NDEBUG` (or with `LUMEX_HAZARD_POINTER_DEBUG_CHECKS=1`); the reader path stays free of branches otherwise. `retire` checks in every build, with `LUMEX_ASSERT`, that the object was not retired before and that a deleter is set. The deleter must not throw.

## Testing

`ctest -R '^hazard_pointer\.'` runs the unit and stress tests (one suite per standard, C++11 to C++20), `ctest -R hazard_pointer` also the CMake cases and consumer fixtures:

- unit tests of every member, the trait, the deleter kinds, copies, offsets, reentrancy;
- ABA tests over a recycling allocator that hands a freed address out again at once: a Treiber stack, a Michael-Scott queue and a Michael list set, scripted (deterministic) and stressed, protected (must pass) and unprotected (the script must damage them, which proves the tests can see ABA);
- `reclaim_or_retire`: unit tests (unprotected, protected, the caller's own protection, a node at a nonzero offset, a reentrant deleter, the counters), the scripted ABA interleaving over the immediate-reuse allocator (protected must keep the register right, unprotected must be damaged) and stress runs with readers and writers that replace the shared object and reclaim the old one, plus two hook cases (a stalled announcement is found by the scan; a reader that announces after the scan fails its validation);
- stress tests with a poisoned payload, producers and consumers, many readers and one writer and the reverse, thread churn, at 1, 2, 4, 8 and twice the cores' worth of threads, with a replayable seed (`LUMEX_HP_SEED`) printed at the start;
- long runs outside the default test run: `-DLUMEX_BUILD_SOAK_TESTS=ON` registers `ctest -L soak` (the ABA structures for `LUMEX_HP_SOAK_SECONDS` seconds, a new seed every round) and `ctest -L tsan` (the same under ThreadSanitizer, with Clang, or with GCC through `setarch x86_64 -R`, because GCC's runtime cannot map its shadow memory with ASLR on; GCC's ThreadSanitizer does not model fences, so the pass uses acquire loads there and a missing reader fence is not found by it, the hook fixture finds that);
- `cmake.consumer_hazard_pointer_hooks`: the engine built with test hooks, threads stalled at named points make the Dekker race deterministic;
- two shared libraries on one domain, static destruction, rejected uses (`hazard_pointer_compile_checks`);
- a deliberately wrong implementation without the fence (`LumexHazardPointerNaive`) that the stress harness is meant to catch.

`LUMEX_HP_THREADS`, `LUMEX_HP_SCALE` (percent of the work), `LUMEX_HP_MAX_THREADS` and `LUMEX_HP_SEED` steer the stress tests; a sanitizer shrinks the work to a tenth.

## Not done on purpose

No custom domains, cohorts, linked objects or executors (the standard has none either), no reserved space in the node. The asymmetric barrier (a compiler barrier for the reader and `membarrier` or `FlushProcessWriteBuffers` in the pass) is a later phase; the fence is chosen in `engine::reader_fence ()` and in the pass, and the entry points do not change when it comes.
