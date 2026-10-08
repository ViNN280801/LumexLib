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
 * @file LumexTestReplay.hpp
 * @brief Prints how to replay a failed concurrent test.
 * @details A `ReplayNote` lives for the body of a test (or one round of it).
 * When the test has failed by the time it is destroyed it writes one line
 * with the base seed, the thread count and the name of the scenario to the
 * standard error, for example
 * `REPLAY Aba.Stress: LUMEX_TEST_SEED=20261008 LUMEX_TEST_THREADS=8`.
 * A message that a failed assertion streams with `replay_text ()` carries the
 * same two variables.
 */
#ifndef LUMEX_TESTS_SUPPORT_TEST_REPLAY_HPP
#define LUMEX_TESTS_SUPPORT_TEST_REPLAY_HPP

#include <cstdint>
#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include "lumex/tests/support/LumexTestConfig.hpp"

namespace lumex_test
{
/// The line a failed scenario prints.
inline std::string
replay_line (char const *scenario, std::uint64_t seed, int threads)
{
  return std::string ("REPLAY ") + scenario
         + ": LUMEX_TEST_SEED=" + std::to_string (seed)
         + " LUMEX_TEST_THREADS=" + std::to_string (threads);
}

class ReplayNote
{
public:
  ReplayNote (char const *scenario, std::uint64_t seed, int threads)
      : scenario_ (scenario), seed_ (seed), threads_ (threads)
  {
  }

  ~ReplayNote ()
  {
    if (::testing::Test::HasFailure ())
      {
        std::fprintf (stderr, "%s\n",
                      replay_line (scenario_, seed_, threads_).c_str ());
        std::fflush (stderr);
      }
  }

  ReplayNote (ReplayNote const &) = delete;
  ReplayNote &operator= (ReplayNote const &) = delete;

private:
  char const *scenario_;
  std::uint64_t seed_;
  int threads_;
};
} // namespace lumex_test

#endif // !LUMEX_TESTS_SUPPORT_TEST_REPLAY_HPP
