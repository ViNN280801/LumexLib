# LumexCircularBuffer: a fixed-capacity ring buffer that overwrites its oldest element {#lumex_circular_buffer}

`lumex::core::circular_buffer::circular_buffer<T, Allocator>` (target `lumex::circular_buffer`, a header-only interface library that depends on `lumex::utility` for `LUMEX_ASSERT`) is a double-ended sequence of at most `capacity` elements: `push_back` into a full buffer destroys the oldest element and `push_front` into a full buffer destroys the newest. Element 0 is always the oldest, `operator[]` and the iterators are random access over that logical order, and the storage is one allocation of `capacity` slots made at construction. It is written for C++11 and works on every standard from there; from C++20 it adds `operator<=>`. It is modeled on `boost::circular_buffer` with a much smaller interface and is not thread-safe.

```cpp
#include <iostream>
#include <string>

#include "lumex/core/circular_buffer/CircularBuffer"

using namespace lumex::core::circular_buffer;

int
main ()
{
  std::cout << "=== Workflow: keep the last N instrument readings ===\n\n";

  circular_buffer<double> window (5);
  double const incoming[] = { 0.12, 0.15, 0.14, 0.91, 0.16, 0.17, 0.18 };
  for (double const sample : incoming)
    {
      window.push_back (sample);
      double sum = 0.0;
      for (circular_buffer<double>::size_type i = 0; i < window.size (); ++i)
        sum += window[i];
      double const mean = sum / static_cast<double> (window.size ());
      std::cout << "sample=" << sample << " window=";
      for (circular_buffer<double>::size_type i = 0; i < window.size (); ++i)
        {
          std::cout << window[i];
          if (i + 1U < window.size ())
            std::cout << ',';
        }
      std::cout << " mean=" << mean
                << " full=" << (window.full () ? "yes" : "no") << '\n';
    }

  std::cout << "oldest=" << window.front () << " newest=" << window.back ()
            << '\n';
  return 0;
}
```

From `lumex/examples/circular_buffer/example_circular_buffer_workflow.cpp`; the last line it prints is `oldest=0.14 newest=0.18`. The other examples are built and run by the tests (`examples.circular_buffer.*`).

A producer that outruns its consumer, the standard algorithms and the reverse iterators (`lumex/examples/circular_buffer/example_circular_buffer_queue.cpp`, prints `consumed sum=42` and `left: sum=23 largest=12 newest first: 12 11`):

```cpp
#include <algorithm>
#include <iostream>
#include <numeric>

#include "lumex/core/circular_buffer/CircularBuffer"

using namespace lumex::core::circular_buffer;

int
main ()
{
  std::cout << "=== Queue: a producer that outruns its consumer ===\n\n";

  // The producer pushes bursts of three; the consumer takes two per step. The
  // buffer holds four, so from the third step push_back drops the oldest
  // value.
  circular_buffer<int> queue (4);
  int next = 1;
  int consumed = 0;
  for (int step = 0; step < 4; ++step)
    {
      for (int i = 0; i < 3; ++i)
        queue.push_back (next++);
      for (int i = 0; i < 2 && !queue.empty (); ++i)
        {
          consumed += queue.front ();
          queue.pop_front ();
        }
      std::cout << "step " << step << ": size=" << queue.size ()
                << " front=" << queue.front () << " back=" << queue.back ()
                << '\n';
    }
  std::cout << "consumed sum=" << consumed << '\n';

  // Iterators are random access: the standard algorithms work on the logical
  // sequence, oldest first, whatever the position of the ring inside.
  int const sum = std::accumulate (queue.begin (), queue.end (), 0);
  int const largest = *std::max_element (queue.begin (), queue.end ());
  std::cout << "left: sum=" << sum << " largest=" << largest
            << " newest first:";
  for (circular_buffer<int>::const_reverse_iterator it = queue.crbegin ();
       it != queue.crend (); ++it)
    std::cout << ' ' << *it;
  std::cout << '\n';
  return 0;
}
```

