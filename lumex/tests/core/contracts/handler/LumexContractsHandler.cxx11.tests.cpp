// The violation handler: install, get, reset, nested scopes, the default
// handler and its output, a handler that throws, a handler that violates,
// threads.

#include <atomic>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;
using contracts::contract_violation;
using contracts::get_violation_handler;
using contracts::invoke_default_violation_handler;
using contracts::invoke_violation_handler;
using contracts::scoped_violation_handler;
using contracts::set_violation_handler;

contract_violation
sample (char const *comment = "x > 0",
        contracts::evaluation_semantic semantic
        = contracts::evaluation_semantic::observe)
{
  return contract_violation (
      comment, contracts::assertion_kind::assert, semantic,
      contracts::detection_mode::predicate_false,
      contracts::source_location ("sample.cpp", "void sample ()", 77, 5));
}

class ContractsHandler : public fixture
{
};

void
first_handler (contract_violation const &)
{
  record ().calls += 100;
}

void
second_handler (contract_violation const &)
{
  record ().calls += 1000;
}

TEST_F (ContractsHandler, StartsWithTheDefaultHandler)
{
  EXPECT_EQ (get_violation_handler (), nullptr);
}

TEST_F (ContractsHandler, SetReturnsThePreviousHandler)
{
  EXPECT_EQ (set_violation_handler (&first_handler), nullptr);
  EXPECT_EQ (get_violation_handler (), &first_handler);
  EXPECT_EQ (set_violation_handler (&second_handler), &first_handler);
  EXPECT_EQ (get_violation_handler (), &second_handler);
  EXPECT_EQ (set_violation_handler (nullptr), &second_handler);
  EXPECT_EQ (get_violation_handler (), nullptr);
}

TEST_F (ContractsHandler, InvokeCallsTheInstalledHandlerOnly)
{
  set_violation_handler (&first_handler);
  invoke_violation_handler (sample ());
  EXPECT_EQ (record ().calls, 100);
  set_violation_handler (&second_handler);
  invoke_violation_handler (sample ());
  EXPECT_EQ (record ().calls, 1100);
}

TEST_F (ContractsHandler, HandlerSeesTheViolation)
{
  set_violation_handler (&recording_handler);
  invoke_violation_handler (
      sample ("a == b", contracts::evaluation_semantic::enforce));
  EXPECT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().comment, "a == b");
  EXPECT_EQ (record ().kind, contracts::assertion_kind::assert);
  EXPECT_EQ (record ().semantic, contracts::evaluation_semantic::enforce);
  EXPECT_TRUE (record ().terminating);
  EXPECT_EQ (record ().mode, contracts::detection_mode::predicate_false);
  EXPECT_EQ (record ().file, "sample.cpp");
  EXPECT_EQ (record ().function, "void sample ()");
  EXPECT_EQ (record ().line, 77u);
  EXPECT_EQ (record ().column, 5u);
}

TEST_F (ContractsHandler, ScopedHandlerRestoresThePreviousOne)
{
  set_violation_handler (&first_handler);
  {
    scoped_violation_handler const outer (&second_handler);
    EXPECT_EQ (get_violation_handler (), &second_handler);
    {
      scoped_violation_handler const inner (nullptr);
      EXPECT_EQ (get_violation_handler (), nullptr);
    }
    EXPECT_EQ (get_violation_handler (), &second_handler);
  }
  EXPECT_EQ (get_violation_handler (), &first_handler);
}

TEST_F (ContractsHandler, ScopedHandlerRestoresAfterAnException)
{
  set_violation_handler (&first_handler);
  try
    {
      scoped_violation_handler const scope (&throwing_handler);
      invoke_violation_handler (sample ());
      FAIL () << "the handler did not throw";
    }
  catch (violation_error const &error)
    {
      EXPECT_EQ (error.comment, "x > 0");
    }
  EXPECT_EQ (get_violation_handler (), &first_handler);
}

TEST_F (ContractsHandler, AHandlerMayThrow)
{
  set_violation_handler (&throwing_handler);
  EXPECT_THROW (invoke_violation_handler (sample ("y")), violation_error);
  EXPECT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().comment, "y");
}

TEST_F (ContractsHandler, ThrowingHandlerLeavesTheNestingState)
{
  // After a throwing handler the thread is not "inside a handler" any more:
  // the next violation reaches the installed handler again.
  set_violation_handler (&throwing_handler);
  EXPECT_THROW (invoke_violation_handler (sample ()), violation_error);
  EXPECT_THROW (invoke_violation_handler (sample ()), violation_error);
  EXPECT_EQ (record ().calls, 2);
}

void
violating_handler (contract_violation const &violation)
{
  record_t &seen = record ();
  ++seen.calls;
  if (seen.calls == 1)
    // A violation reported from inside a handler goes to the default
    // handler, not to this one again.
    invoke_violation_handler (violation);
}

TEST_F (ContractsHandler, AViolationInsideAHandlerGoesToTheDefaultHandler)
{
  set_violation_handler (&violating_handler);
  testing::internal::CaptureStderr ();
  invoke_violation_handler (sample ("nested"));
  std::string const text = testing::internal::GetCapturedStderr ();
  EXPECT_EQ (record ().calls, 1);
  EXPECT_TRUE (contains (text, "nested")) << text;
  // And the next violation of the thread reaches the handler again (which
  // does not report this time: nothing is printed).
  testing::internal::CaptureStderr ();
  invoke_violation_handler (sample ("again"));
  std::string const second = testing::internal::GetCapturedStderr ();
  EXPECT_EQ (record ().calls, 2);
  EXPECT_TRUE (second.empty ()) << second;
}

