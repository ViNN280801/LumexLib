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

// The two races of the standard smart pointers that ThreadSanitizer misreads
// against an uninstrumented libc++ (LumexTestTsanLibcxx.cpp). Both programs
// are race free. In every build they check the counts; in a ThreadSanitizer
// build with libc++ they are the regression test of that file: without it
// (or with a copy that orders less) ThreadSanitizer reports a data race and
// the process ends with its exit code.

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <thread>

#include <gtest/gtest.h>

namespace
{
// Rounds of each race; every round starts a fresh pair of threads.
int const kRounds = 300;

/// Counts its destructions.
struct counted_t
{
  explicit counted_t (std::atomic<int> &deaths) : deaths_ (&deaths), value (0)
  {
  }

  ~counted_t () { deaths_->fetch_add (1, std::memory_order_relaxed); }

  counted_t (counted_t const &) = delete;
  counted_t &operator= (counted_t const &) = delete;

  std::atomic<int> *deaths_;
  int value;
};

/// Allocator that counts the blocks it frees.
template <typename T> class counting_allocator_t
{
public:
  typedef T value_type;

  explicit counting_allocator_t (std::atomic<int> &frees) : frees_ (&frees) {}

  template <typename U>
  counting_allocator_t (counting_allocator_t<U> const &other)
      : frees_ (other.frees ())
  {
  }

  T *
  allocate (std::size_t n)
  {
    return static_cast<T *> (::operator new (n * sizeof (T)));
  }

  void
  deallocate (T *address, std::size_t)
  {
    ::operator delete (address);
    frees_->fetch_add (1, std::memory_order_relaxed);
  }

  std::atomic<int> *
  frees () const
  {
    return frees_;
  }

  /// Equal when they count into the same counter.
  template <typename U>
  bool
  operator== (counting_allocator_t<U> const &other) const
  {
    return frees_ == other.frees ();
  }

  template <typename U>
  bool
  operator!= (counting_allocator_t<U> const &other) const
  {
    return frees_ != other.frees ();
  }

private:
  std::atomic<int> *frees_;
};

/// Lets two threads pass together, so that what follows overlaps.
void
arrive (std::atomic<int> &ready)
{
  ready.fetch_add (1);
  while (ready.load () < 2)
    std::this_thread::yield ();
}
} // namespace

// The last shared_ptr goes in one thread (it destroys the object), the last
// weak_ptr in another (it frees the block). Only the weak count orders the
// two, through __shared_weak_count::__release_weak ().
TEST (
    LumexTestTsanLibcxxTest,
    GivenTheLastSharedAndTheLastWeakPointer_WhenTwoThreadsDropThemTogether_ThenTheObjectAndItsBlockGoOnce)
{
  std::atomic<int> deaths (0);
  std::atomic<int> frees (0);
  for (int round = 0; round < kRounds; ++round)
    {
      std::shared_ptr<counted_t> strong = std::allocate_shared<counted_t> (
          counting_allocator_t<counted_t> (frees), deaths);
      std::weak_ptr<counted_t> weak = strong;
      std::atomic<int> ready (0);
      std::thread owner (
          [&strong, &ready]
            {
              arrive (ready);
              strong.reset ();
            });
      std::thread observer (
          [&weak, &ready]
            {
              arrive (ready);
              weak.reset ();
            });
      owner.join ();
      observer.join ();
    }
  EXPECT_EQ (kRounds, deaths.load ());
  EXPECT_EQ (kRounds, frees.load ());
}

// One owner writes the object and lets go; another thread waits until the
// strong count shows that (a relaxed read, which orders nothing), promotes
// with lock () and reads the object. The compare-exchange of
// __shared_weak_count::lock () reads the owner's release and so orders the
// write before the read.
TEST (
    LumexTestTsanLibcxxTest,
    GivenAnOwnerThatWritesAndLetsGo_WhenAnotherThreadPromotesAfterThat_ThenItSeesTheWrite)
{
  std::atomic<int> deaths (0);
  int wrong = 0;
  for (int round = 0; round < kRounds; ++round)
    {
      std::shared_ptr<counted_t> const keep
          = std::make_shared<counted_t> (deaths);
      std::weak_ptr<counted_t> const weak = keep;
      std::shared_ptr<counted_t> mine = keep;
      int seen = -1;
      std::thread owner (
          [&mine, round]
            {
              mine->value = round + 1;
              mine.reset ();
            });
      std::thread promoter (
          [&weak, &seen]
            {
              while (weak.use_count () != 1)
                std::this_thread::yield ();
              std::shared_ptr<counted_t> const held = weak.lock ();
              seen = held ? held->value : -2;
            });
      owner.join ();
      promoter.join ();
      if (seen != round + 1)
        ++wrong;
    }
  EXPECT_EQ (0, wrong);
  EXPECT_EQ (kRounds, deaths.load ());
}
