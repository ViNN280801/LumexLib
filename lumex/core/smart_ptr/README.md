# LumexSmartPtr: shared_ptr and weak_ptr with an own control block {#lumex_smart_ptr}

`lumex::core::smart_ptr` (target `lumex::smart_ptr`, header-only, requires `lumex::utility`) provides `shared_ptr<T>`, `weak_ptr<T>`, `enable_shared_from_this<T>`, `make_shared`, `allocate_shared`, the four pointer casts, `owner_less`, `get_deleter`, `bad_weak_ptr` and the explicit conversions `from_std` / `to_std` to and from `std::shared_ptr`: the interface of the C++17 `<memory>` smart pointers ([util.smartptr]) from C++11 on, arrays `T[]` and `T[N]` included. They are the module's own classes on every standard and toolchain, never aliases of the `std` ones: the point of the family is its own control block, which the split-count engine of `core/atomic` is built on. `unique ()` (removed in C++20) and the array forms of `make_shared` (C++20) are not provided.

```cpp
#include "lumex/core/smart_ptr/LumexSmartPtr"

namespace sp = lumex::core::smart_ptr;

struct Session : sp::enable_shared_from_this<Session>
{
  sp::shared_ptr<Session> self () { return shared_from_this (); }
};

sp::shared_ptr<Session> session = sp::make_shared<Session> (); // one allocation
sp::weak_ptr<Session> observer = session;
sp::shared_ptr<Session> again = observer.lock ();             // empty once the last owner is gone

std::shared_ptr<Session> standard = sp::to_std (session);     // explicit wrapping, never implicit
sp::shared_ptr<Session> back = sp::from_std (standard);       // unwraps to the original
```

## Origin