Checked access, the zero capacity and the state of a moved-from buffer (`lumex/examples/circular_buffer/example_circular_buffer_checked.cpp`, extract):

```cpp
int
main ()
{
  std::cout << "=== Checked access, capacity and moved-from buffers ===\n\n";

  // A zero capacity is refused: there is no empty "unallocated" buffer.
  try
    {
      circular_buffer<int> none (0);
    }
  catch (std::length_error const &error)
    {
      std::cout << "capacity 0: " << error.what () << '\n';
    }

  circular_buffer<std::string> log (2);
  // front(), back() and operator[] are unchecked; at(), front_safe() and
  // back_safe() throw std::out_of_range, pop_front()/pop_back() do nothing
  // on an empty buffer.
  try
    {
      log.front_safe ();
    }
  catch (std::out_of_range const &)
    {
      std::cout << "front_safe on empty: out_of_range\n";
    }
  log.pop_front ();
  log.emplace_back ("start");
  log.emplace_back (3U, 'x'); // constructed in place: "xxx"
  std::cout << "at(1)=" << log.at (1) << '\n';
  try
    {
      log.at (2);
    }
  catch (std::out_of_range const &)
    {
      std::cout << "at(2): out_of_range\n";
    }

  // A moved-from buffer has capacity 0: assign to it before the next push.
  circular_buffer<std::string> moved (std::move (log));
  std::cout << "moved-from capacity=" << log.capacity ()
            << " size=" << log.size () << '\n';
  log = circular_buffer<std::string> (4);
  log.push_back ("again");
  std::cout << "reused capacity=" << log.capacity ()
            << " front=" << log.front () << '\n';
  return 0;
}
```

An allocator sees one allocation of `capacity` slots (`lumex/examples/circular_buffer/example_circular_buffer_allocator.cpp`, prints `4000 bytes requested` after construction and after 5 000 `push_back`, then `1` for the equality of the allocators):

```cpp
int
main ()
{
  std::cout << "=== Allocator: one allocation of capacity slots ===\n\n";

  circular_buffer<int, counting_allocator<int>> buf (1000);
  std::cout << "after construction: " << g_bytes << " bytes requested\n";
  for (int i = 0; i < 5000; ++i)
    buf.push_back (i);
  std::cout << "after 5000 push_back: " << g_bytes
            << " bytes requested, size=" << buf.size ()
            << " front=" << buf.front () << '\n';
  counting_allocator<int> const same;
  std::cout << "get_allocator () equals a new counting_allocator: "
            << (buf.get_allocator () == same
                && !(buf.get_allocator () != same))
            << '\n';
  return 0;
}
```

## Origin