TEST_F (ContractsHandler, DefaultHandlerPrintsOneLineAndReturns)
{
  testing::internal::CaptureStderr ();
  invoke_violation_handler (
      sample ("p == q", contracts::evaluation_semantic::observe));
  std::string const text = testing::internal::GetCapturedStderr ();
  EXPECT_EQ (
      text, "sample.cpp:77:5: void sample (): contract violation: p == q "
            "[kind: assert, semantic: observe, detection: predicate_false]\n");
}

TEST_F (ContractsHandler, DefaultHandlerNamesSemanticAndDetection)
{
  contract_violation const violation (
      "f ()", contracts::assertion_kind::post,
      contracts::evaluation_semantic::enforce,
      contracts::detection_mode::evaluation_exception,
      contracts::source_location ("e.cpp", "g", 1, 0));
  testing::internal::CaptureStderr ();
  invoke_default_violation_handler (violation);
  std::string const text = testing::internal::GetCapturedStderr ();
  EXPECT_EQ (text, "e.cpp:1:0: g: contract violation: f () [kind: post, "
                   "semantic: enforce, detection: evaluation_exception]\n");
}

TEST_F (ContractsHandler, DefaultHandlerIgnoresTheInstalledOne)
{
  set_violation_handler (&first_handler);
  testing::internal::CaptureStderr ();
  invoke_default_violation_handler (sample ());
  std::string const text = testing::internal::GetCapturedStderr ();
  EXPECT_EQ (record ().calls, 0);
  EXPECT_FALSE (text.empty ());
}

TEST_F (ContractsHandler, ClearingTheHandlerSelectsTheDefaultOne)
{
  set_violation_handler (&first_handler);
  set_violation_handler (nullptr);
  testing::internal::CaptureStderr ();
  invoke_violation_handler (sample ("cleared"));
  std::string const text = testing::internal::GetCapturedStderr ();
  EXPECT_EQ (record ().calls, 0);
  EXPECT_TRUE (contains (text, "cleared")) << text;
}

TEST_F (ContractsHandler, HandlerTypeIsAPlainFunctionPointer)
{
  EXPECT_TRUE ((std::is_same<contracts::violation_handler_type,
                             void (*) (contract_violation const &)>::value));
  EXPECT_FALSE (std::is_copy_constructible<scoped_violation_handler>::value);
  EXPECT_FALSE (std::is_copy_assignable<scoped_violation_handler>::value);
}

std::atomic<int> g_first_calls (0);
std::atomic<int> g_second_calls (0);

void
counting_first (contract_violation const &)
{
  g_first_calls.fetch_add (1);
}

void
counting_second (contract_violation const &)
{
  g_second_calls.fetch_add (1);
}

TEST_F (ContractsHandler, ThreadsInstallAndReportAtOnce)
{
  g_first_calls = 0;
  g_second_calls = 0;
  int const reporters = 4;
  int const per_thread = 20000;
  std::atomic<bool> go (false);
  std::atomic<bool> stop (false);
  set_violation_handler (&counting_first);

  std::thread installer (
      [&]
        {
          while (!go.load ())
            std::this_thread::yield ();
          while (!stop.load ())
            {
              set_violation_handler (&counting_first);
              set_violation_handler (&counting_second);
              // The value read back is one of the two, never anything else.
              contracts::violation_handler_type const seen
                  = get_violation_handler ();
              if (seen != &counting_first && seen != &counting_second)
                ADD_FAILURE () << "torn handler";
            }
        });
  std::vector<std::thread> threads;
  for (int index = 0; index < reporters; ++index)
    threads.emplace_back (
        [&]
          {
            while (!go.load ())
              std::this_thread::yield ();
            for (int count = 0; count < per_thread; ++count)
              invoke_violation_handler (sample ());
          });
  go = true;
  for (std::thread &thread : threads)
    thread.join ();
  stop = true;
  installer.join ();
  // Every report reached exactly one of the two handlers.
  EXPECT_EQ (g_first_calls.load () + g_second_calls.load (),
             reporters * per_thread);
  EXPECT_GT (g_first_calls.load (), 0);
  EXPECT_GT (g_second_calls.load (), 0);
}

TEST_F (ContractsHandler, EveryThreadHasItsOwnNestingState)
{
  // A thread inside a handler does not send the violations of other threads
  // to the default handler.
  std::atomic<int> inside (0);
  std::atomic<bool> release (false);
  static std::atomic<int> *s_inside = nullptr;
  static std::atomic<bool> *s_release = nullptr;
  s_inside = &inside;
  s_release = &release;
  set_violation_handler (+[] (contract_violation const &violation)
                           {
                             if (std::string (violation.comment ()) == "block")
                               {
                                 s_inside->store (1);
                                 while (!s_release->load ())
                                   std::this_thread::yield ();
                               }
                             else
                               record ().calls += 1;
                           });
  std::thread blocker ([] { invoke_violation_handler (sample ("block")); });
  while (inside.load () == 0)
    std::this_thread::yield ();
  invoke_violation_handler (sample ("other"));
  EXPECT_EQ (record ().calls, 1);
  release = true;
  blocker.join ();
}
} // namespace
