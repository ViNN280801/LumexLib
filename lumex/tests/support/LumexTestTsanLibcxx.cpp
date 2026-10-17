/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
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

/**
 * @file LumexTestTsanLibcxx.cpp
 * @brief ThreadSanitizer against libc++: the two members of the shared_ptr
 * control block that libc++ keeps in its shared library, compiled with the
 * instrumentation of the test.
 * @details libc++ defines nearly all of `std::shared_ptr` and
 * `std::weak_ptr` inline in its headers, so a program built with
 * `-fsanitize=thread` instruments them. Two members of the control block are
 * not inline; they are compiled into `libc++.so.1`, which is not
 * instrumented:
 *
 * - `__shared_weak_count::__release_weak ()`, the decrement of the weak
 *   count. The thread that drops the last `shared_ptr` of an object destroys
 *   the object and then gives up the weak reference that all strong owners
 *   hold together; the thread that drops the last `weak_ptr` frees the
 *   control block. The acquire-release decrement of the weak count is the
 *   only thing that orders the two.
 * - `__shared_weak_count::lock ()`, the compare-exchange of the strong count
 *   behind `weak_ptr::lock ()`, which orders a successful promotion after
 *   what an earlier owner did before it released its reference.
 *
 * ThreadSanitizer does not see atomic operations it did not instrument, so it
 * misses these happens-before edges: when the last `shared_ptr` and the last
 * `weak_ptr` of an object are dropped in two threads, it reports the free of
 * the control block, or a later reuse of the same memory, as a data race
 * with the last strong release. Two plain threads that reset a `shared_ptr`
 * and a `weak_ptr` of one object at the same time are enough; no code of this
 * library is involved. libstdc++ keeps the whole control block in its
 * headers and is not affected.
 *
 * This file defines the two members again, with the code of `src/memory.cpp`
 * of libc++. They are exported functions of the stable ABI (version 1), so
 * their code is part of that ABI. A definition in the executable takes the
 * place of the one in the shared library, and ThreadSanitizer then sees the
 * synchronization the program really has. Nothing is suppressed: an access
 * that these operations do not order is still reported. The build links the
 * file into the tests and the in-tree examples of a ThreadSanitizer build
 * only; it is empty unless the standard library is libc++ with ABI version 1
 * and the translation unit is compiled with ThreadSanitizer.
 *
 * The bodies of the two members are taken from libc++ (the LLVM project,
 * Apache License 2.0 with LLVM Exceptions; see THIRD-PARTY-NOTICES.md), the
 * rest of the file is under the license above. A program that links libc++
 * statically gets duplicate definitions of the two members from this file
 * and from `libc++.a`: such a build does not use this file.
 */

#include <memory>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__SANITIZE_THREAD__)
#define LUMEX_TEST_TSAN_LIBCXX_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define LUMEX_TEST_TSAN_LIBCXX_SANITIZED 1
#endif
#endif

#if defined(LUMEX_TEST_TSAN_LIBCXX_SANITIZED) && defined(_LIBCPP_VERSION)     \
    && defined(_LIBCPP_ABI_VERSION) && _LIBCPP_ABI_VERSION == 1
// The namespace of libc++ itself: std and its ABI namespace (__1 by default).
// Spelled out: _LIBCPP_BEGIN_NAMESPACE_STD also applies the attributes of the
// inline members of the headers, which a definition of these exported
// members must not carry.
namespace std
{
inline namespace _LIBCPP_ABI_NAMESPACE
{
void
__shared_weak_count::__release_weak () LUMEX_NOEXCEPT
{
  // The last reference: nobody else can be using the block, so the plain
  // acquire load suffices (the same shortcut as in libc++).
  if (__atomic_load_n (&__shared_weak_owners_, __ATOMIC_ACQUIRE) == 0)
    __on_zero_shared_weak ();
  else if (__atomic_add_fetch (&__shared_weak_owners_, -1L, __ATOMIC_ACQ_REL)
           == -1L)
    __on_zero_shared_weak ();
}

__shared_weak_count *
__shared_weak_count::lock () LUMEX_NOEXCEPT
{
  long owners = __atomic_load_n (&__shared_owners_, __ATOMIC_SEQ_CST);
  while (owners != -1L)
    {
      if (__atomic_compare_exchange_n (&__shared_owners_, &owners, owners + 1L,
                                       false, __ATOMIC_SEQ_CST,
                                       __ATOMIC_SEQ_CST))
        return this;
    }
  return nullptr;
}
} // namespace _LIBCPP_ABI_NAMESPACE
} // namespace std
#endif
