# LumexCircularBuffer: a ring buffer with a run-time capacity that overwrites its oldest element {#lumex_circular_buffer}

`lumex::core::circular_buffer::circular_buffer<T, Allocator>` (target `lumex::circular_buffer`, a header-only interface library that depends on `lumex::utility` for `LUMEX_ASSERT`) is a double-ended sequence of at most `capacity` elements: `push_back` into a full buffer destroys the oldest element and `push_front` into a full buffer destroys the newest. Element 0 is always the oldest, `operator[]` and the iterators are random access over that logical order, and the storage is one allocation of `capacity` slots (made at construction and again by `set_capacity`, `rset_capacity`, `resize`, `assign` and `linearize` when they need a new block). It is written for C++11 and works on every standard from there; from C++20 it adds `operator<=>`, and its iterators model `std::random_access_iterator`. It is modeled on `boost::circular_buffer` and has the main part of its interface: construction from ranges and lists, `assign`, `insert`, `erase`, `resize`, `set_capacity`, `linearize`. It is not thread-safe.

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

Checked access, the zero capacity and the state of a moved-from buffer (`lumex/examples/circular_buffer/example_circular_buffer_checked.cpp`, extract; it prints `capacity 0: size=0 full=yes` first):

```cpp
int
main ()
{
  std::cout << "=== Checked access, capacity and moved-from buffers ===\n\n";

  // A zero capacity is allowed: the buffer is valid, empty and full, and
  // push_back does nothing.
  circular_buffer<int> none (0);
  none.push_back (1);
  std::cout << "capacity 0: size=" << none.size ()
            << " full=" << (none.full () ? "yes" : "no") << '\n';

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

  // A moved-from buffer has capacity 0 and is valid (push does nothing):
  // assign to it to use it again.
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

Construction from a list, `insert`, `erase`, `set_capacity`, `resize`, `rset_capacity`, a range with a capacity, the mixed iterator comparison and `linearize` (`lumex/examples/circular_buffer/example_circular_buffer_editing.cpp`, prints `insert 9 before index 2: capacity=4 values=3,9,4,5` and `sorted, linearized=1 data[0]=5 data[2]=8` among others):

```cpp
int
main ()
{
  std::cout << "=== Editing a ring: construction, insert, erase, capacity "
               "===\n\n";

  // A list gives a full buffer whose capacity is the size of the list.
  circular_buffer<int> ring{ 1, 2, 3, 4 };
  print_buffer ("from a list", ring);
  ring.push_back (5); // full: overwrites the oldest element
  print_buffer ("push_back 5", ring);

  // The result of an insert is the last capacity () elements of the sequence
  // "before the position, the new elements, from the position".
  ring.insert (ring.begin () + 2, 9);
  print_buffer ("insert 9 before index 2", ring);
  ring.erase (ring.begin () + 1, ring.begin () + 3);
  print_buffer ("erase [1, 3)", ring);

  ring.set_capacity (6); // keeps the content; a shrink drops the newest
  ring.resize (4, 8);    // appends copies of 8
  print_buffer ("set_capacity 6, resize 4", ring);
  ring.rset_capacity (3); // a shrink drops the oldest
  print_buffer ("rset_capacity 3", ring);

  // With a capacity, a range longer than the capacity keeps its last part.
  std::vector<int> const more = { 7, 8, 9, 10 };
  circular_buffer<int> tail (2, more.begin (), more.end ());
  print_buffer ("capacity 2 from {7,8,9,10}", tail);

  // The iterators are random access; iterator and const_iterator compare.
  circular_buffer<int>::iterator first = ring.begin ();
  circular_buffer<int>::const_iterator third = ring.cbegin () + 2;
  std::cout << "first[1]=" << first[1] << " first<third=" << (first < third)
            << " third-first=" << (third - first) << '\n';

  // linearize () makes the content one contiguous run for a C API.
  std::sort (ring.begin (), ring.end ());
  int const *const data = ring.linearize ();
  std::cout << "sorted, linearized=" << ring.is_linearized ()
            << " data[0]=" << data[0] << " data[2]=" << data[2] << '\n';
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

The design and the names follow `boost::circular_buffer` (Boost.CircularBuffer, Jan Gaspar): a run-time capacity that may be 0, `push_back` that overwrites the oldest element, `push_front` that overwrites the newest, index 0 is the oldest, `front`, `back`, `capacity`, `full`, `swap`, and the members that change the content or the capacity (`assign`, `insert`, `erase`, `erase_begin`, `erase_end`, `resize`, `set_capacity`, `rset_capacity`, `linearize`, `is_linearized`, `max_size`) with the same meaning, so that a program can move from one to the other (a differential test runs random sequences of these operations on both). Not taken over: `rotate`, `array_one`, `array_two`, `rinsert`, `rerase`, `rresize`, `reserve` (the free room of Boost), `insert (pos)` of a default constructed element and `circular_buffer_space_optimized`. It needs no Boost: the header depends on the standard library and `lumex::utility` only. It adds `emplace_back`, `emplace_front`, checked `front_safe` and `back_safe`, and the `initializer_list` forms of the constructor, `assign` and `insert`, which Boost 1.92.0 does not have.

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
| types | `value_type`, `allocator_type`, `size_type` (`std::size_t`), `difference_type` (`std::ptrdiff_t`), `reference`, `const_reference`, `pointer`, `const_pointer`, `iterator`, `const_iterator` (both are instances of the member template `basic_iterator`), `reverse_iterator`, `const_reverse_iterator` |
| construction | `circular_buffer ()` (capacity 0), `explicit circular_buffer (allocator_type const &)`, `explicit circular_buffer (size_type capacity, allocator_type const & = allocator_type ())` (the capacity may be 0), `circular_buffer (size_type n, value_type const &item, ...)` (a full buffer of `n` copies), `circular_buffer (size_type capacity, size_type n, value_type const &item, ...)`, `circular_buffer (first, last, ...)` (capacity = length of the range), `circular_buffer (capacity, first, last, ...)` (a longer range keeps its last `capacity` elements), `circular_buffer (std::initializer_list, ...)` and `circular_buffer (capacity, std::initializer_list, ...)`; copy (same capacity, same logical content, copied in at most two contiguous pieces); move (`noexcept`, leaves the source with capacity 0) |
| assignment | copy (strong: the copy is made first and swapped in) and move (`noexcept`); `assign (n, item)`, `assign (capacity, n, item)`, `assign (first, last)`, `assign (capacity, first, last)`, `assign (std::initializer_list)` and `assign (capacity, std::initializer_list)` (strong; the capacity becomes the length unless one is given) |
| capacity | `capacity`, `size`, `max_size`, `empty`, `full` (all `noexcept`); `set_capacity` (a shrink drops the newest elements), `rset_capacity` (drops the oldest), `resize (n)` and `resize (n, item)` (a growth appends, a shrink drops the newest; a growth above the capacity raises it) |
| access | `operator[]` (unchecked), `at` (throws `std::out_of_range`), `front`, `back` (unchecked, `LUMEX_ASSERT` when `NDEBUG` is not defined), `front_safe`, `back_safe` (throw `std::out_of_range` on an empty buffer); each in a const and a non-const form. Index 0 is the oldest element |
| adding | `push_back` (copy, move), `push_front` (copy, move), `emplace_back`, `emplace_front` (return a reference to the new element; they throw `std::length_error` on a buffer with capacity 0, where `push_*` do nothing); `insert (pos, item)` (copy, move; returns an iterator to the inserted element), `insert (pos, n, item)`, `insert (pos, first, last)`, `insert (pos, std::initializer_list)`. On a full buffer `push_back` and `emplace_back` replace the oldest element and `push_front` and `emplace_front` the newest; an `insert` leaves the last `capacity ()` elements of the sequence "before `pos`, the new elements, from `pos`" |
| removing | `pop_front`, `pop_back` (no-op on an empty buffer, `noexcept`), `clear` (keeps the capacity), `erase (pos)`, `erase (first, last)` (return an iterator to the element after the removed ones), `erase_begin (n)`, `erase_end (n)` (`n` is limited to `size ()`) |
| iteration | `begin`, `end`, `cbegin`, `cend`, `rbegin`, `rend`, `crbegin`, `crend`; random-access iterators over the logical order, with `operator[]`; `iterator` converts to `const_iterator`, and the two compare, subtract and order against each other in both directions |
| layout | `linearize` (makes the content one contiguous run and returns a pointer to it), `is_linearized` |
| other | `swap` (member and `friend`, O(1)), `get_allocator` |
| comparison | `==`, `!=`, `<`, `<=`, `>`, `>=` (lexicographic over the logical sequence; the capacity is not compared), `operator<=>` from C++20 for every element type that is `std::three_way_comparable`; it returns `std::compare_three_way_result_t<T>` (`std::strong_ordering` for `int` and `std::string`, `std::partial_ordering` for `double`) |

Not there, and present in `boost::circular_buffer`: `rotate`, `array_one`, `array_two`, `rinsert`, `rerase`, `rresize`, `reserve` (the free room, `capacity () - size ()`), `insert (pos)` without a value, and the class `circular_buffer_space_optimized`. There is no `data ()`: `linearize` is the way to a contiguous view of the content.

## How it works

- **Layout.** `sizeof (circular_buffer<T>)` is 40 bytes on x86-64 Linux with GCC 13.2 for every `T` (two empty allocator members with padding take 8 bytes, then the slot pointer, the capacity, the size and the head, 8 bytes each). The class is standard layout and not trivially destructible. The storage is one `Slot` array (`alignas (T) unsigned char[sizeof (T)]`) allocated through the allocator rebound to `Slot`; elements are constructed on demand, so `T` need not be default constructible. The heap block is exactly `capacity * sizeof (T)` in one call (4 096 bytes for 1 024 `int`), allocated at construction whether or not anything is pushed; a buffer with capacity 0 allocates nothing. An iterator is 16 bytes (a pointer to the buffer and a logical index).
- **Indexing.** The physical index of logical index `i` is `head + i`, minus `capacity` when that reaches `capacity`. The head is always below the capacity and `i` is at most the capacity, so one compare and one subtraction wrap (a conditional move in the GCC 13.2 disassembly) and there is no division. `next_index` and `prev_index` are the same compare. `front ()` reads the head slot directly.
- **Copying.** The copy constructor allocates one block and copies the content in at most two contiguous pieces (the part from the head to the end of the storage, then the wrapped part), without an index computation per element. The copy assignment builds that copy first and swaps it in.
- **Iterators.** An iterator is a pointer to the buffer and a logical index; `iterator` and `const_iterator` are the two instances of one member template. They are random access (`operator[]`, `+`, `-`, `+=`, `-=`, the six comparisons, the difference), `iterator` converts to `const_iterator`, and every mix of the two compares, orders and subtracts in both directions, at every standard (at C++20 the rewritten candidates are not ambiguous); iterators of different buffers are unequal. They model `std::random_access_iterator` at C++20 (the C++20 test file asserts it for `iterator`, `const_iterator` and `reverse_iterator`, and the mixed pair as sized sentinels and totally ordered), and `std::ranges::sort`, `std::ranges::find`, `std::ranges::lower_bound` and `std::views::take` accept the buffer. An iterator names a logical position, not an element: after a `push_back` into a full buffer an iterator to `begin ()` dereferences to the new first element, and an `end ()` iterator taken before a `pop_front` no longer equals `end ()`. `pop_front`, `push_front`, an overwriting `push_back`, `insert`, `erase`, `resize` and `clear` invalidate all iterators in that sense; a `push_back` into a non-full buffer keeps them (only `end ()` changes); `pop_back` invalidates the iterators to the last element and `end ()`; `set_capacity`, `rset_capacity`, `assign` and a `linearize` that moves elements invalidate iterators, pointers and references. After `swap` an iterator keeps pointing into the buffer object it was taken from, which now holds the other content; after a move it points into the moved-from object. References and pointers to elements stay valid until that element is destroyed or the storage is replaced; `insert` and `erase` move elements by assignment, so a reference to an element that stays then shows another value.
- **Complexity.** O(1): access, `front`, `back`, `push_*`, `emplace_*`, `pop_*` (also with overwrite), `swap`, `is_linearized`. O(n): copy, `assign`, `clear` for a non-trivially destructible `T`, `set_capacity`, `rset_capacity`, `resize`, `insert`, `erase`, `linearize` (when it moves), iteration, comparison.
- **Exception safety.** `push_back` and `emplace_back` on a non-full buffer: strong. On a full buffer, when `T` is move-constructible, the new element is first built in a temporary on the stack: if that throws, the buffer is unchanged; then the oldest element is destroyed and the new one is moved into its slot, and if that move throws the buffer is consistent and holds one element fewer (probe: capacity 3, a type whose second copy throws, size 2 after the exception). For a `T` that cannot be moved, the oldest element is destroyed first and the new one constructed in place (basic). The copy constructor and the copy assignment leak nothing when a copy throws, and the assignment leaves the target unchanged (strong). `assign`, `set_capacity`, `rset_capacity` and `linearize` are strong (they make the new block first; elements are moved when their move is `noexcept` or the type cannot be copied, and copied otherwise). `insert`, `erase` and `resize` are basic; `insert` and `resize` take back the elements they had constructed when a construction throws. `pop_*`, `clear`, `swap` and the move operations do not throw.
- **Aliasing.** An element of the buffer may be the argument: `push_back (buf.front ())` and `push_front (buf.back ())` on a full buffer, `emplace_back` with arguments that refer to the element being replaced (a pointer into its heap block, for example), `insert (pos, buf.front ())`, `insert (pos, n, buf.front ())`, `resize (n, buf.front ())` and `assign (n, buf.front ())` all read the value before the element is destroyed or moved away. The exception is `emplace_*` on a full buffer for a `T` that is not move-constructible (see above): its arguments must not refer to the replaced element.
- **Capacity 0 and moved-from buffers.** A buffer with capacity 0 (default constructed, `circular_buffer (0)`, or moved from) is valid: it is `empty ()` and `full ()` at the same time, `push_back` and `push_front` do nothing, `emplace_back` and `emplace_front` throw `std::length_error`, `at` and the `_safe` accessors throw `std::out_of_range`, `pop_*`, `clear`, copy, comparison and `swap` work, and `set_capacity`, `assign` or an assignment give it room again. The move constructor and the move assignment take the storage and leave the source with capacity 0, size 0 and no storage.
- **Thread safety.** None. The class has no locks and no atomics; concurrent access from several threads, including one producer and one consumer, needs external synchronization. The tests check only that independent instances do not interfere (`ThreadSafety_MultipleIndependentInstances`).
- **Allocator.** `std::allocator<T>` by default; the allocator is rebound to the slot type and used through `std::allocator_traits`. `get_allocator`, `select_on_container_copy_construction` on copy construction, `propagate_on_container_copy_assignment` on copy assignment and `propagate_on_container_swap` on `swap` are honored: with propagation the copy assignment takes the allocator of the source and releases the old block with the old allocator, without it the own allocator is kept and used for the new block. The move operations always move the allocator along with the storage (the storage and its allocator stay together). `swap` with unequal allocators that do not propagate is undefined, as for the standard containers; a debug build asserts equal allocators. The storage pointer is a plain `Slot *`, so an allocator whose `pointer` is a fancy pointer does not work. `max_size ()` is the smaller of the allocator's `max_size` and the largest `difference_type`; a capacity above it throws `std::length_error` before anything is allocated.
- **`constexpr`.** No member is `constexpr` or usable at compile time (heap allocation, `reinterpret_cast`).
- **Debug checks.** `front ()` and `back ()` assert `!empty ()` when `NDEBUG` is not defined; `erase` asserts that the iterator is not `end ()` and that every iterator argument belongs to this buffer; `operator[]` has no check; `swap` asserts equal allocators when they do not propagate.

## Comparison with existing solutions

Measured with [benchmarks/circular_buffer](../../../benchmarks/circular_buffer/README.md): GCC 13.2.0, libstdc++, Release (`-O2`, `-march=x86-64`), no sanitizers, Intel Core i7-12700K pinned to one core, idle machine under the build lock, 5 passes, Boost 1.92.0 headers. Clang, libc++, MSVC and other CPUs were not measured. The alternatives measured are `boost::circular_buffer` (the library it is modeled on), `std::deque` used as a bounded queue and `std::vector` with manual head and size (a hand-written ring, with the wrap done by a compare and, separately, by `%`, which is how this library computed its index before it was changed to a compare). Not measured: `boost::circular_buffer_space_optimized` (a different allocation policy), lock-free ring queues such as `boost::lockfree::spsc_queue` (they solve a different problem: thread-safe, no overwrite) and the standard library of any compiler other than libstdc++ 13.2.0. The ratios below are the time of the library divided by the time of `boost::circular_buffer` at C++17 (above 1.00 is slower than Boost); the column "before the rework" is the library as it was before the index math, the copy and the memory defects were changed, measured in the earlier run of the same benchmark on the same machine, with the ratio to the Boost of that run.

API and guarantees (the Boost column is from Boost 1.92.0, probed by compiling the member):

| | this library | `boost::circular_buffer` | `std::deque` as a bounded queue | `std::vector` ring by hand |
| --- | --- | --- | --- | --- |
| Standard floor | C++11 | C++03 (the header emulates rvalue references) | C++98 | C++98 |
| Dependency | none (header-only, `lumex::utility`) | Boost headers | the standard library | the standard library |
| Full buffer | `push_back` overwrites the oldest, `push_front` the newest | the same | you pop first | you write it |
| Capacity | run-time value, may be 0, default constructible; `set_capacity`, `rset_capacity`, `resize`, `assign` | the same | unbounded | fixed |
| Overwrite of a non-trivial element | destroy, then construct (allocates again for `std::string` of 80 characters) | assigns into the old element | pop and push | assign |
| `emplace_back`, `emplace_front` | yes | no | yes | no |
| Insert and erase in the middle | `insert`, `erase`, `erase_begin`, `erase_end` (no `rotate`, `rinsert`, `rerase`) | yes (`insert`, `rinsert`, `erase`, `rerase`, `rotate`) | yes (`insert`, `erase`) | you write it |
| Contiguous access | `linearize`, `is_linearized` (no `data`, no `array_one`/`array_two`) | `linearize`, `array_one`, `array_two` | no | `data` of the vector, with the wrap |
| Checked access | `at`, `front_safe`, `back_safe` | `at` | `at` | `at` of the vector |
| `pop_front` on an empty buffer | no-op | precondition (not empty) | undefined | you write it |
| Allocator | yes (raw pointers only; copy assignment follows the propagation traits) | yes | yes | yes |
| Thread safety | none | none | none | none |
| `sizeof` of the container | 40 | 40 | 88 | 48 |
| Heap for capacity 1 024 `int` (empty / 10 elements / full) | 4 096 B in 1 call, constant | the same | 576 B in 2 calls / 576 B / 4 816 B in 11 calls | 4 096 B in 1 call, constant |
| Needs a default constructible `T` | no | no | no | yes (in the benchmark's ring) |
| Move constructor | `noexcept`, leaves capacity 0 | `noexcept` | not `noexcept` in libstdc++ (allocates) | `noexcept` |

Run-time results from the benchmark (29 scenarios; times are ns per operation, the unit of a pass is a pass over the elements, `int` unless noted; `size` is the capacity, and the copy scenarios hold 3/4 of it; a ratio in parentheses is to Boost):

| Scenario | size | `boost` | this library | before the rework | `vector` ring | `vector` ring with `%` | `deque` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `push_overwrite_int` | 1024 | 2.13 | 0.76 (0.36) | 5.67 (2.63) | 1.00 (0.47) | 5.63 (2.65) | 1.62 (0.76) |
| `push_overwrite_str_short` | 1024 | 3.43 | 6.44 (1.88) | 8.98 (2.62) | 3.74 (1.09) | 5.83 (1.70) | 5.36 (1.56) |
| `push_overwrite_str_long` | 1024 | 6.18 | 13.20 (2.13) | 13.34 (2.15) | 6.01 (0.97) | 6.45 (1.04) | 15.63 (2.53) |
| `push_pop_steady_int` | 1024 | 1.20 | 1.23 (1.03) | 8.96 (7.51) | 1.13 (0.94) | 5.56 (4.65) | 1.86 (1.55) |
| `push_pop_steady_str_long` | 1024 | 13.85 | 13.66 (0.99) | 13.96 (1.02) | 6.79 (0.49) | 7.38 (0.53) | 15.12 (1.09) |
| `fill_int` | 1024 | 700.6 | 774.4 (1.11) | 2415 (3.50) | 711.0 (1.01) | 2364 (3.37) | 1617 (2.31) |
| `fill_drain_int` | 1024 | 1211 | 1286 (1.06) | 9142 (7.54) | 1284 (1.06) | 6564 (5.42) | 2521 (2.08) |
| `front_back_int` | 1024 | 0.45 | 0.65 (1.42) | 4.71 (10.41) | 0.65 (1.43) | 2.30 (5.07) | 0.60 (1.32) |
| `index_sum_int` | 1024 | 281.3 | 275.4 (0.98) | 1809 (6.46) | 259.6 (0.92) | 1729 (6.15) | 540.9 (1.92) |
| `iterate_sum_int` | 1024 | 206.6 | 265.9 (1.29) | 1807 (8.68) | 263.4 (1.27) | 1765 (8.54) | 260.7 (1.26) |
| `producer_consumer_int` | 1024 | 18426 | 19225 (1.04) | 37972 (2.07) | 19371 (1.05) | 30346 (1.65) | 25086 (1.36) |
| `producer_consumer_str_short` | 1024 | 38474 | 39400 (1.02) | 48673 (1.28) | 38711 (1.01) | 40845 (1.06) | 47451 (1.23) |
| `ctor_dtor_int` | 1024 | 23.61 | 22.92 (0.97) | 19.48 (0.83) | 43.70 (1.85) | 43.73 (1.85) | 17.43 (0.74) |
| `ctor_dtor_str_short` | 1024 | 21.36 | 21.48 (1.01) | 20.39 (0.98) | 684.3 (32.03) | 677.3 (31.71) | 19.51 (0.91) |
| `copy_ctor_int` | 1024 | 226.8 | 178.6 (0.79) | 3638 (15.85) | 56.13 (0.25) | 57.14 (0.25) | 155.6 (0.69) |
| `copy_ctor_str_long` | 128 | 2401 | 2178 (0.91) | 2325 (0.97) | 2842 (1.18) | 2871 (1.20) | 1713 (0.71) |
| `copy_assign_int` | 1024 | 237.0 | 186.1 (0.79) | 3636 (15.32) | 34.29 (0.14) | 34.71 (0.15) | 92.74 (0.39) |
| `copy_assign_str_long` | 128 | 2309 | 2165 (0.94) | 2364 (1.03) | 407.0 (0.18) | 406.3 (0.18) | 412.3 (0.18) |
| `move_pair_int` | 1024 | 3.32 | 3.30 (0.99) | 3.38 (1.02) | 3.36 (1.01) | 3.35 (1.01) | 25.14 (7.57) |

Summary of the whole set: the geometric mean of the time of this library over Boost is 1.04 at C++17 (1.05 at C++11 against Boost at C++11), against 2.77 (2.82) before the rework; it is slower by more than 10 % in 7 of 29 scenarios (before: 21) and faster in 4 (before: 1). Within 1.3x of Boost in 24 of 29; the five scenarios above 1.3x are `push_overwrite_str_long` (2.13x), `push_overwrite_str_short` (1.88x), `front_back_str_short` (1.54x), `iterate_sum_str_short` (1.48x) and `front_back_int` (1.42x); `iterate_sum_int` is at 1.29x. The reasons, checked with the disassembly and with hardware counters (`perf_event_open`, user-space instructions and cycles of a probe program built with the benchmark's flags, `front () + back ()` and iteration on 768 wrapped `int`, 2 000 000 overwriting pushes of strings):

- **The division is gone.** The three index computations are `head + i` minus the capacity when that reaches it (a compare and a conditional move); the probe loops of this library contain no integer `div`. Before, `index_sum_int` took 1 809 ns for 768 elements (one `div`, about 2.4 ns each); now it takes 275.4 ns, 0.98x of Boost: 9 instructions per element against 12 for Boost's `operator[]`. `push_pop_steady_int` went from 7.51x to 1.03x, `fill_drain_int` from 7.54x to 1.06x, `fill_int` from 3.50x to 1.11x and `push_overwrite_int` from 2.63x to 0.36x (0.76 ns against 2.13 ns).
- **Copying.** The copy constructor copies in at most two contiguous pieces and the copy assignment builds that copy and swaps it in: 768 `int` take 178.6 ns to copy-construct (0.79x of Boost, before 15.85x) and 186.1 ns to copy-assign (0.79x, before 15.32x). The hand-written `vector` ring is still faster (56 and 34 ns: one contiguous copy into storage it already owns); for 96 strings of 80 characters copying is at par with Boost (0.91x and 0.94x).
- **Iteration and `front () + back ()`: the representation.** This library keeps a head and a size and wraps the physical index with a compare and a conditional move on every access; Boost keeps first and last pointers and wraps only when it steps a pointer. The probe counts 15 instructions for `front () + back ()` in this library against 11 in Boost (the loop of this library has the loads of the head, the size, the capacity and the slot pointer and the `mov`, `sub`, `cmp`, `cmovae` of the wrap), 2.8 against 1.8 to 2.3 cycles; 9 instructions per element for iteration against 7, 1.5 to 2.0 against 1.2 cycles. The hand-written `vector` ring with a compare is as slow as this library in the same scenarios (1.43x and 1.27x), so this is the price of the head-and-size representation, not an inefficiency of a particular line. `iterate_sum_int` and `iterate_sum_str_short` move together with it.
- **Overwriting a string allocates, and the temporary costs.** The oldest element is destroyed and the new one constructed: 1 heap call per push for an 80-character string (none for Boost, which assigns), 259 instructions against 83 (2.13x of the time); for a short string 106 instructions against 75 (1.88x). The `vector` ring, which assigns like Boost, is at par with Boost (3.74 ns and 6.01 ns). The new element is built in a temporary before the old one is destroyed so that pushing an element of the buffer itself is safe and a throwing constructor leaves the buffer unchanged; that temporary costs about 28 instructions (106 against 78 for a short string, measured with a header copy that destroys first) and about 12 for a long string, the rest of the gap is the destroy and construct against assign.
- **Where it is not slower:** moving (3.30 ns against 3.32 ns for a move construction plus a move assignment back), construction and destruction of an empty buffer (0.97x), `producer_consumer_*` (1.04x and 1.02x), `index_sum_*` and `push_pop_steady_*`, and the long-string operations dominated by allocation (`push_pop_steady_str_long` 0.99x, `copy_ctor_str_long` 0.91x).
- **C++11 against C++17:** the same code at both standards, geometric mean 1.001, no scenario more than 10 % apart (0.97x to 1.02x).
- **Noise.** In this run four values (all of the `vector` ring: `push_pop_steady_str_short`, `fill_int`, `fill_str_short`, `move_pair_str_short`) carry a dagger, their slowest pass is more than 25 % above the best; none of this library's values does. The ratios in the sections above are best-pass medians; the ratio between two executables that run identical machine code in a scenario would be placement, and none of the ratios quoted as a cause is of that kind (the causes are the instruction counts above, which do not depend on placement).

## Strengths and weaknesses

Strengths:

- Works from C++11 and needs nothing but `lumex::utility`: no Boost.
- Within the cost of Boost on the benchmarked operations (geometric mean 1.04 over 29 scenarios), faster at copying (0.79x for `int`) and in 4 scenarios by more than 10 %.
- The main Boost interface (construction from ranges and lists, `assign`, `insert`, `erase`, `resize`, `set_capacity`, `rset_capacity`, `linearize`) with the same results, checked against Boost by a differential test.
- `sizeof` is 40 bytes (same as Boost) and the heap use is exactly `capacity * sizeof (T)` in one allocation; the `deque` as a bounded queue uses 88 bytes for the object and grows in blocks.
- `T` need not be default constructible, unlike a `std::vector` ring; move-only elements work with every member that does not copy.
- Move construction and move assignment are `noexcept` and as fast as Boost's (3.3 ns); a moved-from buffer and a buffer with capacity 0 are valid.
- Pushing an element of the buffer itself is safe; copy construction and assignment leak nothing and keep the strong guarantee; copy assignment follows the allocator propagation traits.
- Random-access iterators that model `std::random_access_iterator`; `iterator` and `const_iterator` compare with each other; `operator<=>` works for `double` too.
- `emplace_back`, `emplace_front` and the checked `front_safe` and `back_safe`, which Boost does not have; `at` and `pop_*` on an empty buffer cannot corrupt anything.

Weaknesses:

- Iteration and `front ()` + `back ()` take 1.3x to 1.5x of Boost's time (the buffer keeps a head and a size and wraps with a compare on every access, Boost keeps pointers; see the comparison).
- Overwriting a non-trivial element destroys and reconstructs it instead of assigning, so an overwriting `push_back` of a heap-owning type allocates every time (2.1x of Boost for 80-character strings), and the temporary that makes the push safe against aliasing costs about 28 instructions per overwrite of a short string (1.9x of Boost).
- No `rotate`, `array_one`, `array_two`, `rinsert`, `rerase`, `rresize`, `reserve`; no `data ()` (use `linearize`).
- `insert` and `erase` need an assignable `T`; `linearize`, `set_capacity`, `rset_capacity` and `assign` allocate a new block (Boost moves in place for `linearize`).
- Iterators name positions, not elements.
- No thread safety, no `constexpr`, no fancy-pointer allocators.

## Testing

On GCC 13.2.0 Release (the build used for the numbers) `ctest -R circular_buffer` runs 290 tests and all pass: 139 at C++11 and 144 at C++20 (the C++20 suite compiles the same files plus `CircularBufferIterators.cxx20.tests.cpp`, so `operator<=>`, the iterator concepts and the `std::ranges` tests are compiled; the module is declared for standards 11 and 20 in `LumexTestStandards.cmake`), 6 examples (`examples.circular_buffer.*`) and 1 `cmake.*` test (`cmake.require_ok_logger_and_circular_buffer_without_core`). The same 290 pass with Clang 23.1 on libc++ and on libstdc++; GCC 8.3 (C++11 and its `-std=c++2a`) runs 288, three of them skipped because GCC 8 has neither concepts nor ranges nor `<compare>`. The test files, one per concern: `CircularBuffer.cxx11.tests.cpp` and `CircularBufferHeader.cxx11.tests.cpp` (construction, copy and move, `swap`, element access and its exceptions, `push_*`, `emplace_*` and `pop_*` with overwrite, `clear`, iterators, comparisons, destruction of every element, independent instances on several threads, a Release-only timing test), `CircularBufferIndex` (every capacity from 1 to 7 with every head position and size, copies of wrapped buffers, a model-based run against `std::deque`), `CircularBufferDefects` (capacity 0, moved-from buffers, aliasing, throwing copies and leaks, allocator propagation, with a counting allocator that records which allocator released which block), `CircularBufferIterators` (the mixed comparisons, `operator[]`, the arithmetic, the standard algorithms) and its C++20 file, `CircularBufferApi` (every added member, including exception safety with a type whose copy throws, move-only elements, and a model for every capacity, head position, size, insert position and erase range) and `CircularBufferBoost` (random operations applied to this class and to `boost::circular_buffer`, 60 sequences of 600 operations on `int` and 20 of 300 on `std::string`, comparing size, capacity, content and the positions returned by `insert` and `erase` after every step; compiled when `-DLUMEX_TEST_BOOST_INCLUDE_DIR=<dir with boost/circular_buffer.hpp>` is given, otherwise one skipped test stands in for it). The suites are clean under AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer (Debug, with the debug assertions) at C++11, 14, 17 and 20, and pass at C++14 and C++17 on GCC 13.2. A check of the tests by mutation (64 changes to the header: the index arithmetic, the copy order, each memory fix, the allocator propagation, the comparisons, the iterators and each added member) is caught by at least one test in 63 cases; the one that survives removes an early return that the next line makes redundant (an equivalent change). What is not covered: Windows and MSVC, other standard libraries than libstdc++ and libc++, fancy-pointer allocators (not supported).

## Fixed, limits and known issues

Fixed (each has a regression test; the first four were reproduced with AddressSanitizer, LeakSanitizer and UndefinedBehaviorSanitizer before they were fixed):

- Reuse of a moved-from buffer crashed (`% 0`); a buffer with capacity 0 is now valid and `push_*` on it does nothing.
- `push_back (buf.front ())` and the other pushes of an element of the buffer into a full buffer read freed memory; the value is now taken before the element is destroyed.
- A copy constructor or a copy assignment whose `T` copy throws leaked the block and the elements already copied; nothing leaks now and the target of an assignment is unchanged.
- Copy assignment between unequal, non-propagating allocators freed memory with the other allocator; it follows `propagate_on_container_copy_assignment` now.
- `operator<=>` did not compile for `circular_buffer<double>`; it returns `std::compare_three_way_result_t<T>`.
- `iterator == const_iterator` did not compile and the iterators had no `operator[]`; both directions of every comparison compile at every standard and the iterators model `std::random_access_iterator`.
- The documentation slips in `CircularBuffer.hpp` (`front_safe`, `totally_ordered` against `three_way_comparable`, a trait that does not exist) are corrected.
- The run-time division of the index and the per-element copy were replaced (see the comparison). The constructor with capacity 0 used to throw `std::length_error`; it creates a valid empty buffer now, as in Boost.

Limits and known issues:

- **Overwriting a non-trivial element destroys and reconstructs it** where Boost assigns: an overwriting `push_back` of an 80-character string allocates every time (2.1x of Boost), and the temporary that makes the overwrite safe against aliasing costs about 28 instructions for a short string (1.9x). A fast path without the temporary for pushes whose argument is not an element of the buffer would remove the second cost and weaken the guarantee to basic; it is not there.
- **`emplace_back` and `emplace_front` throw `std::length_error` on a buffer with capacity 0** (they return a reference), where `push_*` do nothing.
- **For a `T` that is not move-constructible** the replaced element is destroyed before the new one is built, so the arguments of `emplace_*` must not refer to it and a throwing constructor leaves the buffer one element short.
- **`insert`, `erase` and `resize` are basic**, need an assignable `T` (move or copy), and invalidate every iterator; `assign`, `set_capacity`, `rset_capacity` and `linearize` always allocate a new block (Boost moves in place for `linearize`).
- **`is_linearized ()` of an empty buffer is `true`** (the condition on the addresses holds vacuously); Boost can report `false` for an empty buffer in some internal states.
- **`swap` with unequal allocators that do not propagate** is undefined, as for the standard containers; a debug build asserts. Move assignment always moves the allocator with the storage.
- **Not implemented:** `rotate`, `array_one`, `array_two`, `rinsert`, `rerase`, `rresize`, `reserve`, `insert (pos)` without a value, `circular_buffer_space_optimized`; fancy-pointer allocators; `constexpr`.
- **Not measured:** compile time, other compilers and standard libraries in the benchmark, other CPUs (the cost of a branch-free wrap is CPU dependent), Windows. MSVC was not tested.