The design and the names follow `boost::circular_buffer` (Boost.CircularBuffer, Jan Gaspar): a fixed capacity, `push_back` that overwrites the oldest element, `push_front` that overwrites the newest, index 0 is the oldest, `front`, `back`, `capacity`, `full`, `swap`. The class keeps the part of the Boost interface that a bounded queue or a sliding window needs and nothing else (no capacity change, no insertion in the middle, no linearization; see the comparison). It needs no Boost: the header depends on the standard library and `lumex::utility` only. It adds `emplace_back`, `emplace_front` and checked `front_safe` and `back_safe`, which Boost does not have.

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/circular_buffer/CircularBuffer` | Umbrella: includes `CircularBuffer.hpp` |
| `lumex/core/circular_buffer/CircularBuffer.hpp` | `circular_buffer`, its `iterator` and `const_iterator`, the six comparison operators and, from C++20 with `<compare>` and `<concepts>`, `operator<=>` |

The module has no compiled part. The CMake target `lumex::circular_buffer` is an interface library that adds the include path and links `lumex::utility` (the header includes `LumexAssert.hpp`, `LumexCheckFeatures.hpp`, `LumexConstantMacros.hpp` and `LumexKeywords.hpp`). There is no export macro and no global alias: the class is used as `lumex::core::circular_buffer::circular_buffer`.

## Interface

The facts are from `CircularBuffer.hpp`.

| Group | Members |
| --- | --- |
| types | `value_type`, `allocator_type`, `size_type` (`std::size_t`), `difference_type` (`std::ptrdiff_t`), `reference`, `const_reference`, `pointer`, `const_pointer`, `iterator`, `const_iterator`, `reverse_iterator`, `const_reverse_iterator` |
| construction | `explicit circular_buffer (size_type capacity, allocator_type const & = allocator_type ())`; copy (same capacity, same logical content); move (`noexcept`, leaves the source with capacity 0). There is no default constructor, no fill, range or `initializer_list` constructor. A capacity of 0 throws `std::length_error` |
| assignment | copy (copy-and-swap: allocates a new block, takes the capacity of the source) and move (`noexcept`). There is no `assign` |
| capacity | `capacity`, `size`, `empty`, `full` (all `noexcept`). There is no `max_size`, `set_capacity`, `resize` or `reserve`: the capacity never changes after construction |
| access | `operator[]` (unchecked), `at` (throws `std::out_of_range`), `front`, `back` (unchecked, `LUMEX_ASSERT` when `NDEBUG` is not defined), `front_safe`, `back_safe` (throw `std::out_of_range` on an empty buffer); each in a const and a non-const form. Index 0 is the oldest element |
| adding | `push_back` (copy, move), `push_front` (copy, move), `emplace_back`, `emplace_front` (return a reference to the new element). On a full buffer `push_back` and `emplace_back` destroy the oldest element first and `push_front` and `emplace_front` the newest |
| removing | `pop_front`, `pop_back` (no-op on an empty buffer), `clear` (keeps the capacity) |
| iteration | `begin`, `end`, `cbegin`, `cend`, `rbegin`, `rend`, `crbegin`, `crend`; random-access iterators over the logical order |
| other | `swap` (member and `friend`, O(1)), `get_allocator` |
| comparison | `==`, `!=`, `<`, `<=`, `>`, `>=` (lexicographic over the logical sequence; the capacity is not compared), `operator<=>` from C++20; it returns `std::strong_ordering`, so it compiles only for elements whose `<=>` gives `strong_ordering` (`circular_buffer<double>` does not compile) |

Not there, and present in `boost::circular_buffer`: default construction, `set_capacity`, `resize`, `rset_capacity`, `rresize`, `insert`, `rinsert`, `erase`, `rerase`, `erase_begin`, `erase_end`, `rotate`, `linearize`, `is_linearized`, `array_one`, `array_two`, `assign`, `max_size`, the fill, range and `initializer_list` constructors (a compile-time probe of each name against Boost 1.92.0 gave the table in the comparison). There is no `data ()` and no way to get a contiguous view of the content.

## How it works

- **Layout.** `sizeof (circular_buffer<T>)` is 40 bytes on x86-64 Linux with GCC 13.2 for every `T` (two empty allocator members with padding take 8 bytes, then the slot pointer, the capacity, the size and the head, 8 bytes each). The class is standard layout and not trivially destructible. The storage is one `Slot` array (`alignas (T) unsigned char[sizeof (T)]`) allocated through the allocator rebound to `Slot`; elements are constructed on demand, so `T` need not be default constructible. The heap block is exactly `capacity * sizeof (T)` in one call (4 096 bytes for 1 024 `int`), allocated at construction whether or not anything is pushed.
- **Indexing.** The physical index of logical index `i` is `(head + i) % capacity` with a run-time capacity, so every `operator[]`, `front`, `back`, iterator dereference and every step of `push_back` and `pop_front` executes a 64-bit division (`div` in the GCC 13.2 disassembly of the loops). This is the main cost against Boost, which keeps pointers and wraps with a compare (see the comparison).
- **Iterators.** An iterator is a pointer to the buffer and a logical index. It stays dereferenceable as long as the buffer lives, but it names a logical position, not an element: after a `push_back` into a full buffer an iterator to `begin ()` dereferences to the new first element (probe: it read 1 before the overwrite and 2 after), and an `end ()` iterator taken before a `pop_front` no longer equals `end ()`. `pop_front`, `push_front`, an overwriting `push_back` and `clear` invalidate all iterators in that sense; a `push_back` into a non-full buffer keeps them (only `end ()` changes); `pop_back` invalidates the iterators to the last element and `end ()`. After `swap` an iterator keeps pointing into the buffer object it was taken from, which now holds the other content; after a move it points into the moved-from object. References and pointers to elements stay valid until that element is destroyed; the elements never move.
- **Complexity.** O(1): access, `front`, `back`, `push_*`, `emplace_*`, `pop_*` (also with overwrite), `swap`. O(n): copy, `clear` for a non-trivially destructible `T`, iteration, comparison.
- **Exception safety.** `push_back` and `emplace_back` on a non-full buffer: strong. On a full buffer: basic. The oldest element is destroyed first, then the new one is constructed; if that throws, the buffer is consistent and holds one element fewer (probe: capacity 3, throwing copy constructor, size 2 after the exception). `pop_*`, `clear`, `swap` and the move operations do not throw. The copy constructor and the copy assignment, which copies through it, throw what `T` or the allocator throws and leak the storage and the elements copied so far (see the known issues), so they leak on an exception (the copy assignment still leaves `*this` unchanged).
- **Aliasing.** `push_back (buf.front ())` on a full buffer reads the element after it was destroyed (see the known issues).
- **Moved-from buffers.** The move constructor and move assignment take the storage and leave the source with capacity 0, size 0 and no storage. `clear`, `pop_front`, `pop_back`, `size`, `empty`, the destructor and assignment to it are fine; `push_*` and `emplace_*` on it compute `% 0`, which is undefined behavior (GCC 13.2 kills the process with SIGFPE, SIGSEGV or SIGILL depending on `T` and the optimization level). A moved-from buffer reports `empty ()` and `full ()` both true. Assign a new buffer before reusing it.
- **Thread safety.** None. The class has no locks and no atomics; concurrent access from several threads, including one producer and one consumer, needs external synchronization. The tests check only that independent instances do not interfere (`ThreadSafety_MultipleIndependentInstances`).
- **Allocator.** `std::allocator<T>` by default; the allocator is rebound to the slot type and used through `std::allocator_traits`. `get_allocator`, `select_on_container_copy_construction` on copy construction and `propagate_on_container_swap` on `swap` are honored; the move operations always move the allocator along with the storage. The storage pointer is a plain `Slot *`, so an allocator whose `pointer` is a fancy pointer does not work. Copy assignment between buffers with unequal allocators that do not propagate keeps its allocator and frees memory of the other one (see the known issues).
- **`constexpr`.** No member is `constexpr` or usable at compile time (heap allocation, `reinterpret_cast`).
- **Debug checks.** `front ()` and `back ()` assert `!empty ()` when `NDEBUG` is not defined; `operator[]` has no check; `swap` asserts equal allocators when they do not propagate.

## Comparison with existing solutions

Measured with [benchmarks/circular_buffer](../../../benchmarks/circular_buffer/README.md): GCC 13.2.0, libstdc++, Release (`-O2`, `-march=x86-64`), no sanitizers, Intel Core i7-12700K pinned to one core, idle machine under the build lock, 5 passes, Boost 1.92.0 headers. Clang, libc++, MSVC and other CPUs were not measured. The alternatives measured are `boost::circular_buffer` (the library it is modeled on), `std::deque` used as a bounded queue and `std::vector` with manual head and size (a hand-written ring, with the wrap done by a compare and, separately, by `%` as this library does). Not measured: `boost::circular_buffer_space_optimized` (a different allocation policy), lock-free ring queues such as `boost::lockfree::spsc_queue` (they solve a different problem: thread-safe, no overwrite) and the standard library of any compiler other than libstdc++ 13.2.0. The ratios below are the time of the library divided by the time of `boost::circular_buffer` at C++17 (above 1.00 is slower than Boost).

API and guarantees (the Boost column is from Boost 1.92.0, probed by compiling the member):

| | this library | `boost::circular_buffer` | `std::deque` as a bounded queue | `std::vector` ring by hand |
| --- | --- | --- | --- | --- |
| Standard floor | C++11 | C++03 (the header emulates rvalue references) | C++98 | C++98 |
| Dependency | none (header-only, `lumex::utility`) | Boost headers | the standard library | the standard library |
| Full buffer | `push_back` overwrites the oldest, `push_front` the newest | the same | you pop first | you write it |
| Capacity | fixed at construction, at least 1 | `set_capacity`, `rset_capacity`, may be 0, default constructible | unbounded | fixed |
| Overwrite of a non-trivial element | destroy, then construct (allocates again for `std::string` of 80 characters) | assigns into the old element | pop and push | assign |
| `emplace_back`, `emplace_front` | yes | no | yes | no |
| Insert and erase in the middle, `rotate` | no | yes (`insert`, `rinsert`, `erase`, `rerase`, `rotate`) | yes (`insert`, `erase`) | you write it |
| Contiguous access, `linearize`, `array_one`/`array_two` | no (no `data`) | yes | no | `data` of the vector, with the wrap |
| Checked access | `at`, `front_safe`, `back_safe` | `at` | `at` | `at` of the vector |
| `pop_front` on an empty buffer | no-op | precondition (not empty) | undefined | you write it |
| Allocator | yes (raw pointers only; copy assignment with unequal non-propagating allocators mishandled) | yes | yes | yes |
| Thread safety | none | none | none | none |
| `sizeof` of the container | 40 | 40 | 88 | 48 |
| Heap for capacity 1 024 `int` (empty / 10 elements / full) | 4 096 B in 1 call, constant | the same | 576 B in 2 calls / 576 B / 4 816 B in 11 calls | 4 096 B in 1 call, constant |
| Needs a default constructible `T` | no | no | no | yes (in the benchmark's ring) |
| Move constructor | `noexcept`, leaves capacity 0 | `noexcept` | not `noexcept` in libstdc++ (allocates) | `noexcept` |

Run-time results from the benchmark (29 scenarios; times are ns per operation, the unit of a pass is a pass over the elements, `int` unless noted; `size` is the capacity, and the copy scenarios hold 3/4 of it; a ratio in parentheses is to Boost):

| Scenario | size | `boost` | this library | `vector` ring | `vector` ring with `%` | `deque` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `push_overwrite_int` | 1024 | 2.16 | 5.67 (2.63) | 1.00 (0.46) | 5.75 (2.66) | 1.61 (0.74) |
| `push_overwrite_str_short` | 1024 | 3.42 | 8.98 (2.62) | 3.69 (1.08) | 5.86 (1.71) | 5.36 (1.57) |
| `push_overwrite_str_long` | 1024 | 6.21 | 13.34 (2.15) | 5.95 (0.96) | 6.38 (1.03) | 15.17 (2.44) |
| `push_pop_steady_int` | 1024 | 1.19 | 8.96 (7.51) | 1.11 (0.93) | 5.66 (4.74) | 1.86 (1.56) |
| `push_pop_steady_str_long` | 1024 | 13.63 | 13.96 (1.02) | 6.80 (0.50) | 7.28 (0.53) | 15.03 (1.10) |
| `fill_int` | 1024 | 690.5 | 2415 (3.50) | 699.9 (1.01) | 2415 (3.50) | 1598 (2.31) |
| `fill_drain_int` | 1024 | 1212 | 9142 (7.54) | 1286 (1.06) | 6719 (5.55) | 2561 (2.11) |
| `front_back_int` | 1024 | 0.45 | 4.71 (10.41) | 0.64 (1.42) | 2.36 (5.20) | 0.61 (1.34) |
| `index_sum_int` | 1024 | 280.0 | 1809 (6.46) | 260.8 (0.93) | 1809 (6.46) | 547.8 (1.96) |
| `iterate_sum_int` | 1024 | 208.1 | 1807 (8.68) | 262.6 (1.26) | 1809 (8.69) | 263.1 (1.26) |
| `producer_consumer_int` | 1024 | 18335 | 37972 (2.07) | 19385 (1.06) | 30922 (1.69) | 25271 (1.38) |
| `producer_consumer_str_short` | 1024 | 38015 | 48673 (1.28) | 38502 (1.01) | 40582 (1.07) | 46945 (1.23) |
| `ctor_dtor_int` | 1024 | 23.36 | 19.48 (0.83) | 43.32 (1.85) | 43.92 (1.88) | 16.92 (0.72) |
| `ctor_dtor_str_short` | 1024 | 20.89 | 20.39 (0.98) | 683.8 (32.73) | 688.8 (32.97) | 19.10 (0.91) |
| `copy_ctor_int` | 1024 | 229.6 | 3638 (15.85) | 49.63 (0.22) | 50.00 (0.22) | 150.4 (0.66) |
| `copy_ctor_str_long` | 128 | 2399 | 2325 (0.97) | 2779 (1.16) | 2840 (1.18) | 1693 (0.71) |
| `copy_assign_int` | 1024 | 237.3 | 3636 (15.32) | 28.26 (0.12) | 28.45 (0.12) | 93.58 (0.39) |
| `copy_assign_str_long` | 128 | 2305 | 2364 (1.03) | 405.1 (0.18) | 406.2 (0.18) | 404.1 (0.18) |
| `move_pair_int` | 1024 | 3.32 | 3.38 (1.02) | 3.46 (1.04) | 3.49 (1.05) | 24.88 (7.50) |

Summary of the whole set: the geometric mean of the time of this library over Boost is 2.77 at C++17 (2.82 at C++11 against Boost at C++11); it is slower by more than 10 % in 21 of 29 scenarios and faster in 1 (`ctor_dtor_int`, 0.83x; that row is not a property of the class: run alone in a process both take about 14.7 ns, the gap in the full run comes from the heap state left by earlier scenarios, both do one `malloc` and one `free` of 4 096 bytes). The reasons, checked in the disassembly, with the `vector` ring that wraps with `%`, and with an experiment on a copy of the header:

- **One `div` per index.** `index_sum_int`, `iterate_sum_int` and `fill_int` take the same time with this library and with the `vector` ring that wraps with `%` (1 809, 1 807 and 2 415 ns against 1 809, 1 809 and 2 415) and are 3.5x to 8.7x slower than Boost; the `vector` ring with a compare is as fast as Boost. The loops contain one `div` per element (two in `front_back`, in the full-buffer `push_back` and per element in the copy), about 2.4 ns each on this CPU. For `std::string` the element work hides part of it (`index_sum_str_short` 3.12x). It is the cost of keeping the capacity a run-time value and the head as an index; a division-free wrap would remove it without changing the interface (reported, not changed here).
- **The experiment.** A copy of `CircularBuffer.hpp` in which only the three index computations (`physical_index`, `next_index`, `prev_index`) are changed from `% m_capacity` to a compare and a subtraction (the indexes are below twice the capacity), generated by `benchmarks/circular_buffer/CMakeLists.txt` into the build tree and built as the variant `lumex compare-wrap`, gives these ratios to Boost (lumex as it is in parentheses): `index_sum_int` 0.93 (6.46), `fill_int` 0.91 (3.50), `push_overwrite_int` 0.54 (2.63), `push_pop_steady_int` 1.15 (7.51), `fill_drain_int` 1.15 (7.54), `iterate_sum_int` 1.26 (8.68), `front_back_int` 1.82 (10.41), `copy_ctor_int` 2.43 (15.85; the rest is the element-by-element copy), `copy_assign_int` 2.36 (15.32). The geometric mean over the 29 scenarios is 1.18 (14 scenarios slower than Boost by more than 10 %, 2 faster) against 2.77. It was measured in a separate 5-pass run after the others. This is a measurement on a header copy, not a change of the library. Hardware counters (read with `perf_event_open`) show the same: ARITH.DIV_ACTIVE about 99.7 % of the cycles in `index_sum_int`, `front_back_int` and `copy_ctor_int` for this library and for the `vector %` ring, 0.002 % for Boost.
- **Copying.** Copy construction appends element by element through `emplace_back` (two `div` per element) where Boost and `std::vector` copy in bulk: 15.85x for 768 `int`; copy assignment builds a new block and swaps (15.32x). For 96 strings of 80 characters (3/4 of a capacity of 128) copying is at par with Boost (0.97x and 1.03x).
- **Overwriting a string allocates.** The oldest element is destroyed and a new one constructed: 1 000 allocations for 1 000 overwriting pushes of an 80-character string, none for Boost, which assigns (13.34 ns against 6.21 ns, 2.15x).
- **Where it is not slower:** moving (3.38 ns against 3.32 ns for a move construction plus a move assignment back), construction and destruction of an empty buffer (equal in a process of its own, see above), and operations that are dominated by allocation (`push_pop_steady_str_long` 1.02x).
- **C++11 against C++17:** the same code at both standards (geometric mean 1.010); the one scenario 1.32x apart runs the same machine code of the scenario function in both builds and is code placement: the callees are identical too, the instruction counts per run are equal, and compiling both with `-falign-functions=64` brings the C++11 build to 13.6 to 14.8 ns against 13.5 to 13.9 ns for C++17.

## Strengths and weaknesses

Strengths:

- Works from C++11 and needs nothing but `lumex::utility`: no Boost.
- `sizeof` is 40 bytes (same as Boost) and the heap use is exactly `capacity * sizeof (T)` in one allocation; the `deque` as a bounded queue uses 88 bytes for the object and grows in blocks.
- `T` need not be default constructible, unlike a `std::vector` ring.
- Move construction and move assignment are `noexcept` and as fast as Boost's (3.4 ns).
- `emplace_back`, `emplace_front` and the checked `front_safe` and `back_safe`, which Boost does not have; `at` and `pop_*` on an empty buffer cannot corrupt anything.
- The documented guarantees of `push_back` and `emplace_back` (strong when not overwriting, basic on overwrite) match the probes.

Weaknesses:

- The index is computed with `%` and a run-time capacity: 2.6x to 10.4x slower than Boost for `int` push, access and iteration, 15x for copying (numbers above, one machine).
- The capacity is fixed and at least 1; no `set_capacity`, `resize`, `insert`, `erase`, `rotate`, `linearize`, `assign`, default constructor or range/`initializer_list` construction (all present in Boost).
- No contiguous access: no `data`, no `array_one`/`array_two`, so the content cannot be handed to a C API or `memcpy` without a copy.
- Overwriting a non-trivial element destroys and reconstructs it instead of assigning, so an overwriting `push_back` of a heap-owning type allocates every time.
- A moved-from buffer is not usable until assigned (capacity 0; `push_back` crashes). A copy constructor that throws leaks.
- `push_back (buf.front ())` on a full buffer is undefined behavior; copy assignment with unequal non-propagating allocators frees memory of the other allocator; `operator<=>` does not compile for `circular_buffer<double>`.
- Iterators name positions, not elements; `iterator == const_iterator` does not compile at any standard and the iterators have no `operator[]`, so neither models `std::random_access_iterator` at C++20.
- No thread safety, no `constexpr`, no fancy-pointer allocators.

## Testing

On GCC 13.2.0 Release (the build used for the numbers) `ctest -R circular_buffer` runs 122 tests and all pass: 58 tests at C++11 and the same 58 at C++20 (the C++20 suite compiles the same two files, so `operator<=>` and the concepts branch are compiled; the module is declared for standards 11 and 20 in `LumexTestStandards.cmake`), 5 examples (`examples.circular_buffer.*`) and 1 `cmake.*` test (`cmake.require_ok_logger_and_circular_buffer_without_core`). The suite covers construction and the zero capacity, copy and move construction and assignment, `swap` (member, free, `noexcept` for the default allocator), element access and its exceptions, `push_*`, `emplace_*` and `pop_*` including overwrite, `clear`, range-based `for`, const and reverse iterators, comparisons, single-element buffers, empty-buffer operations, the strong and the basic exception guarantees with a throwing type, destruction of every element, independent instances on several threads, and a wall-clock performance test that is Release-only. What is not covered: no differential test against `boost::circular_buffer`, no run under a sanitizer, no mutation check recorded, and none of the known issues below (aliasing push, reuse of a moved-from buffer, allocator propagation, `iterator == const_iterator`, iterator `operator[]`).

## Not done on purpose / limits / known issues

Reported here, not changed.

- **Reuse of a moved-from buffer crashes.** After `circular_buffer b (std::move (a))` the source has capacity 0 and no storage; `push_*` and `emplace_*` on it compute `% 0` (undefined behavior; GCC 13.2 kills the process with SIGFPE, SIGSEGV or SIGILL depending on `T` and `-O`: `push_back` of `int` gives SIGFPE at `-O1` and `-O2`, of `std::string` SIGSEGV and SIGILL; `emplace_front` of `int` SIGSEGV and SIGSEGV, of `std::string` SIGFPE and SIGILL). The moved-from buffer reports `empty ()` and `full ()` both true. Assigning a new buffer to `a` is fine.
- **Pushing an element of the buffer itself into a full buffer reads freed memory.** `buf.push_back (buf.front ())` on a full buffer destroys the source and constructs the copy in the same slot: undefined behavior (AddressSanitizer: heap-use-after-free for a heap-owning type, heap-buffer-overflow for `std::string`). Boost does not have the problem (the same probe printed the right value). `std::vector` handles the same call.
- **A copy constructor that throws leaks.** `circular_buffer (circular_buffer const &)` allocates the storage and copies the elements in the constructor body, so when a `T` copy throws the destructor never runs: the slot block and the elements already copied are not released (probe with a type that throws on its third copy: one allocation and two live elements left behind; the same through `operator=`, which copies through the constructor). The class documentation says "Construct/insert on a non-full buffer: strong guarantee".
- **Copy assignment with unequal, non-propagating allocators.** `a = b` builds a copy with `select_on_container_copy_construction (b's allocator)` and swaps it in without swapping the allocators, so `a` keeps its allocator and frees memory made by the other one (a tagged allocator reported "deallocate by allocator 2 of memory from allocator 1" and the reverse, with `NDEBUG`; `std::vector` copies into its own storage). Without `NDEBUG` the copy assignment aborts in the `swap` assertion. `swap` with such allocators is undefined for the standard containers too; the library asserts. Stateful allocators such as arenas are affected; `std::allocator` is not.
- **`iterator == const_iterator` does not compile at any standard** (C++11, 17, 20 and 23, GCC 13.2 and Clang 23): the comparison operators are members of each iterator class, `const_iterator` is constructible from `iterator` and not the other way round, and at C++20 the reversed candidate is dropped because each iterator declares a matching `operator!=`; `const_iterator == iterator` does compile. The iterators have no `operator[]`, which a random-access iterator requires (`begin ()[2]` does not compile), so neither models `std::random_access_iterator` at C++20 and `std::ranges` algorithms that need random access reject them. `std::sort`, `std::accumulate`, `std::distance` and range construction of a `std::vector` work (probe).
- **`operator<=>` does not compile for `circular_buffer<double>`.** It returns `std::strong_ordering`, so for elements whose `<=>` gives `std::partial_ordering` there is no conversion (GCC 13.2 and Clang 23); the `requires` clause only asks for `std::three_way_comparable`.
- **Documentation slips in `CircularBuffer.hpp`:** `front_safe` is documented as throwing "Undefined" when empty (it throws `std::out_of_range`, as the code and the tests show), and the const `front_safe` refers to `back ()`; the `<=>` documentation says `std::totally_ordered<T>` where the code requires `std::three_way_comparable<T>` and returns `strong_ordering`; the copy-constructor documentation names a trait that does not exist (`propagate_on_container_copy_construction`).
- **No bulk copy and no division-free wrap** (see the comparison); `copy_ctor_int` is 15.85x and the access loops 3x to 10x slower than Boost.
- **Not measured:** compile time, other compilers and standard libraries, other CPUs (the cost of the division is CPU dependent), Windows. MSVC was not tested.
