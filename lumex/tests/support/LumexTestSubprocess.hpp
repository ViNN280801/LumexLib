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
 * @file LumexTestSubprocess.hpp
 * @brief Runs a callable in a forked child process and reports how it ended.
 * @details A test that must prove a deliberately unsafe fixture (a naive
 * lock-free structure, a missing lock) is caught runs the fixture in a child:
 * a crash, an abort, a sanitizer report or a non-zero exit of the child is
 * the expected result, and the test runner stays alive. Where `fork ()` does
 * not exist (Windows, MinGW) `subprocess_available ()` is false and
 * `run_in_child ()` reports `unavailable`; the tests that need it skip.
 *
 * Preconditions: the calling process has no other running thread when it
 * forks (the child inherits only the calling thread); the body uses only the
 * memory of the child; the child's standard output and error go to
 * /dev/null so a sanitizer report does not flood the log, and an alarm kills
 * a child that hangs.
 */
#ifndef LUMEX_TESTS_SUPPORT_TEST_SUBPROCESS_HPP
#define LUMEX_TESTS_SUPPORT_TEST_SUBPROCESS_HPP

#include <cerrno>
#include <functional>
#include <string>

// LUMEX_TEST_FORCE_NO_FORK builds the branch of platforms without fork () on
// any platform, so the tests of that branch run everywhere.
#if (defined(__unix__) || defined(__APPLE__))                                 \
    && !defined(LUMEX_TEST_FORCE_NO_FORK)
#define LUMEX_TEST_HAS_FORK 1
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#define LUMEX_TEST_HAS_FORK 0
#endif

namespace lumex_test
{
/// How a child ended.
enum class ChildEnd
{
  /// The child returned from its body with exit code 0.
  clean,
  /// The child exited with a non-zero code (a sanitizer exit, a failed check).
  failed_exit,
  /// The child was killed by a signal (a crash, an abort, the alarm).
  signaled,
  /// `fork ()` is not available on this platform or failed.
  unavailable
};

/// The outcome of `run_in_child`.
struct ChildResult
{
  ChildEnd end;
  /// Exit code for `failed_exit`, the signal number for `signaled`.
  int code;
};

/// True where `run_in_child` really forks.
inline bool
subprocess_available ()
{
  return LUMEX_TEST_HAS_FORK != 0;
}

/// True for every ending that is not `clean` and not `unavailable`.
inline bool
child_was_caught (ChildResult const &result)
{
  return result.end == ChildEnd::failed_exit
         || result.end == ChildEnd::signaled;
}

/// Readable form of a result.
inline std::string
describe_child (ChildResult const &result)
{
  switch (result.end)
    {
    case ChildEnd::clean:
      return "exited normally";
    case ChildEnd::failed_exit:
      return "exited with code " + std::to_string (result.code);
    case ChildEnd::signaled:
      return "killed by signal " + std::to_string (result.code);
    case ChildEnd::unavailable:
      return "no subprocess on this platform";
    }
  return "?";
}

/**
 * @brief Runs @p body in a child; the child exits 0 when the body returns
 * @c 0 and with the returned code otherwise.
 * @param body The code to run in the child.
 * @param timeout_seconds The alarm that kills a hung child.
 */
inline ChildResult
run_in_child (std::function<int ()> const &body, int timeout_seconds = 60)
{
  ChildResult result;
  result.end = ChildEnd::unavailable;
  result.code = 0;
#if LUMEX_TEST_HAS_FORK
  pid_t const pid = fork ();
  if (pid < 0)
    return result;
  if (pid == 0)
    {
      int const null_fd = open ("/dev/null", O_WRONLY);
      if (null_fd >= 0)
        {
          dup2 (null_fd, 1);
          dup2 (null_fd, 2);
        }
      alarm (static_cast<unsigned> (timeout_seconds));
      int code = 1;
      try
        {
          code = body ();
        }
      catch (...)
        {
          code = 99;
        }
      _exit (code);
    }
  int status = 0;
  pid_t waited;
  do
    {
      waited = waitpid (pid, &status, 0);
    }
  while (waited < 0 && errno == EINTR);
  if (waited < 0)
    return result;
  if (WIFSIGNALED (status))
    {
      result.end = ChildEnd::signaled;
      result.code = WTERMSIG (status);
    }
  else if (WIFEXITED (status) && WEXITSTATUS (status) == 0)
    {
      result.end = ChildEnd::clean;
    }
  else
    {
      result.end = ChildEnd::failed_exit;
      result.code = WIFEXITED (status) ? WEXITSTATUS (status) : -1;
    }
#else
  (void)body;
  (void)timeout_seconds;
#endif
  return result;
}
} // namespace lumex_test

#endif // !LUMEX_TESTS_SUPPORT_TEST_SUBPROCESS_HPP