The interface follows [util.smartptr] of the C++17 draft; the observable behavior of every operation the standard fixes is checked against `std::shared_ptr` on the toolchain (the tests run each scenario on both and compare). The control block is designed from the split reference count of Anthony Williams (the external counter of a lock-free `atomic<shared_ptr>`) and of Folly's `AtomicSharedPtr`: ideas only, no source text of either is copied; `THIRD-PARTY-NOTICES.md` credits them. The two proven corrections to those designs are in the protocol below.

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/smart_ptr/LumexSmartPtr` | Umbrella: everything below |
| `shared/LumexSharedPtr.hpp` | `shared_ptr`, comparisons, `swap`, `operator<<`, `std::hash`, `get_deleter`, the casts |
| `weak/LumexWeakPtr.hpp` | `weak_ptr`, `owner_less` |
| `shared/LumexEnableSharedFromThis.hpp` | `enable_shared_from_this` |
| `shared/LumexBadWeakPtr.hpp` | `bad_weak_ptr` (derives from `std::bad_weak_ptr`) |
| `make/LumexMakeShared.hpp` | `make_shared`, `allocate_shared` |
| `interop/LumexSmartPtrStd.hpp` | `from_std`, `to_std` |
| `ctl/LumexSmartPtrCounter.hpp` | `split_counter`: the packed `{count:32, ext:32}` word |
| `ctl/LumexSmartPtrCtlBase.hpp` | `ctl_base`: the 32-byte control block |
| `ctl/LumexSmartPtrCtlKinds.hpp` | `ctl_ptr` (pointer and deleter), `ctl_inplace` (object in the block), `ctl_holder` (out-of-window alias) |
| `detail/LumexSmartPtrAccess.hpp` | `detail::access`: the friend for the engine; the `enable_shared_from_this` probes |
| `detail/LumexSmartPtrTraits.hpp` | Type identity without RTTI, empty-base storage, `block_allocator`, "compatible with" traits |
| `detail/LumexSmartPtrConfig.hpp` | Macros: exceptions, ThreadSanitizer, debug asserts, the `std::enable_shared_from_this` policy |

## Interface

The names are in `lumex::core::smart_ptr` and nowhere else (`LumexSmartPtrGlobalNames` checks that the module declares nothing globally). Write `sp::shared_ptr` after a namespace alias (`namespace sp = lumex::core::smart_ptr;`). Note that `lumex::core::atomic::smart_ptr` is a different namespace (the atomic smart pointers); code inside `core/atomic` that wants this module writes `::lumex::core::smart_ptr`, because the plain name finds the atomic one.

| Name | Notes |
| --- | --- |
| `shared_ptr<T>` | Two pointers in size; all constructors, assignments, `reset` forms, `swap`, `get`, `*`, `->`, `[]` (arrays), `use_count`, `operator bool`, `owner_before`; `element_type` is `remove_extent_t<T>`; `weak_type` |
| `weak_ptr<T>` | `lock`, `expired`, `use_count`, `reset`, `swap`, `owner_before`; the promotion is an increment-if-nonzero |
| `enable_shared_from_this<T>` | `shared_from_this ()` (throws `bad_weak_ptr`), `weak_from_this ()`; set by every constructor that takes over a raw pointer and by `make_shared` / `allocate_shared` |
| `make_shared<T> (args...)` | One allocation for block and object; over-aligned types are aligned on every standard |
| `allocate_shared<T> (alloc, args...)` | One allocation made with `alloc` rebound to the block; `allocator_traits::construct` / `destroy` of an allocator rebound to the value type |
| `static_pointer_cast`, `dynamic_pointer_cast`, `const_pointer_cast`, `reinterpret_pointer_cast` | Lvalue and rvalue forms |
| `get_deleter<D> (p)` | Finds a deleter given to a constructor (not the implicit `delete`, not `make_shared`) |
| `owner_less<>` | `owner_less<shared_ptr<T>>`, `owner_less<weak_ptr<T>>`, transparent `owner_less<void>` |
| `from_std (p)`, `to_std (p)` | Explicit wrapping; a round trip returns the original pointer |

Differences from `std::shared_ptr`, on purpose:

- No implicit conversion to or from `std::shared_ptr`, no mixed comparison, no mixed `owner_before`. `from_std` / `to_std` wrap with an allocation and the result has its own `use_count` and owner.
- `shared_ptr<T> (Y *)` and `make_shared` reject, with a `static_assert`, a class that derives from `std::enable_shared_from_this` and not from the module's `enable_shared_from_this` (the standard base cannot be set by this class; a class may derive from both, and each family fills its own). `LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS` turns the check off.
- `bad_weak_ptr` is the module's class derived from `std::bad_weak_ptr`.
- Allocators with fancy pointers are not supported (a `static_assert` says so). `get_deleter` and the unwrapping by `from_std` / `to_std` use a type identity that is per shared library.
- A weak pointer does not cross the families (lock, convert, make a weak pointer from the result).
- `use_count ()` is `long`; counts are 32 bit (a debug assertion checks 2^31 - 1).

## Control block

`detail::ctl_base`, 32 bytes on a 64-bit target and polymorphic: the table pointer, the strong counter, the weak ledger and the anchor.

| Field | Contents |
| --- | --- |
| strong counter | One 64-bit atomic `Z = {count:32, ext:32}` |
| weak ledger | The same packing; it starts at 1, the implicit unit of the whole group of strong owners |
| anchor | The integer value of the first stored pointer of the block (the object for `make_shared`, the converted pointer for `shared_ptr<Base> (new Derived)`) |

`count` is the number of references taken by an ordinary addition. `ext` is a debt ledger used only by the split-count engine (see below); every program that does not call `transfer_ext` and `settle` sees `ext == 0` and an ordinary counter. The group ends when `count` and `ext` are both zero, which is one comparison of the whole word, made by the one read-modify-write that produces it: `dispose ()` destroys the object and the implicit weak unit goes; the block is deallocated by whoever brings the weak ledger to zero.

Memory orders, as in the standard libraries: taking a reference is relaxed; dropping is `release`, and the thread that sees the word become zero runs an `acquire` fence before it destroys the object, so every access of every other owner happens before the destruction. ThreadSanitizer does not model fences, so the drop is an `acq_rel` read-modify-write under it. The promotion of a weak pointer is an `acq_rel` compare-exchange that never moves a zero word.

Block kinds: `ctl_ptr<P, D, A>` (a separate object; deleter and allocator are stored with the empty-base optimization; `get_deleter` is `query`), `ctl_inplace<T, A>` (block and object in one allocation), `ctl_std` (owner of a `std::shared_ptr<void>`, the interop block) and `ctl_holder` (below). Blocks are finished through two private virtual functions, `dispose ()` and `destroy ()`.

## The contract with the split-count engine

The engine (`core/atomic`, phase 2) keeps a block pointer, a packed offset of the stored pointer and a count of "ticks" (loads in flight) in one 16-byte atomic word. The module exposes what it needs in `detail`, documented in the headers and pinned by tests:

- `ctl_base::add_strong ()`, `release_strong ()`, `transfer_strong_ext (n)`, `settle_strong ()`, `try_add_strong ()` and the same four for the weak ledger. `release_strong` and `settle_strong` dispose the object only when `count` and `ext` are both zero, so a block that a pinned reader or a pending transfer still refers to is never disposed (this is "release without destroying a still-referenced block"). `strong_counter ()` and `weak_counter ()` give the words.
- The protocol the engine runs on them: a reader pins the block with a tick in the atomic's word; it takes its own count with `add_strong` while pinned, then settles its tick (it takes a tick back from the word if one is there, otherwise it pays `settle_strong`); a writer that swapped the word out first `transfer_strong_ext (L)` for the ticks it took, then drops its own count with `release_strong`, never the other way round. Ticks are never turned into strong references, which removes the tick leak and the double release of the libc++ attempt: nothing is counted twice, and an address of a freed block cannot confuse any comparison, because every comparison is either a whole-word compare-exchange or made while the comparer holds a count.
- `detail::access` (friend of `shared_ptr`, `weak_ptr`, `enable_shared_from_this`): `control (p)` returns the block, `adopt<T> (cb, ptr)` / `adopt_weak<T> (cb, ptr)` build a pointer from a block and a stored pointer taking over one count the caller owns without touching the counters, `detach (p, ptr)` / `detach_weak` empty a pointer without dropping its count (the hand-over of `exchange`), `weak_count (p)`.
- `ctl_holder::create (owner, ptr)`: the block for an alias whose stored pointer is further from the anchor than the engine's offset field holds (beyond 40 bits). It owns one strong reference of `owner` and the pointer; a reader ticks the holder, reads `owner ()` and `pointer ()` and takes its count on the owner; disposing the holder drops the owner reference.
- `ctl_base::anchor ()`: the engine packs `stored - anchor`; the pointers set it once when they create the block.

## Thread safety

As [util.smartptr.shared.general]: distinct `shared_ptr` / `weak_ptr` objects that share a block may be copied, assigned and destroyed concurrently (the counters are atomic); one object needs external synchronization or the atomic smart pointers of `core/atomic`. `weak_ptr::lock` and `expired` are atomic against the last release. `use_count ()` is exact when no other thread changes the owners at that moment.

## Tests

`lumex/tests/core/smart_ptr/` follows the source tree; suites for C++11, 14, 17 and 20. The differential scenarios (`std_family` / `lumex_family` in `LumexSmartPtrTestSupport.hpp`) run each behavior on `std::shared_ptr` and on this module and compare the observations; the concurrent checkers (`LumexSmartPtrTestScenarios.hpp`) run with 1, 2, 4, 8 and more than the cores threads and four schedule kinds; the ABA tests recycle block addresses through `lumex_test::ReusePool` and prove with a negative control that the harness sees address reuse. The rejected uses are the compile checks `cmake.smart_ptr_compile_checks`. Every test carries the label `tsan`.
