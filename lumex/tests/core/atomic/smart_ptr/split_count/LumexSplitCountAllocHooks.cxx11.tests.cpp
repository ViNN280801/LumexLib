/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

// The replacement of the global `operator new` and `operator delete` of the
// split-count test executables. It counts the calls of the calling thread
// (`new_calls`), the blocks not yet freed (`live_allocations`: a leak of a
// holder or of a control block shows without a leak checker) and can make the
// next call of a thread throw `std::bad_alloc` (`fail_next_new`: the holder
// allocation failure of the death test). The whole family is replaced, so
// that a sanitizer never sees a block of one family freed by another: the
// scalar and array forms, the nothrow forms, the sized forms and, from C++17,
// the aligned forms. The control blocks and holders (alignment 8) use the
// plain ones. ThreadSanitizer builds do not replace them (its runtime
// defines the family): `alloc_hooks_active ()` is false there.

#include <cstddef>
#include <cstdlib>
#include <new>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT && !LUMEX_TEST_HAS_TSAN

namespace
{
void *
split_allocate (std::size_t size, bool may_throw)
{
  ++split_test::new_calls ();
  if (split_test::fail_next_new ())
    {
      split_test::fail_next_new () = false;
      if (may_throw)
        throw std::bad_alloc ();
      return nullptr;
    }
  void *const block = std::malloc (size == 0 ? 1 : size);
  if (block == nullptr)
    {
      if (may_throw)
        throw std::bad_alloc ();
      return nullptr;
    }
  split_test::live_allocations ().fetch_add (1, std::memory_order_relaxed);
  return block;
}

void
split_release (void *block) noexcept
{
  if (block == nullptr)
    return;
  split_test::live_allocations ().fetch_sub (1, std::memory_order_relaxed);
  std::free (block);
}

#if defined(__cpp_aligned_new)
void *
split_allocate_aligned (std::size_t size, std::size_t alignment,
                        bool may_throw)
{
  ++split_test::new_calls ();
  if (split_test::fail_next_new ())
    {
      split_test::fail_next_new () = false;
      if (may_throw)
        throw std::bad_alloc ();
      return nullptr;
    }
  void *block = nullptr;
  if (alignment < sizeof (void *))
    alignment = sizeof (void *);
  if (posix_memalign (&block, alignment, size == 0 ? 1 : size) != 0)
    {
      if (may_throw)
        throw std::bad_alloc ();
      return nullptr;
    }
  split_test::live_allocations ().fetch_add (1, std::memory_order_relaxed);
  return block;
}
#endif
} // namespace

void *
operator new (std::size_t size)
{
  return split_allocate (size, true);
}

void *
operator new[] (std::size_t size)
{
  return split_allocate (size, true);
}

void *
operator new (std::size_t size, std::nothrow_t const &) noexcept
{
  return split_allocate (size, false);
}

void *
operator new[] (std::size_t size, std::nothrow_t const &) noexcept
{
  return split_allocate (size, false);
}

void
operator delete (void *block) noexcept
{
  split_release (block);
}

void
operator delete[] (void *block) noexcept
{
  split_release (block);
}

void
operator delete (void *block, std::size_t) noexcept
{
  split_release (block);
}

void
operator delete[] (void *block, std::size_t) noexcept
{
  split_release (block);
}

void
operator delete (void *block, std::nothrow_t const &) noexcept
{
  split_release (block);
}

void
operator delete[] (void *block, std::nothrow_t const &) noexcept
{
  split_release (block);
}

#if defined(__cpp_aligned_new)
void *
operator new (std::size_t size, std::align_val_t alignment)
{
  return split_allocate_aligned (size, static_cast<std::size_t> (alignment),
                                 true);
}

void *
operator new[] (std::size_t size, std::align_val_t alignment)
{
  return split_allocate_aligned (size, static_cast<std::size_t> (alignment),
                                 true);
}

void *
operator new (std::size_t size, std::align_val_t alignment,
              std::nothrow_t const &) noexcept
{
  return split_allocate_aligned (size, static_cast<std::size_t> (alignment),
                                 false);
}

void *
operator new[] (std::size_t size, std::align_val_t alignment,
                std::nothrow_t const &) noexcept
{
  return split_allocate_aligned (size, static_cast<std::size_t> (alignment),
                                 false);
}

void
operator delete (void *block, std::align_val_t) noexcept
{
  split_release (block);
}

void
operator delete[] (void *block, std::align_val_t) noexcept
{
  split_release (block);
}

void
operator delete (void *block, std::size_t, std::align_val_t) noexcept
{
  split_release (block);
}

void
operator delete[] (void *block, std::size_t, std::align_val_t) noexcept
{
  split_release (block);
}

void
operator delete (void *block, std::align_val_t,
                 std::nothrow_t const &) noexcept
{
  split_release (block);
}

void
operator delete[] (void *block, std::align_val_t,
                   std::nothrow_t const &) noexcept
{
  split_release (block);
}
#endif

#endif // LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT && !LUMEX_TEST_HAS_TSAN
